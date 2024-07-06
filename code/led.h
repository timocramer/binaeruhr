#pragma once

#include <stdint.h>

void led_init_outputs();

void led_show_time(uint8_t hours, uint8_t minutes);
void led_show_hours(uint8_t hours);
void led_show_minutes(uint8_t minutes);

// This function is used to show a nice blinking pattern. The input is a bit pattern with
// the least significant bit as hour8 and the most significant bit as minute32, forming
// a clockwise circle.
//
// bit |  0   1   2   3   4   5   6   7    8   9
// led | h8  h4  h2  h1  m1  m2  m4  m8  m16 m23
void led_show_bit_pattern(uint16_t bit_pattern);

void leds_on();
void leds_off();
void led_hours_off();
void led_minutes_off();
