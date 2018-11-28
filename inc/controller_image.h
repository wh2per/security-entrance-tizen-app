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

#ifndef __CONTROLLER_IMAGE_H__
#define __CONTROLLER_IMAGE_H__

#include "security-entrance-tizen-app.h"

void controller_image_initialize(void);
void mv_image_init(struct app_data_s *user_data);
void mv_image_destroy(void);
int mv_image_detect(void);
void controller_image_finalize(void);
int controller_image_save_image_file(const char *path,
	unsigned int width, unsigned int height, const unsigned char *buffer,
	const char *comment, unsigned int comment_len);
int controller_image_read_image_file(const char *path,
	unsigned int *width, unsigned int *height, unsigned char *buffer, unsigned long long *size);
#endif
