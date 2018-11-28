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

#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <tizen.h>
#include <image_util.h>
#include "log.h"
#include "exif.h"
#include "security-entrance-tizen-app.h"
#include <glib.h>
#include <app_common.h>

#include <mv_image.h>

/* Image decoding for image recognition */
#include <image_util.h>

static image_util_encode_h encode_h = NULL;
static image_util_decode_h decode_h = NULL;

#define IMAGE_COLORSPACE IMAGE_UTIL_COLORSPACE_I420

struct _imagedata_s {
    mv_source_h g_source;
    mv_engine_config_h g_engine_config;
    mv_image_object_h g_image_object;
    struct app_data_s *ad;
};
typedef struct _imagedata_s imagedata_s;
imagedata_s imagedata;

void mv_image_init(struct app_data_s *user_data){
	int error_code = 0;
	imagedata.ad = user_data;

	/* For details, see the Image Util API Reference */
	unsigned char *dataBuffer = NULL;
	unsigned long long bufferSize = 0;
	unsigned long width = 0;
	unsigned long height = 0;
	char* filepath = NULL;
	filepath = app_get_resource_path();
	char* filename = NULL;
	image_util_decode_h imageDecoder = NULL;

	error_code = mv_create_source(&imagedata.g_source);
	if (error_code != MEDIA_VISION_ERROR_NONE)
	    dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", error_code);

	error_code = mv_create_engine_config(&imagedata.g_engine_config);
	if (error_code != MEDIA_VISION_ERROR_NONE)
	    dlog_print(DLOG_ERROR, LOG_TAG, "error code= %d", error_code);

	error_code = image_util_decode_create(&imageDecoder);
	if (error_code != IMAGE_UTIL_ERROR_NONE)
	    dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", error_code);

	filename = g_strconcat(filepath, "star.jpg", NULL);
	error_code = image_util_decode_set_input_path(imageDecoder, filename);
	if (error_code != IMAGE_UTIL_ERROR_NONE)
	    dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", error_code);

	error_code = image_util_decode_set_colorspace(imageDecoder, IMAGE_UTIL_COLORSPACE_RGB888);
	if (error_code != IMAGE_UTIL_ERROR_NONE)
	    dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", error_code);

	error_code = image_util_decode_set_output_buffer(imageDecoder, &dataBuffer);
	if (error_code != IMAGE_UTIL_ERROR_NONE)
	    dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", error_code);

	error_code = image_util_decode_run(imageDecoder, &width, &height, &bufferSize);
	if (error_code != IMAGE_UTIL_ERROR_NONE)
	    dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", error_code);

	error_code = image_util_decode_destroy(imageDecoder);
	if (error_code != IMAGE_UTIL_ERROR_NONE)
	    dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", error_code);

	/* Fill the dataBuffer to g_source */
	error_code = mv_source_fill_by_buffer(imagedata.g_source, dataBuffer, (unsigned int)bufferSize,
	                                      (unsigned int)width, (unsigned int)height, MEDIA_VISION_COLORSPACE_RGB888);
	if (error_code != MEDIA_VISION_ERROR_NONE)
	    dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", error_code);

	free(dataBuffer);
	dataBuffer = NULL;

	error_code = mv_image_object_create(&imagedata.g_image_object);
	if (error_code != MEDIA_VISION_ERROR_NONE)
	    dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", error_code);

	error_code = mv_image_object_set_label(&imagedata.g_image_object, 1);
	if (error_code != MEDIA_VISION_ERROR_NONE)
	    dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", error_code);

	error_code = mv_image_object_fill(imagedata.g_image_object, imagedata.g_engine_config,
	                                  imagedata.g_source, NULL);
	if (error_code != MEDIA_VISION_ERROR_NONE)
	    dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", error_code);
}

void mv_image_destroy(){
	int error_code = 0;
	error_code = mv_destroy_source(imagedata.g_source);
	if (error_code != MEDIA_VISION_ERROR_NONE)
	    dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", error_code);

	error_code = mv_destroy_engine_config(imagedata.g_engine_config);
	if (error_code != MEDIA_VISION_ERROR_NONE)
	    dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", error_code);

	error_code = mv_image_object_destroy(imagedata.g_image_object);
	if (error_code != MEDIA_VISION_ERROR_NONE)
	    dlog_print(DLOG_ERROR, LOG_TAG, "error code = %d", error_code);
}

static void
_on_image_recognized_cb(mv_source_h source, mv_engine_config_h engine_config,
                        const mv_image_object_h *image_objects, mv_quadrangle_s **locations,
                        unsigned int number_of_objects, void *user_data)
{
    int object_num = 0;
    for (object_num = 0; object_num < number_of_objects; ++object_num) {
        if (locations[object_num]) {
            imagedata.ad->door = 1;
            int recognized_label = 0;
            mv_image_object_get_label(image_objects[object_num], &recognized_label);
            _E("image label [%d] on location: (%d,%d) -> (%d,%d) -> (%d,%d) -> (%d,%d)\n",
                       recognized_label, locations[object_num]->points[0].x, locations[object_num]->points[0].y,
                       locations[object_num]->points[1].x, locations[object_num]->points[1].y,
                       locations[object_num]->points[2].x, locations[object_num]->points[2].y,
                       locations[object_num]->points[3].x, locations[object_num]->points[3].y);
        }
    }
}

int mv_image_detect(void){
	int error_code = 0;

	/* For details, see the Image Util API Reference */
	unsigned char *dataBuffer = NULL;
	unsigned long long bufferSize = 0;
	unsigned long width = 0;
	unsigned long height = 0;
	char* filepath = NULL;
	filepath = app_get_shared_data_path();
	char* filename = NULL;
	image_util_decode_h imageDecoder = NULL;

	error_code = image_util_decode_create(&imageDecoder);
	if (error_code != IMAGE_UTIL_ERROR_NONE)
		return -1;
	filename = g_strconcat(filepath, "latest.jpg", NULL);
	error_code = image_util_decode_set_input_path(imageDecoder, filename);
	if (error_code != IMAGE_UTIL_ERROR_NONE)
		return -1;
	error_code = image_util_decode_set_colorspace(imageDecoder, IMAGE_UTIL_COLORSPACE_RGB888);
	if (error_code != IMAGE_UTIL_ERROR_NONE)
		return -1;
	error_code = image_util_decode_set_output_buffer(imageDecoder, &dataBuffer);
	if (error_code != IMAGE_UTIL_ERROR_NONE)
		return -1;
	error_code = image_util_decode_run(imageDecoder, &width, &height, &bufferSize);
	if (error_code != IMAGE_UTIL_ERROR_NONE)
		return -1;
	error_code = image_util_decode_destroy(imageDecoder);
	if (error_code != IMAGE_UTIL_ERROR_NONE)
		return -1;
	error_code = mv_source_clear(imagedata.g_source);
	if (error_code != MEDIA_VISION_ERROR_NONE)
		return -1;
	/* Fill the dataBuffer to g_source */
	error_code = mv_source_fill_by_buffer(imagedata.g_source, dataBuffer, (unsigned int)bufferSize,
	                                      (unsigned int)width, (unsigned int)height, MEDIA_VISION_COLORSPACE_RGB888);
	if (error_code != MEDIA_VISION_ERROR_NONE)
		return -1;
	free(dataBuffer);
	dataBuffer = NULL;

	error_code = mv_image_recognize(imagedata.g_source, &imagedata.g_image_object, 1,
	                                imagedata.g_engine_config, _on_image_recognized_cb, NULL);
	if (error_code != MEDIA_VISION_ERROR_NONE)
		return -1;
	return 1;
}

void controller_image_initialize(void)
{
	int error_code = image_util_encode_create(IMAGE_UTIL_JPEG, &encode_h);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_create [%s]", get_error_message(error_code));
	}

	error_code = image_util_decode_create(&decode_h);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_decode_create [%s]", get_error_message(error_code));
	}
}

void controller_image_finalize(void)
{
	int error_code = image_util_encode_destroy(encode_h);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_destroy [%s]", get_error_message(error_code));
	}

	error_code = image_util_decode_destroy(decode_h);
    if (error_code != IMAGE_UTIL_ERROR_NONE) {
        _E("image_util_decode_destroy [%s]", get_error_message(error_code));
    }
}

int controller_image_save_image_file(const char *path,
	unsigned int width, unsigned int height, const unsigned char *buffer,
	const char *comment, unsigned int comment_len)
{
	unsigned char *encoded = NULL;
	unsigned long long size = 0;

	int error_code = image_util_encode_set_resolution(encode_h, width, height);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_set_resolution [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_encode_set_colorspace(encode_h, IMAGE_COLORSPACE);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_set_colorspace [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_encode_set_quality(encode_h, 90);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_set_quality [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_encode_set_input_buffer(encode_h, buffer);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_set_input_buffer [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_encode_set_output_buffer(encode_h, &encoded);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_set_output_buffer [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_encode_run(encode_h, &size);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_encode_run [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = exif_write_jpg_file_with_comment(path,
			encoded, (unsigned int)size, width, height, comment, comment_len);

	free(encoded);

	return error_code;
}

int controller_image_read_image_file(const char *path,
	unsigned long *width, unsigned long *height, unsigned char *buffer, unsigned long long *size)
{
	int error_code = image_util_decode_set_input_path(decode_h, path);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_decode_set_input_path [%s] [%s]", path, get_error_message(error_code));
		return -1;
	}

	error_code = image_util_decode_set_output_buffer(decode_h, &buffer);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_decode_set_output_buffer [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_decode_set_colorspace(decode_h, IMAGE_UTIL_COLORSPACE_RGBA8888);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_decode_set_colorspace [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_decode_set_jpeg_downscale(decode_h, IMAGE_UTIL_DOWNSCALE_1_1);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_decode_set_jpeg_downscale [%s]", get_error_message(error_code));
		return -1;
	}

	error_code = image_util_decode_run(decode_h, width, height, size);
	if (error_code != IMAGE_UTIL_ERROR_NONE) {
		_E("image_util_decode_run [%s]", get_error_message(error_code));
		return -1;
	}

	return error_code;
}
