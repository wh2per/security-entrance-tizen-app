/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * Contact: Jin Yoon <jinny.yoon@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <peripheral_io.h>
#include "log.h"

#define SERVO_MOTOR_CHANNEL (0)
#define SERVO_MOTOR_DEFAULT_PERIOD 20.0

static peripheral_pwm_h g_pwm_h;

void resource_close_servo_motor(void)
{
	if (g_pwm_h) {
		peripheral_pwm_close(g_pwm_h);
		g_pwm_h = NULL;
	}
}

int resource_set_servo_motor_value(double duty_cycle_ms)
{
	int ret = 0;

	if (duty_cycle_ms >= SERVO_MOTOR_DEFAULT_PERIOD) {
		_E("Too large duty cycle");
		return -1;
	}

	if (!g_pwm_h) {
		ret = peripheral_pwm_open(0, SERVO_MOTOR_CHANNEL, &g_pwm_h);
		if (ret != PERIPHERAL_ERROR_NONE) {
			_E("failed to open servo motor with ch : %s", get_error_message(ret));
			return -1;
		}
	}

	ret = peripheral_pwm_set_period(g_pwm_h, SERVO_MOTOR_DEFAULT_PERIOD * 1000 * 1000);
	if (ret != PERIPHERAL_ERROR_NONE) {
		_E("failed to set period : %s", get_error_message(ret));
		return -1;
	}

	ret = peripheral_pwm_set_duty_cycle(g_pwm_h, duty_cycle_ms * 1000 * 1000);
	if (ret != PERIPHERAL_ERROR_NONE) {
		_E("failed to set duty cycle : %s", get_error_message(ret));
		return -1;
	}

	ret = peripheral_pwm_set_enabled(g_pwm_h, true);
	if (ret != PERIPHERAL_ERROR_NONE) {
		_E("failed to enable : %s", get_error_message(ret));
		return -1;
	}

	return 0;
}
