#pragma once

#include <stdint.h>

void led_init_outputs();

void led_show_time(uint8_t hours, uint8_t minutes);
void led_show_hours(uint8_t hours);
void led_show_minutes(uint8_t minutes);

void leds_on();
void leds_off();
void led_hours_off();
void led_minutes_off();
