/*
 * main2.c
 *
 *  Created on: Sep 24, 2021
 *      Author: povalac
 */

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "main.h"
#include "rf12.h"
#include "TM1637Display.h"

extern void SystemClock_Config(void);
extern volatile uint32_t uwTick;


static void lowpower_wait(uint32_t seconds, bool allow_break)
{
    uint32_t timeoutTick = uwTick + (seconds * 1000) / 64;
	LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_64);
	LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE3);
    do {
    	LL_IWDG_ReloadCounter(IWDG);
    	__WFI();
    	if (allow_break && !LL_GPIO_IsInputPinSet(BTN1_GPIO_Port, BTN1_Pin)) {
            break;
        }
    } while (uwTick < timeoutTick);
	LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE2);
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
}

void fault_handler(void)
{
	for (uint16_t i = 0; i < 1000; i++) {
		LL_GPIO_SetOutputPin(LED1_GPIO_Port, LED1_Pin);
		LL_mDelay(50);
		LL_GPIO_ResetOutputPin(LED1_GPIO_Port, LED1_Pin);
		LL_mDelay(50);
	}
	NVIC_SystemReset();
}

uint16_t doMeasurement(void)
{
    LL_ADC_StartCalibration(ADC1);
    while (LL_ADC_IsCalibrationOnGoing(ADC1)) {}

	LL_GPIO_SetOutputPin(REF_EN_GPIO_Port, REF_EN_Pin);
	LL_GPIO_ResetOutputPin(DCDC_SHDN_GPIO_Port, DCDC_SHDN_Pin);
	lowpower_wait(1, false);
    LL_ADC_Enable(ADC1);
    while (!LL_ADC_IsActiveFlag_ADRDY(ADC1)) {}
	LL_ADC_REG_StartConversion(ADC1);
	while (!LL_ADC_IsActiveFlag_EOC(ADC1)) {}
	uint16_t adcMeas = LL_ADC_REG_ReadConversionData12(ADC1); // [mA] = [mV] / 5ohm / 20
	while (!LL_ADC_IsActiveFlag_EOC(ADC1)) {}
	uint16_t adcRef = LL_ADC_REG_ReadConversionData12(ADC1); // reference 2.500V = 25mA
    LL_ADC_Disable(ADC1);
	LL_GPIO_ResetOutputPin(REF_EN_GPIO_Port, REF_EN_Pin);
	LL_GPIO_SetOutputPin(DCDC_SHDN_GPIO_Port, DCDC_SHDN_Pin);

	uint32_t current = (25000UL * adcMeas) / adcRef; // in [uA]
	if (current < 4000) current = 4000;              // min. 4mA
	if (current > 20000) current = 20000;            // max. 20mA

	return 300 * (current - 4000) / (20000 - 4000); // in [cm]
}

void main2(void)
{
	unsigned int seed = 11111;
	uint16_t value = 0;
	uint16_t eta = -1;

	LL_SYSTICK_EnableIT();
    rf12_initialize();
    rf12_shutdown();

    while (1) {
    	if (eta > 3600 || !LL_GPIO_IsInputPinSet(BTN1_GPIO_Port, BTN1_Pin)) {
    		value = doMeasurement();
    		eta = 0;

            TM1637_on();
        	TM1637_showNumber(value);
    		lowpower_wait(3, false);
    	    TM1637_off();
    	}

		LL_GPIO_SetOutputPin(LED1_GPIO_Port, LED1_Pin);
		rf12_initialize();
		rf12_SendITPlusFrame(1 /*SensorId*/, false, value, 106);
		rf12_waitBusy();
		rf12_shutdown();
		LL_GPIO_ResetOutputPin(LED1_GPIO_Port, LED1_Pin);

		uint8_t delay = (rand_r(&seed) & 0x04) + 4; // random 4-7 seconds
		lowpower_wait(delay, true);
    	eta += delay;
    }
}
