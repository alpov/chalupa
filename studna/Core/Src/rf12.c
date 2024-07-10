// RFM12B driver implementation
// 2009-02-09 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php
// La Crosse extensions

#include <string.h>
#include <stdio.h>
#include "main.h"
#include "rf12.h"

// RF12 command codes
#define RF_RECEIVER_ON  0x82DD
#define RF_XMITTER_ON   0x823D
#define RF_IDLE_MODE    0x820D
#define RF_SLEEP_MODE   0x8205
#define RF_WAKEUP_MODE  0x8207
#define RF_TXREG_WRITE  0xB800
#define RF_RX_FIFO_READ 0xB000
#define RF_WAKEUP_TIMER 0xE000

#define RF_GROUP		0xD4
#define RF_868MHZ       0x02

typedef enum {
    RF_IDLE,
    RF_RX_BUSY,
	RF_RX_DONE,
    RF_TX,
} rf12_state_t;

static volatile uint8_t rf12_buffer[16];
static volatile uint8_t rf12_length;
static volatile uint8_t rf12_position;
static volatile rf12_state_t state;

static uint16_t rf12_xfer(uint16_t cmd)
{
    uint16_t reply = 0;

    LL_GPIO_ResetOutputPin(RFM_SS_GPIO_Port, RFM_SS_Pin);
    // 10ns
    for (uint8_t i = 0; i < 16; i++) {
    	reply <<= 1;
    	if (cmd & 0x8000) LL_GPIO_SetOutputPin(RFM_MOSI_GPIO_Port, RFM_MOSI_Pin);
    	else LL_GPIO_ResetOutputPin(RFM_MOSI_GPIO_Port, RFM_MOSI_Pin);
    	// 5ns
    	LL_GPIO_SetOutputPin(RFM_SCK_GPIO_Port, RFM_SCK_Pin);
    	cmd <<= 1;
    	// 25ns
    	if (LL_GPIO_IsInputPinSet(RFM_MISO_GPIO_Port, RFM_MISO_Pin)) reply |= 0x0001;
    	LL_GPIO_ResetOutputPin(RFM_SCK_GPIO_Port, RFM_SCK_Pin);
    	// 25ns
    }
    // 10ns
    LL_GPIO_SetOutputPin(RFM_SS_GPIO_Port, RFM_SS_Pin);

    return reply;
}

static uint8_t crc8_update(uint8_t crc, uint8_t a)
{
	crc = crc ^ a;

	for (uint8_t i = 0; i < 8; i++) {
		if ((crc & 0x80) != 0)
			crc = (crc << 1) ^ 0x31;
		else
			crc <<= 1;
	}

	return crc;
}

void rf12_isr(void)
{
    rf12_xfer(0x0000);
    if (state == RF_RX_BUSY) {
        rf12_buffer[rf12_length++] = rf12_xfer(RF_RX_FIFO_READ);
		if (rf12_length == 5) {   // IT+ Frames always has 5 bytes
			rf12_xfer(RF_IDLE_MODE);
			state = RF_RX_DONE;
		}
    } else if (state == RF_TX) {
		if (rf12_position <= rf12_length) {
			rf12_xfer(RF_TXREG_WRITE + rf12_buffer[rf12_position++]);
		} else {
			rf12_xfer(RF_IDLE_MODE);
			state = RF_IDLE;
		}
    }
}

uint8_t rf12_recvDone(void)
{
	if (state == RF_RX_DONE && rf12_length == 5) {
        state = RF_IDLE;
		return 1;
	} else if (state == RF_IDLE) {
        rf12_length = 0;
        rf12_xfer(RF_RECEIVER_ON);
        state = RF_RX_BUSY;
    }
    return 0;
}

void rf12_sendStart(uint8_t *data, uint8_t length)
{
	uint8_t crc = 0;
	for (uint8_t i = 0; i < length; i++) {
		crc = crc8_update(crc, data[i]);
	}

	rf12_buffer[0] = 0xAA;
	rf12_buffer[1] = 0xAA;
	rf12_buffer[2] = 0xAA;
	rf12_buffer[3] = 0x2D;
	rf12_buffer[4] = RF_GROUP;
	memcpy((uint8_t *)(&rf12_buffer[5]), data, length);
	rf12_buffer[length+5] = crc;

	rf12_position = 0;
	rf12_length = length+6;
	state = RF_TX;
    rf12_xfer(RF_XMITTER_ON); // bytes will be fed via interrupts
}

bool rf12_sendBusy(void)
{
	return (state == RF_TX);
}

bool rf12_waitBusy(void)
{
	uint16_t timeout = 200; // in milliseconds because of SysTick IRQ

	do {
		__WFI();
		timeout--;
	} while (timeout && rf12_sendBusy());

	return (timeout > 0);
}

void rf12_initialize(void)
{
    /* Enable SPI before start transmission */
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_9);
    LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_9);

    rf12_xfer(0x0000); // initial SPI transfer added to avoid power-up problem
    rf12_xfer(RF_SLEEP_MODE); // DC (disable clk pin), enable lbd

    // wait until RFM12B is out of power-up reset, this takes several *seconds*
    rf12_xfer(RF_TXREG_WRITE); // in case we're still in OOK mode
    for (uint8_t i = 0; i < 8; i++) {
        rf12_xfer(0x0000);
    }

    rf12_xfer(0x80C7 | (RF_868MHZ << 4)); // EL (ena TX), EF (ena RX FIFO), 12.0pF
    rf12_xfer(0xA67C); // IT+: FREQUENCY 868.300MHz
    rf12_xfer(0xC613); // IT+: DATA RATE 17.241 kbps
    rf12_xfer(0x94a0); // IT+: RECEIVER CONTROL VDI Medium 134khz LNA max DRRSI 103 dbm
    rf12_xfer(0xC2AC); // AL,!ml,DIG,DQD4
	rf12_xfer(0xCA83); // FIFO8,2-SYNC,!ff,DR
	rf12_xfer(0xCE00 | RF_GROUP); // SYNC=2DXX；
    rf12_xfer(0xC483); // @PWR,NO RSTRIC,!st,!fi,OE,EN
    rf12_xfer(0x9850); // !mp,90kHz,MAX OUT
    rf12_xfer(0xCC77); // OB1，OB0, LPX,！ddy，DDIT，BW0
    rf12_xfer(0xE000); // NOT USE
    rf12_xfer(0xC800); // NOT USE
    rf12_xfer(0xC049); // 1.66MHz,3.1V

    state = RF_IDLE;
}

void rf12_shutdown(void)
{
    rf12_xfer(RF_SLEEP_MODE); // DC (disable clk pin), enable lbd
    LL_EXTI_DisableIT_0_31(LL_EXTI_LINE_9);
}

bool rf12_ProcessITPlusFrame(uint8_t *SensorId, bool *WeakBatt, int16_t *TempCelsius, uint8_t *Hygro)
{
    if (!rf12_recvDone()) return false;
    if ((rf12_buffer[0] & 0xf0) != 0x90) return false; // not ITPlus

    uint8_t crc = 0;
    for (uint8_t i = 0; i < 5; i++) {
    	crc = crc8_update(crc, rf12_buffer[i]);
    }
    if (crc != 0) return false; // wrong CRC

    if (SensorId) *SensorId = ((rf12_buffer[0] & 0x0f) << 2) + ((rf12_buffer[1] & 0xc0) >> 6);
    // NewBatt = (rf12_buf[1] & 0x20) >> 5;
    if (WeakBatt) *WeakBatt = ((rf12_buffer[3] & 0x80) >> 7) ? true : false;

    if (TempCelsius) *TempCelsius = -400 + \
        100*((rf12_buffer[1] & 0x0f) >> 0) + \
        10 *((rf12_buffer[2] & 0xf0) >> 4) + \
        1  *((rf12_buffer[2] & 0x0f) >> 0);

    if (Hygro) *Hygro = rf12_buffer[3] & 0x7f; /* or 106 if N/A */

    return true;
}

void rf12_SendITPlusFrame(uint8_t SensorId, bool WeakBatt, int16_t TempCelsius, uint8_t Hygro)
{
	uint8_t buf[4];
	uint8_t TempBCD[3];

	TempCelsius += 400;
	if (TempCelsius < 0) TempCelsius = 0;
	if (TempCelsius > 999) TempCelsius = 999;
	TempBCD[2] = TempCelsius % 10; TempCelsius /= 10;
	TempBCD[1] = TempCelsius % 10; TempCelsius /= 10;
	TempBCD[0] = TempCelsius % 10;

	buf[0] = 0x90 | ((SensorId >> 2) & 0x0F);
	buf[1] = ((SensorId << 6) & 0xC0) | (TempBCD[0] & 0x0F);
	buf[2] = ((TempBCD[1] << 4) & 0xF0) | (TempBCD[2] & 0x0F);
	buf[3] = (WeakBatt ? 0x80 : 0x00) | (Hygro & 0x7F);
	rf12_sendStart(buf, sizeof(buf));
}
