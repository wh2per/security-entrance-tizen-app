#include <tizen.h>
#include <service_app.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <Ecore.h>
#include <security-entrance-tizen-app.h>

#include "log.h"
#include "resource_infrared_motion_sensor.h"
#include "resource_led.h"
#include "resource_servo_motor.h"

// Duration for a timer
#define TIMER_GATHER_INTERVAL (1.0f)

// Motion sensor info
#define SENSOR_MOTION_GPIO_NUMBER (130)

// LED info
#define SENSOR_LED_GPIO_NUMBER (46)

// Peripheral info for Servo-motor(HS-53)
// Duty Cycle : 0.54ms ~ 2.1ms
// Spec Duty Cycle : 0.553ms ~ 2.227ms(https://www.servocity.com/hitec-hs-53-servo)
#define SERVO_MOTOR_DUTY_CYCLE_COUNTER_CLOCKWISE 1.0
#define SERVO_MOTOR_DUTY_CYCLE_CLOCKWISE 2.0

#define SENSOR_GATHER_INTERVAL (1.0f)


typedef struct app_data_s {
	Ecore_Timer *getter_timer_PL;		// PIR과 LED용 app_data : 개더링 타임
	Ecore_Timer *getter_timer_MT;		// 모터용 app_data : 개더링 타임
	int door;						// 모터용 app_data : 보안카드가 있으면 1, 없으면 0
} app_data;

static app_data *g_ad = NULL;

static int _set_led(int on)
{
	int ret = 0;

	ret = resource_write_led(SENSOR_LED_GPIO_NUMBER, on);
	if (ret != 0) {
		_E("cannot write led data");
		return -1;
	}

	// 보안카드가 있다면
	if(on==1)
		return 1;

	return 0;
}

static Eina_Bool _motion_to_led(void *user_data)
{
	int ret = 0;
	uint32_t value = 0;
	app_data *ad = user_data;

	ret = resource_read_infrared_motion_sensor(SENSOR_MOTION_GPIO_NUMBER, &value);
	if (ret != 0) {
		_E("cannot read data from the infrared motion sensor");
		return ECORE_CALLBACK_CANCEL;
	}

	ret = _set_led((int)value);
	if (ret == -1) {
		_E("cannot write led data");
		return ECORE_CALLBACK_CANCEL;
	}else if(ret == 1){
		ad->door = 1;
	}else {
		ad->door = 0;
	}
	_I("LED : %u", value);

	return ECORE_CALLBACK_RENEW;
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

static Eina_Bool __start_servo_motor(void *data){
	int on = 0;
	app_data *ad = data;
	int ret = 0;

	if (!ad) {
		_E("failed to get app_data");
	    return ECORE_CALLBACK_CANCEL;
	}

	_E("door : %d", ad->door);
	if(ad->door==1){
		on = 1;		// 열기
	}else {
		on = 0;		// 닫기
	}

	ret = _set_servo_motor(on);
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


static bool service_app_create(void *user_data)
{

	return true;
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
