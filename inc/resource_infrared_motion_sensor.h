/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
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

#ifndef __POSITION_FINDER_RESOURCE_INFRARED_MOTION_SENSOR_H__
#define __POSITION_FINDER_RESOURCE_INFRARED_MOTION_SENSOR_H__

/**
 * @brief Reads the value of gpio connected infrared motion sensor(HC-SR501).
 * @param[in] pin_num The number of the gpio pin connected to the infrared motion sensor
 * @param[out] out_value The value of the gpio (zero or non-zero)
 * @return 0 on success, otherwise a negative error value
 * @see If the gpio pin is not open, creates gpio handle before reading the value of gpio.
 */
extern int resource_read_infrared_motion_sensor(int pin_num, uint32_t *out_value);

/**
 * @brief Releases the gpio handle and changes the gpio pin state to the close(0).
 * @param[in] pin_num The number of the gpio pin connected to the infrared motion sensor
 */
extern void resource_close_infrared_motion_sensor(void);

#endif /* __POSITION_FINDER_RESOURCE_INFRARED_MOTION_SENSOR_H__ */
