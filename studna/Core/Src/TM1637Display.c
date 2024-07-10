// Author: avishorp@gmail.com
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "main.h"
#include "TM1637Display.h"

#define TM1637_I2C_COMM1 0x40
#define TM1637_I2C_COMM2 0xC0
#define TM1637_I2C_COMM3 0x80

//
//      A
//     ---
//  F |   | B
//     -G-
//  E |   | C
//     ---
//      D
static const uint8_t digitToSegment[] = {
    // XGFEDCBA
    0b00111111, // 0
    0b00000110, // 1
    0b01011011, // 2
    0b01001111, // 3
    0b01100110, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b00000111, // 7
    0b01111111, // 8
    0b01101111, // 9
    0b01110111, // A
    0b01111100, // b
    0b00111001, // C
    0b01011110, // d
    0b01111001, // E
    0b01110001  // F
};

static void bitDelay(void)
{
    // ~100us
	for (volatile uint32_t i = 0; i < 100; i++) {
    	;
    }
}

static void start(void)
{
	LL_GPIO_SetPinMode(DISP_DIO_GPIO_Port, DISP_DIO_Pin, LL_GPIO_MODE_OUTPUT);
    bitDelay();
}

static void stop(void)
{
	LL_GPIO_SetPinMode(DISP_DIO_GPIO_Port, DISP_DIO_Pin, LL_GPIO_MODE_OUTPUT);
    bitDelay();
    LL_GPIO_SetPinMode(DISP_CLK_GPIO_Port, DISP_CLK_Pin, LL_GPIO_MODE_INPUT);
    bitDelay();
    LL_GPIO_SetPinMode(DISP_DIO_GPIO_Port, DISP_DIO_Pin, LL_GPIO_MODE_INPUT);
    bitDelay();
}

static bool writeByte(uint8_t b)
{
    uint8_t data = b;

    // 8 Data Bits
    for (uint8_t i = 0; i < 8; i++) {
        // CLK low
        LL_GPIO_SetPinMode(DISP_CLK_GPIO_Port, DISP_CLK_Pin, LL_GPIO_MODE_OUTPUT);
        bitDelay();

        // Set data bit
        if (data & 0x01) {
        	LL_GPIO_SetPinMode(DISP_DIO_GPIO_Port, DISP_DIO_Pin, LL_GPIO_MODE_INPUT);
        } else {
        	LL_GPIO_SetPinMode(DISP_DIO_GPIO_Port, DISP_DIO_Pin, LL_GPIO_MODE_OUTPUT);
        }

        bitDelay();

        // CLK high
        LL_GPIO_SetPinMode(DISP_CLK_GPIO_Port, DISP_CLK_Pin, LL_GPIO_MODE_INPUT);
        bitDelay();
        data = data >> 1;
    }

    // Wait for acknowledge
    // CLK to zero
    LL_GPIO_SetPinMode(DISP_CLK_GPIO_Port, DISP_CLK_Pin, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinMode(DISP_DIO_GPIO_Port, DISP_DIO_Pin, LL_GPIO_MODE_INPUT);
    bitDelay();

    // CLK to high
    LL_GPIO_SetPinMode(DISP_CLK_GPIO_Port, DISP_CLK_Pin, LL_GPIO_MODE_INPUT);
    bitDelay();
    uint8_t ack = LL_GPIO_IsInputPinSet(DISP_DIO_GPIO_Port, DISP_DIO_Pin);
    if (ack == 0) {
    	LL_GPIO_SetPinMode(DISP_DIO_GPIO_Port, DISP_DIO_Pin, LL_GPIO_MODE_OUTPUT);
    }

    bitDelay();
    LL_GPIO_SetPinMode(DISP_CLK_GPIO_Port, DISP_CLK_Pin, LL_GPIO_MODE_OUTPUT);
    bitDelay();

    return ack;
}

void TM1637_on(void)
{
    // Set the pin direction and default value.
    // Both pins are set as inputs, allowing the pull-up resistors to pull them up
    LL_GPIO_SetPinMode(DISP_CLK_GPIO_Port, DISP_CLK_Pin, LL_GPIO_MODE_INPUT);
    LL_GPIO_SetPinMode(DISP_DIO_GPIO_Port, DISP_DIO_Pin, LL_GPIO_MODE_INPUT);
    LL_GPIO_ResetOutputPin(DISP_CLK_GPIO_Port, DISP_CLK_Pin);
    LL_GPIO_ResetOutputPin(DISP_DIO_GPIO_Port, DISP_DIO_Pin);

    LL_GPIO_ResetOutputPin(DISP_SHDN_GPIO_Port, DISP_SHDN_Pin);
    TM1637_showNumber(0);
}

void TM1637_off(void)
{
	LL_GPIO_SetOutputPin(DISP_SHDN_GPIO_Port, DISP_SHDN_Pin);
}

void TM1637_showNumber(uint16_t num)
{
    uint8_t digits[4];

	for (int8_t i = 3; i >= 0; --i) {
		uint8_t digit = num % 10;

		if (digit == 0 && num == 0 && i != 3) {
			// Leading zero is blank
			digits[i] = 0;
		} else {
			digits[i] = digitToSegment[digit & 0x0f];
		}

		num /= 10;
	}

    // Write COMM1
    start();
    writeByte(TM1637_I2C_COMM1);
    stop();

    // Write COMM2 + first digit address
    start();
    writeByte(TM1637_I2C_COMM2 + 0x00);
    for (uint8_t k = 0; k < 4; k++) {
        // Write the data bytes
        writeByte(digits[k]);
    }
    stop();

    // Write COMM3 + brightness
    start();
    writeByte(TM1637_I2C_COMM3 + (TM1637_BRIGHTNESS & 0x7) + 0x08);
    stop();
}

