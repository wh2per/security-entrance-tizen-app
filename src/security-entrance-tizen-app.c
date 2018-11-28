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
#include "motion.h"
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


typedef struct app_data_s {
	Ecore_Timer *getter_timer_PL;		// PIR과 LED용 app_data : 개더링 타임
	Ecore_Timer *getter_timer_MT;		// 모터용 app_data : 개더링 타임
	int door;						// 모터용 app_data : 보안카드가 있으면 1, 없으면 0

	// 카메라용 app_data
	double current_servo_x;
	double current_servo_y;
	int motion_state;

	long long int last_moved_time;
	long long int last_valid_event_time;
	int valid_vision_result_x_sum;
	int valid_vision_result_y_sum;
	int valid_event_count;

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

static app_data *g_ad = NULL;

void __device_interfaces_fini(void)
{
	motion_finalize();
}

int __device_interfaces_init(app_data *ad)
{
	retv_if(!ad, -1);

	if (motion_initialize()) {
		_E("failed to motion_initialize()");
		goto ERROR;
	}

	return 0;

ERROR :
	__device_interfaces_fini();
	return -1;
}

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

static long long int __get_monotonic_ms(void)
{
	long long int ret_time = 0;
	struct timespec time_s;

	if (0 == clock_gettime(CLOCK_MONOTONIC, &time_s))
		ret_time = time_s.tv_sec* 1000 + time_s.tv_nsec / 1000000;
	else
		_E("Failed to get time");

	return ret_time;
}

static Eina_Bool _motion_to_led(void *user_data)
{
	int ret = 0;
	uint32_t value = 0;

	ret = resource_read_infrared_motion_sensor(SENSOR_MOTION_GPIO_NUMBER, &value);
	if (ret != 0) {
		_E("cannot read data from the infrared motion sensor");
		return ECORE_CALLBACK_CANCEL;
	}

	ret = _set_led((int)value);

	///////////////////////////
	//	app_data에 pir_detect 변수 추가해서 (int)value로 설정
	////////////////////////
	if (ret == -1) {
		_E("cannot write led data");
		return ECORE_CALLBACK_CANCEL;
	}
	_I("LED : %u", value);

	return ECORE_CALLBACK_RENEW;
}

static int _set_servo_motor(int on, int door)
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

static Eina_Bool __start_servo_motor(void *data){
	int on = 0;
	app_data *ad = data;
	int ret = 0;

	if (!ad) {
		_E("failed to get app_data");
	    return ECORE_CALLBACK_CANCEL;
	}

	_I("door : %d", ad->door);
	if(ad->door==1){					// 이미지 분석 후 통과면 ad->door = 1로 바꾼것
		on = 1;		// 열기
	}else {
		on = 0;		// 닫기
	}

	ret = _set_servo_motor(on,ad->door);
	if (ret != 0) {
		_E("cannot set servo motor");
		return ECORE_CALLBACK_RENEW;
	}
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

	// MOTOR
	if (ad->getter_timer_MT) {
		ecore_timer_del(ad->getter_timer_MT);
		ad->getter_timer_MT = NULL;
	}
}

static void _start_gathering(void *data)
{
	// PIR, LED
	app_data *ad = data;
	ret_if(!ad);

	if (ad->getter_timer_PL)
		ecore_timer_del(ad->getter_timer_PL);

	ad->getter_timer_PL = ecore_timer_add(TIMER_GATHER_INTERVAL, _motion_to_led, ad);
	if (!ad->getter_timer_PL)
		_E("Failed to add a timer PL");

	// MOTOR
	ad->getter_timer_MT = ecore_timer_add(SENSOR_GATHER_INTERVAL, __start_servo_motor, ad);
	if(!ad->getter_timer_MT)
		_E("Failed to add a timer MT");
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

	motion_state_set(ad->motion_state, APP_CALLBACK_KEY);
	ad->motion_state = 0;

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

static void __set_result_info(int result[], int result_count, app_data *ad, int image_result_type)
{
	char image_info[IMAGE_INFO_MAX + 1] = {'\0', };
	char *current_position;
	int current_index = 0;
	int string_count = 0;
	int i = 0;
	char *latest_image_info = NULL;
	char *info = NULL;

	current_position = image_info;

	current_position += snprintf(current_position, IMAGE_INFO_MAX, "%02d", image_result_type);
	string_count += 2;

	current_position += snprintf(current_position, IMAGE_INFO_MAX - string_count, "%02d", result_count);
	string_count += 2;

	for (i = 0; i < result_count; i++) {
		current_index = i * 4;
		if (IMAGE_INFO_MAX - string_count < 8)
			break;

		current_position += snprintf(current_position, IMAGE_INFO_MAX - string_count, "%02d%02d%02d%02d"
			, result[current_index], result[current_index + 1], result[current_index + 2], result[current_index + 3]);
		string_count += 8;
	}

	latest_image_info = strdup(image_info);
	pthread_mutex_lock(&ad->mutex);
	info = ad->latest_image_info;
	ad->latest_image_info = latest_image_info;
	pthread_mutex_unlock(&ad->mutex);
	free(info);
}

static void __mv_detection_event_cb(int horizontal, int vertical, int result[], int result_count, void *user_data)
{
	app_data *ad = (app_data *)user_data;
	long long int now = __get_monotonic_ms();

	ad->motion_state = 1;

	if (now < ad->last_moved_time + CAMERA_MOVE_INTERVAL_MS) {
		ad->valid_event_count = 0;
		pthread_mutex_lock(&ad->mutex);
		ad->latest_image_type = 0; // 0: image during camera repositioning
		pthread_mutex_unlock(&ad->mutex);
		__set_result_info(result, result_count, ad, 0);
		return;
	}

	if (now < ad->last_valid_event_time + VALID_EVENT_INTERVAL_MS) {
		ad->valid_event_count++;
	} else {
		ad->valid_event_count = 1;
	}

	ad->last_valid_event_time = now;

	if (ad->valid_event_count < THRESHOLD_VALID_EVENT_COUNT) {
		ad->valid_vision_result_x_sum += horizontal;
		ad->valid_vision_result_y_sum += vertical;
		pthread_mutex_lock(&ad->mutex);
		ad->latest_image_type = 1; // 1: single valid image but not completed
		pthread_mutex_unlock(&ad->mutex);
		__set_result_info(result, result_count, ad, 1);
		return;
	}

	ad->valid_event_count = 0;
	ad->valid_vision_result_x_sum += horizontal;
	ad->valid_vision_result_y_sum += vertical;

	int x = ad->valid_vision_result_x_sum / THRESHOLD_VALID_EVENT_COUNT;
	int y = ad->valid_vision_result_y_sum / THRESHOLD_VALID_EVENT_COUNT;

	x = 10 * x / (IMAGE_WIDTH / 2);
	y = 10 * y / (IMAGE_HEIGHT / 2);

	ad->last_moved_time = now;

	ad->valid_vision_result_x_sum = 0;
	ad->valid_vision_result_y_sum = 0;
	pthread_mutex_lock(&ad->mutex);
	ad->latest_image_type = 2; // 2: fully validated image
	pthread_mutex_unlock(&ad->mutex);

	__set_result_info(result, result_count, ad, 2);
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

	pthread_mutex_init(&ad->mutex, NULL);

	if (__device_interfaces_init(ad))
		goto ERROR;

	if (controller_mv_set_movement_detection_event_cb(__mv_detection_event_cb, user_data) == -1) {
		_E("Failed to set movement detection event callback");
		goto ERROR;
	}

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
	//	__device_interfaces_fini();

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
