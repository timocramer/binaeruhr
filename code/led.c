#include "led.h"

#include <avr/io.h>

void led_init_outputs() {
    DDRD |= _BV(PD4); // hour8
    DDRD |= _BV(PD1); // hour4
    DDRD |= _BV(PD0); // hour2
    DDRC |= _BV(PC5); // hour1
    
    DDRB |= _BV(PB0); // minute32
    DDRB |= _BV(PB1); // minute16
    DDRB |= _BV(PB2); // minute8
    DDRB |= _BV(PB3); // minute4
    DDRB |= _BV(PB4); // minute2
    DDRB |= _BV(PB5); // minute1
}

static void hour8_on() {
    PORTD |= _BV(PD4);
}

static void hour8_off() {
    PORTD &= ~_BV(PD4);
}

static void hour4_on() {
    PORTD |= _BV(PD1);
}

static void hour4_off() {
    PORTD &= ~_BV(PD1);
}

static void hour2_on() {
    PORTD |= _BV(PD0);
}

static void hour2_off() {
    PORTD &= ~_BV(PD0);
}

static void hour1_on() {
    PORTC |= _BV(PC5);
}

static void hour1_off() {
    PORTC &= ~_BV(PC5);
}

void led_show_hours(uint8_t hours) {
    if(hours & 8) {
        hour8_on();
    } else {
        hour8_off();
    }
    if(hours & 4) {
        hour4_on();
    } else {
        hour4_off();
    }
    if(hours & 2) {
        hour2_on();
    } else {
        hour2_off();
    }
    if(hours & 1) {
        hour1_on();
    } else {
        hour1_off();
    }
}

static void minute32_on() {
    PORTB |= _BV(PB0);
}

static void minute32_off() {
    PORTB &= ~_BV(PB0);
}

static void minute16_on() {
    PORTB |= _BV(PB1);
}

static void minute16_off() {
    PORTB &= ~_BV(PB1);
}

static void minute8_on() {
    PORTB |= _BV(PB2);
}

static void minute8_off() {
    PORTB &= ~_BV(PB2);
}

static void minute4_on() {
    PORTB |= _BV(PB3);
}

static void minute4_off() {
    PORTB &= ~_BV(PB3);
}

static void minute2_on() {
    PORTB |= _BV(PB4);
}

static void minute2_off() {
    PORTB &= ~_BV(PB4);
}

static void minute1_on() {
    PORTB |= _BV(PB5);
}

static void minute1_off() {
    PORTB &= ~_BV(PB5);
}

void led_show_minutes(uint8_t minutes) {
    if(minutes & 32) {
        minute32_on();
    } else {
        minute32_off();
    }
    if(minutes & 16) {
        minute16_on();
    } else {
        minute16_off();
    }
    if(minutes & 8) {
        minute8_on();
    } else {
        minute8_off();
    }
    if(minutes & 4) {
        minute4_on();
    } else {
        minute4_off();
    }
    if(minutes & 2) {
        minute2_on();
    } else {
        minute2_off();
    }
    if(minutes & 1) {
        minute1_on();
    } else {
        minute1_off();
    }
}

void led_show_time(uint8_t hours, uint8_t minutes) {
    led_show_hours(hours);
    led_show_minutes(minutes);
}

void led_hours_off() {
    hour8_off();
    hour4_off();
    hour2_off();
    hour1_off();
}

void led_minutes_off() {
    minute32_off();
    minute16_off();
    minute8_off();
    minute4_off();
    minute2_off();
    minute1_off();
}

void leds_off() {
    led_hours_off();
    led_minutes_off();
}

void leds_on() {
    hour8_on();
    hour4_on();
    hour2_on();
    hour1_on();
    minute32_on();
    minute16_on();
    minute8_on();
    minute4_on();
    minute2_on();
    minute1_on();
}
