#include <tizen.h>
#include <service_app.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <Ecore.h>
#include <glib.h>
#include <camera.h>
#include <mv_common.h>
#include <pthread.h>
#include <security-entrance-tizen-app.h>

#include "log.h"
#include "resource_infrared_motion_sensor.h"
#include "resource_led.h"
#include "resource_servo_motor.h"
#include "resource_camera.h"
#include "controller_image.h"
#include "controller_mv.h"

#define CAMERA_MOVE_INTERVAL_MS 450
#define THRESHOLD_VALID_EVENT_COUNT 2
#define VALID_EVENT_INTERVAL_MS 200

#define IMAGE_FILE_PREFIX "CAM_"
#define EVENT_INTERVAL_SECOND 0.5f

// Duration for a timer
#define TIMER_GATHER_INTERVAL (1.0f)		// PIR 모으는 시간

// Motion sensor info
#define SENSOR_MOTION_GPIO_NUMBER (130)

// LED info
#define SENSOR_LED_GPIO_NUMBER (46)

// Peripheral info for Servo-motor(HS-53)
// Duty Cycle : 0.54ms ~ 2.1ms
// Spec Duty Cycle : 0.553ms ~ 2.227ms(https://www.servocity.com/hitec-hs-53-servo)
#define SERVO_MOTOR_DUTY_CYCLE_COUNTER_CLOCKWISE 1.0
#define SERVO_MOTOR_DUTY_CYCLE_CLOCKWISE 2.0

#define SENSOR_GATHER_INTERVAL (2.0f)		// 모터 시간

#define APP_CALLBACK_KEY "security"


static app_data *g_ad = NULL;


static int _set_led(int on)
{
	int ret = 0;

	ret = resource_write_led(SENSOR_LED_GPIO_NUMBER, on);
	if (ret != 0) {
		_E("cannot write led data");
		return -1;
	}

	return 0;
}

static int _set_servo_motor(int on)
{
	double duty_cycle = 0;
	int ret = 0;

	if (on==1)		// 열기
		duty_cycle = SERVO_MOTOR_DUTY_CYCLE_CLOCKWISE;
	else			// 닫기
		duty_cycle = SERVO_MOTOR_DUTY_CYCLE_COUNTER_CLOCKWISE;

	ret = resource_set_servo_motor_value(duty_cycle);
	if (ret != 0) {
		_E("cannot set servo motor");
		return -1;
	}

	return 0;
}

static Eina_Bool __motion_to_led(void *user_data)
{
	int ret = 0;
	uint32_t value = 0;
	app_data *data = (app_data*) user_data;

	ret = resource_read_infrared_motion_sensor(SENSOR_MOTION_GPIO_NUMBER, &value);
	if (ret != 0) {
		_E("cannot read data from the infrared motion sensor");
		return ECORE_CALLBACK_CANCEL;
	}

	ret = _set_led((int)value);
	if (ret == -1) {
			_E("cannot write led data");
			return ECORE_CALLBACK_CANCEL;
	}

	if((int)value==0){
		data->door = 0;
	} else {
		if(mv_image_detect() < 0)
			_E("detect error");
	}

	ret = _set_servo_motor(data->door);

	if (ret == -1) {
		_E("cannot write led data");
		return ECORE_CALLBACK_CANCEL;
	}
	_I("LED : %u", value);

	return ECORE_CALLBACK_RENEW;
}

static void _stop_gathering(void *data)
{
	// PIR, LED
	app_data *ad = data;
	ret_if(!ad);

	if (ad->getter_timer_PL) {
		ecore_timer_del(ad->getter_timer_PL);
		ad->getter_timer_PL = NULL;
	}

}

static void _start_gathering(void *data)
{
	// PIR, LED
	app_data *ad = data;
	ret_if(!ad);

	if (ad->getter_timer_PL)
		ecore_timer_del(ad->getter_timer_PL);

	ad->getter_timer_PL = ecore_timer_add(TIMER_GATHER_INTERVAL, __motion_to_led, ad);
	if (!ad->getter_timer_PL)
		_E("Failed to add a timer PL");

}


static mv_colorspace_e __convert_colorspace_from_cam_to_mv(camera_pixel_format_e format)
{
	mv_colorspace_e colorspace = MEDIA_VISION_COLORSPACE_INVALID;
	switch (format) {
	case CAMERA_PIXEL_FORMAT_NV12:
		colorspace = MEDIA_VISION_COLORSPACE_NV12;
		break;
	case CAMERA_PIXEL_FORMAT_NV21:
		colorspace = MEDIA_VISION_COLORSPACE_NV21;
		break;
	case CAMERA_PIXEL_FORMAT_YUYV:
		colorspace = MEDIA_VISION_COLORSPACE_YUYV;
		break;
	case CAMERA_PIXEL_FORMAT_UYVY:
		colorspace = MEDIA_VISION_COLORSPACE_UYVY;
		break;
	case CAMERA_PIXEL_FORMAT_422P:
		colorspace = MEDIA_VISION_COLORSPACE_422P;
		break;
	case CAMERA_PIXEL_FORMAT_I420:
		colorspace = MEDIA_VISION_COLORSPACE_I420;
		break;
	case CAMERA_PIXEL_FORMAT_YV12:
		colorspace = MEDIA_VISION_COLORSPACE_YV12;
		break;
	case CAMERA_PIXEL_FORMAT_RGB565:
		colorspace = MEDIA_VISION_COLORSPACE_RGB565;
		break;
	case CAMERA_PIXEL_FORMAT_RGB888:
		colorspace = MEDIA_VISION_COLORSPACE_RGB888;
		break;
	case CAMERA_PIXEL_FORMAT_RGBA:
		colorspace = MEDIA_VISION_COLORSPACE_RGBA;
		break;
	case CAMERA_PIXEL_FORMAT_NV12T:
	case CAMERA_PIXEL_FORMAT_NV16:
	case CAMERA_PIXEL_FORMAT_ARGB:
	case CAMERA_PIXEL_FORMAT_JPEG:
	case CAMERA_PIXEL_FORMAT_H264:
	case CAMERA_PIXEL_FORMAT_INVALID:
	default :
		colorspace = MEDIA_VISION_COLORSPACE_INVALID;
		_E("unsupported format : %d", format);
		break;
	}

	return colorspace;
}

static void __thread_write_image_file(void *data, Ecore_Thread *th)
{
	app_data *ad = (app_data *)data;
	unsigned int width = 0;
	unsigned int height = 0;
	unsigned char *buffer = 0;
	char *image_info = NULL;
	int ret = 0;

	pthread_mutex_lock(&ad->mutex);
	width = ad->latest_image_width;
	height = ad->latest_image_height;
	buffer = ad->latest_image_buffer;
	ad->latest_image_buffer = NULL;
	if (ad->latest_image_info) {
		image_info = ad->latest_image_info;
		ad->latest_image_info = NULL;
	} else {
		image_info = strdup("00");
	}
	pthread_mutex_unlock(&ad->mutex);

	ret = controller_image_save_image_file(ad->temp_image_filename, width, height, buffer, image_info, strlen(image_info));
	if (ret) {
		_E("failed to save image file");
	} else {
		ret = rename(ad->temp_image_filename, ad->latest_image_filename);
		if (ret != 0 )
			_E("Rename fail");
	}
	free(image_info);
	free(buffer);
}

static void __thread_end_cb(void *data, Ecore_Thread *th)
{
	app_data *ad = (app_data *)data;

	pthread_mutex_lock(&ad->mutex);
	ad->image_writter_thread = NULL;
	pthread_mutex_unlock(&ad->mutex);
}

static void __thread_cancel_cb(void *data, Ecore_Thread *th)
{
	app_data *ad = (app_data *)data;
	unsigned char *buffer = NULL;

	_E("Thread %p got cancelled.\n", th);
	pthread_mutex_lock(&ad->mutex);
	buffer = ad->latest_image_buffer;
	ad->latest_image_buffer = NULL;
	ad->image_writter_thread = NULL;
	pthread_mutex_unlock(&ad->mutex);

	free(buffer);
}

static void __copy_image_buffer(image_buffer_data_s *image_buffer, app_data *ad)
{
	unsigned char *buffer = NULL;

	pthread_mutex_lock(&ad->mutex);
	ad->latest_image_height = image_buffer->image_height;
	ad->latest_image_width = image_buffer->image_width;

	buffer = ad->latest_image_buffer;
	ad->latest_image_buffer = image_buffer->buffer;
	pthread_mutex_unlock(&ad->mutex);

	free(buffer);
}

static void __preview_image_buffer_created_cb(void *data)
{
	image_buffer_data_s *image_buffer = data;
	app_data *ad = (app_data *)image_buffer->user_data;
	mv_colorspace_e image_colorspace = MEDIA_VISION_COLORSPACE_INVALID;
	char *info = NULL;

	ret_if(!image_buffer);
	ret_if(!ad);

	image_colorspace = __convert_colorspace_from_cam_to_mv(image_buffer->format);
	goto_if(image_colorspace == MEDIA_VISION_COLORSPACE_INVALID, FREE_ALL_BUFFER);

	__copy_image_buffer(image_buffer, ad);

	pthread_mutex_lock(&ad->mutex);
	info = ad->latest_image_info;
	ad->latest_image_info = NULL;
	pthread_mutex_unlock(&ad->mutex);
	free(info);

	free(image_buffer);


	pthread_mutex_lock(&ad->mutex);
	if (!ad->image_writter_thread) {
		ad->image_writter_thread = ecore_thread_run(__thread_write_image_file, __thread_end_cb, __thread_cancel_cb, ad);
	} else {
		_E("Thread is running NOW");
	}
	pthread_mutex_unlock(&ad->mutex);

	return;

FREE_ALL_BUFFER:
	free(image_buffer->buffer);
	free(image_buffer);
}

static bool service_app_create(void *user_data)
{

	app_data *ad = (app_data *)user_data;

	char* shared_data_path = app_get_shared_data_path();
	if (shared_data_path == NULL) {
		_E("Failed to get shared data path");
		goto ERROR;
	}
	ad->temp_image_filename = g_strconcat(shared_data_path, "tmp.jpg", NULL);
	ad->latest_image_filename = g_strconcat(shared_data_path, "latest.jpg", NULL);
	free(shared_data_path);

		_D("%s", ad->temp_image_filename);
		_D("%s", ad->latest_image_filename);

	controller_image_initialize();
	mv_image_init(user_data);

	pthread_mutex_init(&ad->mutex, NULL);

	if (resource_camera_init(__preview_image_buffer_created_cb, ad) == -1) {
		_E("Failed to init camera");
		goto ERROR;
	}

	if (resource_camera_start_preview() == -1) {
		_E("Failed to start camera preview");
		goto ERROR;
	}


	return true;

	ERROR:

	resource_camera_close();
	controller_image_finalize();

	pthread_mutex_destroy(&ad->mutex);

	return false;
}


static void service_app_control(app_control_h app_control, void *user_data)
{
	_stop_gathering(user_data);
	_start_gathering(user_data);
}

static void service_app_terminate(void *user_data)
{
	app_data *ad = user_data;
	int ret = 0;

	// Turn off Camera
	Ecore_Thread *thread_id = NULL;
	unsigned char *buffer = NULL;
	char *info = NULL;
	gchar *temp_image_filename;
	gchar *latest_image_filename;
	_D("App Terminated - enter");

	// Turn off LED light with __set_led()
	ret = resource_write_led(SENSOR_LED_GPIO_NUMBER, 0);
	if (ret != 0) {
		_E("cannot write led data");
	}

	// Close Motion and LED resources
	resource_close_infrared_motion_sensor();
	resource_close_led();

	// Close Motor
	resource_close_servo_motor();

	// Close Camera
	resource_camera_close();

	pthread_mutex_lock(&ad->mutex);
	thread_id = ad->image_writter_thread;
	ad->image_writter_thread = NULL;
	pthread_mutex_unlock(&ad->mutex);

	if (thread_id)
		ecore_thread_wait(thread_id, 3.0); // wait for 3 second

	controller_image_finalize();

	pthread_mutex_lock(&ad->mutex);
	buffer = ad->latest_image_buffer;
	ad->latest_image_buffer = NULL;
	info  = ad->latest_image_info;
	ad->latest_image_info = NULL;
	temp_image_filename = ad->temp_image_filename;
	ad->temp_image_filename = NULL;
	latest_image_filename = ad->latest_image_filename;
	ad->latest_image_filename = NULL;
	pthread_mutex_unlock(&ad->mutex);
	free(buffer);
	free(info);
	g_free(temp_image_filename);
	g_free(latest_image_filename);

	pthread_mutex_destroy(&ad->mutex);

	// Free app data
	free(ad);

	FN_END;
}

int main(int argc, char *argv[])
{
	app_data *ad = NULL;
	service_app_lifecycle_callback_s event_callback;

	ad = calloc(1, sizeof(app_data));
	ad->door = 0;		// 보안카드 없음으로 초기화
	retv_if(!ad, -1);

	g_ad = ad;

	event_callback.create = service_app_create;
	event_callback.terminate = service_app_terminate;
	event_callback.app_control = service_app_control;

	return service_app_main(argc, argv, &event_callback, ad);
}
