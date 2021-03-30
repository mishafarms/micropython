/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 "Andreas Valder" <andreas.valder@serioese.gmbh>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef MICROPYTHON_ESP32_SERVO_H
#define MICROPYTHON_ESP32_SERVO_H

// all the include files

#include "freertos/FreeRTOS.h"
#include "esp_attr.h"

#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"

#define MCPWM_CHANNEL_MAX 6 // let's only use unit 0, so 6 is the max

#define MCPWM_UNIT MCPWM_UNIT_0

#define MCPWM_FREQ 50 // this is for standard servos
#define MCPWM_DUTY_CYCLE 0
#define MCPWM_COUNTER_MODE MCPWM_UP_COUNTER
#define MCPWM_DUTY_MODE MCPWM_DUTY_MODE_0

#endif // MICROPYTHON_ESP32_SERVO_H
