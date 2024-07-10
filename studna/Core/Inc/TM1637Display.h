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

#ifndef __TM1637DISPLAY__
#define __TM1637DISPLAY__

#include <stdint.h>

// A number from 0 (lowes brightness) to 7 (highest brightness)
#define TM1637_BRIGHTNESS 2

// TM1637 control
void TM1637_on(void);
void TM1637_off(void);

// Display a decimal number
void TM1637_showNumber(uint16_t num);

#endif // __TM1637DISPLAY__
