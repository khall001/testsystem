#include <timer.h>
#include <pwm.h>
#include <capture.h>
#include <iomux.h>
#include <gpio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <devs.h>
#include <stdio.h>
#define PWM_PERIOD 1000000

bool timer_callback(struct timer *timer, void *data) {
#ifndef CONFIG_STM32_TIM4_PWM_3
	int32_t ret;
	struct gpio_pin *pin = data;
	ret = gpioPin_togglePin(pin);
	CONFIG_ASSERT(ret >= 0);
#endif
	return false;
}
uint64_t oldtime = 0;
bool capture_callback(struct capture *capture, uint32_t index, uint64_t time, void *data) {
	if (time < oldtime) {
		printf("Capture Time: %llu\n", ((PWM_PERIOD - oldtime) + time));
	} else {
		printf("Capture Time: %llu\n", time - oldtime);
	}
	oldtime = time;
	return false;
}
bool gpio_callback(struct gpio_pin *pin, uint32_t pinID, void *data) {
	TickType_t lastWakeUpTime = xTaskGetTickCount();
	struct timer *timer = data;
	uint64_t time = timer_getTime(timer);
	printf("%lu: GPIO Capture TIme: %llu\n", lastWakeUpTime ,time - oldtime);
	oldtime = time;
	return false;
}
struct capture *capture;
void timertest_task(void *data) {
	uint64_t us[3];
	struct timer **timer = data;
	TickType_t lastWakeUpTime = xTaskGetTickCount();
	for (;;) {
		us[0] = timer_getTime(timer[0]);
		printf("%lu: Time of Timer: %llu us\n", lastWakeUpTime, us[0]);
		if (timer[1] && capture) {
			us[1] = timer_getTime(timer[1]);
			us[2] = capture_getChannelTime(capture);
			printf("%lu: Time of Timer2: %llu us %llu us\n", lastWakeUpTime, us[1], us[2]);
		}
		vTaskDelayUntil(&lastWakeUpTime, 1000 / portTICK_PERIOD_MS);
	}
}
OS_DEFINE_TASK(timerTask, 786);
void timertest_init() {
	int32_t ret;
	struct gpio_pin *pin = NULL;
#ifndef CONFIG_STM32_TIM4_PWM_3
	struct gpio *gpio = gpio_init(GPIO_ID);
#else
	struct pwm *pwm;
#endif
	static struct timer *timer[2] = {NULL, NULL};
	timer[0] = timer_init(TIMER4_ID, 5000, 10, 0);
	CONFIG_ASSERT(timer != NULL);
#ifndef CONFIG_STM32_TIM4_PWM_3
	pin = gpioPin_init(gpio, PTB8, GPIO_OUTPUT, GPIO_PULL_UP);
	CONFIG_ASSERT(pin != NULL);
	printf("Start Timer\n");
	ret = timer_periodic(timer[0], 100000);
	CONFIG_ASSERT(ret >= 0);
#else
	pwm = pwm_init(PWM4_3_ID);
	CONFIG_ASSERT(pwm != NULL);
	ret = pwm_setPeriod(pwm, 1000000);
	CONFIG_ASSERT(ret >= 0);
	ret = pwm_setDutyCycle(pwm, 500000);
	CONFIG_ASSERT(ret >= 0);
#endif
#if defined(CONFIG_STM32_TIM9_CAPTURE_2) || defined(CONFIG_STM32_TIM9_PWM_1)
	{
		timer[1] = timer_init(TIMER9_ID, 5000, 10, 0);
		CONFIG_ASSERT(timer[1] != NULL);
# ifdef CONFIG_STM32_TIM9_CAPTURE_2
			capture = capture_init(CAPTURE9_2_ID);
			CONFIG_ASSERT(capture != NULL);
			ret = capture_setCallback(capture, capture_callback, NULL);
			CONFIG_ASSERT(ret >= 0);
			ret = capture_setPeriod(capture, PWM_PERIOD);
			CONFIG_ASSERT(ret >= 0);
# endif
# ifdef CONFIG_STM32_TIM9_PWM_1
		{
			struct pwm *pwm2;
			pwm2 = pwm_init(PWM9_1_ID);
			CONFIG_ASSERT(pwm2 != NULL);
			ret = pwm_setPeriod(pwm2, PWM_PERIOD);
			CONFIG_ASSERT(ret >= 0);
			ret = pwm_setDutyCycle(pwm2, PWM_PERIOD / 2);
			CONFIG_ASSERT(ret >= 0);
		}
# endif
	}
#else
	{
		struct gpio *gpio;
		struct gpio_pin *pin;
		gpio = gpio_init(GPIO_ID);
		CONFIG_ASSERT(gpio != NULL);
		pin = gpioPin_init(gpio, PTE6, GPIO_INPUT, GPIO_OPEN);
		CONFIG_ASSERT(pin != NULL);
		ret = gpioPin_setCallback(pin, gpio_callback, timer[0], GPIO_EITHER);
		CONFIG_ASSERT(ret >= 0);
		ret = gpioPin_enableInterrupt(pin);
		CONFIG_ASSERT(ret >= 0);
	}
#endif
	ret = timer_setOverflowCallback(timer[0], timer_callback, pin);
	CONFIG_ASSERT(ret >= 0);
	if (timer[1]) {
		ret = timer_setOverflowCallback(timer[1], timer_callback, pin);
		CONFIG_ASSERT(ret >= 0);
	}
	ret = OS_CREATE_TASK(timertest_task, "Timer test task", 786, timer, 1, timerTask);
	CONFIG_ASSERT(ret == pdPASS);
}
