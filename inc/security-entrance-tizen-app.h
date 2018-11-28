#ifndef __security_entrance_tizen_app_H__
#define __security_entrance_tizen_app_H__

#include <dlog.h>
#include <Ecore.h>
#include <pthread.h>

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "security-entrance-tizen-app"

#define MV_RESULT_COUNT_MAX 30
#define MV_RESULT_LENGTH_MAX (MV_RESULT_COUNT_MAX * 4) //4(x, y, w, h) * COUNT
#define IMAGE_INFO_MAX ((8 * MV_RESULT_LENGTH_MAX) + 4)

#define IMAGE_WIDTH 320
#define IMAGE_HEIGHT 240
#define CAMERA_IMAGE_QUALITY 100 //1~100
#define CAMERA_PREVIEW_INTERVAL_MIN 50

typedef struct app_data_s {
	Ecore_Timer *getter_timer_PL;		// PIR과 LED용 app_data : 개더링 타임
	Ecore_Timer *getter_timer_MT;		// 모터용 app_data : 개더링 타임
	int door;						// 모터용 app_data : 보안카드가 있으면 1, 없으면 0
	int pir_detect;

	unsigned int latest_image_width;
	unsigned int latest_image_height;
	char *latest_image_info;
	int latest_image_type; // 0: image during camera repositioning, 1: single valid image but not completed, 2: fully validated image
	unsigned char *latest_image_buffer;

	Ecore_Thread *image_writter_thread;
	pthread_mutex_t mutex;

	char* temp_image_filename;
	char* latest_image_filename;
} app_data;

#endif /* __smart-security-entrance_H__ */
