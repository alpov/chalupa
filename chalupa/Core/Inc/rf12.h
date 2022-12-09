// RFM12B driver definitions
// 2009-02-09 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#ifndef RF12_h
#define RF12_h

#include <stdint.h>
#include <stdbool.h>

// version 1 did not include the group code in the crc
// version 2 does include the group code in the crc
#define RF12_VERSION    2

#define rf12_grp        rf12_buf[0]
#define rf12_hdr        rf12_buf[1]
#define rf12_len        rf12_buf[2]
#define rf12_data       (rf12_buf + 3)

#define RF12_HDR_CTL    0x80
#define RF12_HDR_DST    0x40
#define RF12_HDR_ACK    0x20
#define RF12_HDR_MASK   0x1F

#define RF12_MAXDATA    66

#define RF12_433MHZ     1
#define RF12_868MHZ     2
#define RF12_915MHZ     3

extern volatile uint16_t rf12_crc;  // running crc value, should be zero at end
extern volatile uint8_t rf12_buf[]; // recv/xmit buf including hdr & crc bytes
extern long rf12_seq;               // seq number of encrypted packet (or -1)

void rf12_isr(void);

// only needed if you want to init the SPI bus before rf12_initialize does it
void rf12_spiInit(void);

// call this once with the node ID, frequency band, and optional group
uint8_t rf12_initialize(void);

// call this frequently, returns true if a packet has been received
uint8_t rf12_recvDone(void);

void rf12_shutdown(void);

bool rf12_ProcessITPlusFrame(uint8_t *SensorId, bool *WeakBatt, int16_t *TempCelsius, uint8_t *Hygro);

#endif
