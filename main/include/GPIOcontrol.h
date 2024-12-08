#ifndef __GPIOCONTROL__
#define __GPIOCONTROL__

#include "driver/gpio.h"

#define LED1 12
#define LED2 13
#define IO_ISR 2
#define ESP_INTR_FLAG_DEFAULT 0

void gpio_init();
void set_led_state(uint8_t state);

#endif