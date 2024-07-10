// RFM12B driver definitions
// 2009-02-09 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#ifndef RF12_h
#define RF12_h

#include <stdint.h>
#include <stdbool.h>

void rf12_isr(void);

// call this once with the node ID, frequency band, and optional group
void rf12_initialize(void);

// call this frequently, returns true if a packet has been received
uint8_t rf12_recvDone(void);

void rf12_sendStart(uint8_t *data, uint8_t length);
bool rf12_sendBusy(void);
bool rf12_waitBusy(void);

void rf12_shutdown(void);

bool rf12_ProcessITPlusFrame(uint8_t *SensorId, bool *WeakBatt, int16_t *TempCelsius, uint8_t *Hygro);
void rf12_SendITPlusFrame(uint8_t SensorId, bool WeakBatt, int16_t TempCelsius, uint8_t Hygro);

#endif
