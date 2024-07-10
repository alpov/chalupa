// RFM12B driver implementation
// 2009-02-09 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php
// La Crosse extensions

#include <string.h>
#include <stdio.h>
#include "main.h"
//#include <util/crc16.h>
#include "rf12.h"

// maximum transmit / receive buffer: 3 header + data + 2 crc bytes
#define RF_MAX   (RF12_MAXDATA + 5)

// pins used for the RFM12B interface
#define SPI_INSTANCE SPI2
#define SPI_SS(x)   do { \
                        if (x) LL_GPIO_SetOutputPin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin); \
                        else LL_GPIO_ResetOutputPin(SPI2_NSS_GPIO_Port, SPI2_NSS_Pin); \
                    } while (0)

// RF12 command codes
#define RF_RECEIVER_ON  0x82DD
#define RF_XMITTER_ON   0x823D
#define RF_IDLE_MODE    0x820D
#define RF_SLEEP_MODE   0x8205
#define RF_WAKEUP_MODE  0x8207
#define RF_TXREG_WRITE  0xB800
#define RF_RX_FIFO_READ 0xB000
#define RF_WAKEUP_TIMER 0xE000

// RF12 status bits
#define RF_LBD_BIT      0x0400
#define RF_RSSI_BIT     0x0100

// bits in the node id configuration byte
#define NODE_BAND       0xC0        // frequency band
#define NODE_ACKANY     0x20        // ack on broadcast packets if set
#define NODE_ID         0x1F        // id of this node, as A..Z or 1..31

// transceiver states, these determine what to do with each interrupt
enum {
    TXCRC1, TXCRC2, TXTAIL, TXDONE, TXIDLE,
    TXRECV,
    TXPRE1, TXPRE2, TXPRE3, TXSYN1, TXSYN2,
};

static uint8_t nodeid;              // address of this node
static uint8_t group;               // network group
static volatile uint8_t rxfill;     // number of data bytes in rf12_buf
static volatile int8_t rxstate;     // current transceiver state

volatile uint16_t rf12_crc;         // running crc value
volatile uint8_t rf12_buf[RF_MAX];  // recv/xmit buf, including hdr & crc bytes
long rf12_seq;                      // seq number of encrypted packet (or -1)

void (*crypter)(uint8_t);           // does en-/decryption (null if disabled)

static bool ITPlusFrame;


static uint16_t rf12_xfer(uint16_t cmd)
{
    uint16_t reply = 0;

    SPI_SS(0);

    //  HAL_SPI_TransmitReceive(&hspi1, (uint8_t*)(&cmd), (uint8_t*)(&reply), 2, HAL_MAX_DELAY);

    /* Check TXE flag to transmit data */
    do {} while (!LL_SPI_IsActiveFlag_TXE(SPI_INSTANCE));

    /* Transmit 16bit Data */
    LL_SPI_TransmitData16(SPI_INSTANCE, cmd);

    /* Check RXE flag */
    do {} while (!LL_SPI_IsActiveFlag_RXNE(SPI_INSTANCE));

    /* Receive 16bit Data */
    reply = LL_SPI_ReceiveData16(SPI_INSTANCE);

    SPI_SS(1);

    return reply;
}

static uint16_t crc16_update(uint16_t crc, uint8_t a)
{
    crc ^= a;

    for (uint8_t i = 0; i < 8; ++i) {
        if (crc & 1)
            crc = (crc >> 1) ^ 0xA001;
        else
            crc = (crc >> 1);
    }

    return crc;
}

void rf12_isr(void)
{
    // a transfer of 2x 16 bits @ 2 MHz over SPI takes 2x 8 us inside this ISR
    // correction: now takes 2 + 8 µs, since sending can be done at 8 MHz
    rf12_xfer(0x0000);

    if (rxstate == TXRECV) {
        uint8_t in = rf12_xfer(RF_RX_FIFO_READ);

        // See http://forum.jeelabs.net/comment/4434#comment-4434
        // For an explanation , and limitation of this modification , to read the La Crosse TX29+ 868Mhz Sensors

        // Check what type of frame it looks like with the first received byte
        // Is it a TX29+ Frame ? (Always starts with 0x9? , on group 0xD4)
        if (rxfill == 0 && group == 0xd4) {
            if ((in & 0xf0) == 0x90)
                ITPlusFrame = true;
            else
                ITPlusFrame = false;
        }

        if (rxfill == 0 && group != 0 && !ITPlusFrame) /* GCR : added  && !ITPlusFrame */
            rf12_buf[rxfill++] = group;

        rf12_buf[rxfill++] = in;

        if (ITPlusFrame) {
            if (rxfill == 5)    // IT+ Frames always has 5 bytes
                rf12_xfer(RF_IDLE_MODE);
                // CRC will be computed later
        } else {
            rf12_crc = crc16_update(rf12_crc, in);

            if (rxfill >= rf12_len + 5 || rxfill >= RF_MAX)
                rf12_xfer(RF_IDLE_MODE);
        }
    } else {
        uint8_t out;

        if (rxstate < 0) {
            uint8_t pos = 3 + rf12_len + rxstate++;
            out = rf12_buf[pos];
            rf12_crc = crc16_update(rf12_crc, out);
        } else
            switch (rxstate++) {
                case TXSYN1: out = 0x2D; break;
                case TXSYN2: out = group; rxstate = - (2 + rf12_len); break;
                case TXCRC1: out = rf12_crc; break;
                case TXCRC2: out = rf12_crc >> 8; break;
                case TXDONE: rf12_xfer(RF_IDLE_MODE); // fall through
                default:     out = 0xAA;
            }

        rf12_xfer(RF_TXREG_WRITE + out);
    }
}

static void rf12_recvStart(void)
{
    rxfill = rf12_len = 0;
    rf12_crc = ~0;
#if RF12_VERSION >= 2
    if (group != 0)
        rf12_crc = crc16_update(~0, group);
#endif
    rxstate = TXRECV;
    rf12_xfer(RF_RECEIVER_ON);
}

uint8_t rf12_recvDone(void)
{
    // Two different receive conditions depending on frame type RFM12/Jeenode or RFM01/IT+
    if (ITPlusFrame) {
        if (rxstate == TXRECV && rxfill == 5) { // IT+ Frames always has 5 bytes
            rxstate = TXIDLE;
            return 1;   // Got full IT+ frame
        }
    } else {    // RFM12/Jeenode normal processing
        if (rxstate == TXRECV && (rxfill >= rf12_len + 5 || rxfill >= RF_MAX)) {
            rxstate = TXIDLE;
            if (rf12_len > RF12_MAXDATA)
                rf12_crc = 1; // force bad crc if packet length is invalid
            if (!(rf12_hdr & RF12_HDR_DST) || (nodeid & NODE_ID) == 31 ||
                    (rf12_hdr & RF12_HDR_MASK) == (nodeid & NODE_ID)) {
                if (rf12_crc == 0 && crypter != 0)
                    crypter(0);
                else
                    rf12_seq = -1;
                return 1; // it's a broadcast packet or it's addressed to this node
            }
        }
    }
    if (rxstate == TXIDLE)
        rf12_recvStart();
    return 0;
}

/*!
  Call this once with the node ID (0-31), frequency band (0-3), and
  optional group (0-255 for RF12B, only 212 allowed for RF12).
*/
uint8_t rf12_initialize(void)
{
    nodeid = 1;
    group = 0xd4;

    /* Enable SPI before start transmission */
    LL_SPI_Enable(SPI_INSTANCE);
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_2);
    LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_2);

    rf12_xfer(0x0000); // intitial SPI transfer added to avoid power-up problem

    rf12_xfer(RF_SLEEP_MODE); // DC (disable clk pin), enable lbd

    // wait until RFM12B is out of power-up reset, this takes several *seconds*
    rf12_xfer(RF_TXREG_WRITE); // in case we're still in OOK mode
    for (uint8_t i = 0; i < 8; i++)
        rf12_xfer(0x0000);

    rf12_xfer(0x80C7 | (RF12_868MHZ << 4)); // EL (ena TX), EF (ena RX FIFO), 12.0pF
    rf12_xfer(0xA640); // 868MHz
    rf12_xfer(0xC606); // approx 49.2 Kbps, i.e. 10000/29/(1+6) Kbps
    rf12_xfer(0x94A2); // VDI,FAST,134kHz,0dBm,-91dBm
    rf12_xfer(0xC2AC); // AL,!ml,DIG,DQD4
    if (group != 0) {
        rf12_xfer(0xCA83); // FIFO8,2-SYNC,!ff,DR
        rf12_xfer(0xCE00 | group); // SYNC=2DXX；
    } else {
        rf12_xfer(0xCA8B); // FIFO8,1-SYNC,!ff,DR
        rf12_xfer(0xCE2D); // SYNC=2D；
    }
    rf12_xfer(0xC483); // @PWR,NO RSTRIC,!st,!fi,OE,EN
    rf12_xfer(0x9850); // !mp,90kHz,MAX OUT
    rf12_xfer(0xCC77); // OB1，OB0, LPX,！ddy，DDIT，BW0
    rf12_xfer(0xE000); // NOT USE
    rf12_xfer(0xC800); // NOT USE
    rf12_xfer(0xC049); // 1.66MHz,3.1V

    /* RF12 overide settings for IT+ */
    rf12_xfer(0xA67c); // FREQUENCY 868.300MHz
    rf12_xfer(0xC613); // DATA RATE 17.241 kbps
    rf12_xfer(0x94a0); // RECEIVER CONTROL VDI Medium 134khz LNA max DRRSI 103 dbm

    rxstate = TXIDLE;

    return nodeid;
}

void rf12_shutdown(void)
{
    rf12_xfer(RF_SLEEP_MODE); // DC (disable clk pin), enable lbd
    LL_EXTI_DisableIT_0_31(LL_EXTI_LINE_2);
    LL_SPI_Disable(SPI_INSTANCE);
}

static bool rf12_CheckITPlusCRC(uint8_t *msge, uint8_t nbBytes)
{
    uint8_t reg = 0;
    uint8_t curByte, curbit, bitmask;
    uint8_t do_xor;

    while (nbBytes-- != 0) {
        curByte = *msge++; bitmask = 0b10000000;
        while (bitmask != 0) {
            curbit = ((curByte & bitmask) == 0) ? 0 : 1;
            bitmask >>= 1;
            do_xor = (reg & 0x80);

            reg <<=1;
            reg |= curbit;

            if (do_xor) reg ^= 0x31;
        }
    }
    return (reg == 0);
}

bool rf12_ProcessITPlusFrame(uint8_t *SensorId, bool *WeakBatt, int16_t *TempCelsius, uint8_t *Hygro)
{
    if (!rf12_recvDone()) return false;
    if (!ITPlusFrame || !rf12_CheckITPlusCRC((uint8_t *)(rf12_buf), 5)) return false; // not ITPlus or wrong CRC

    if (SensorId) *SensorId = ((rf12_buf[0] & 0x0f) << 2) + ((rf12_buf[1] & 0xc0) >> 6);
    // NewBatt = (rf12_buf[1] & 0x20) >> 5;
    if (WeakBatt) *WeakBatt = ((rf12_buf[3] & 0x80) >> 7) ? true : false;

    if (TempCelsius) *TempCelsius = -400 + \
        100*((rf12_buf[1] & 0x0f) >> 0) + \
        10 *((rf12_buf[2] & 0xf0) >> 4) + \
        1  *((rf12_buf[2] & 0x0f) >> 0);

    if (Hygro) *Hygro = rf12_buf[3] & 0x7f; /* or 106 if N/A */

    return true;
}
