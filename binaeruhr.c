#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stddef.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "led.h"

struct localtime {
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
};

static const uint16_t TIME_SETTING_PRESCALER = 64;

#define TIME_COUNTING_PRESCALER 1024
#if TIME_COUNTING_PRESCALER < 128
	#error "TIME_COUNTING_PRESCALER needs to be at least 128"
#endif

// quartz frequency is 32768 Hz and timer has 8 bits, so a timer overflow happens every (prescaler / (32768 / 256)) seconds
static const uint8_t second_increment = (uint16_t)TIME_COUNTING_PRESCALER / 128;

// debug
// static const uint8_t second_increment = 60;

static volatile struct localtime watch_time = {
	.seconds = 0,
	.hours = 1,
	.minutes = 1,
};

enum state {
	JUST_SHOW_TIME,
	SET_HOURS,
	SET_MINUTES,
};

static volatile enum state watch_state = JUST_SHOW_TIME;
static volatile uint8_t timer_overflows_with_button_pressed = 0;
static volatile bool show_leds_to_set = false;

static void show_time(const struct localtime);

static uint8_t current_timer_value();
static uint16_t time_difference_in_ms(uint8_t timer_value_from, uint8_t timer_value_to, uint8_t timer_overflows);
static void set_timer2_prescaler(uint16_t prescaler);


static void normalize_time(struct localtime *time) {
	while(time->seconds >= 60) {
		time->seconds -= 60;
		time->minutes += 1;
	}
	while(time->minutes >= 60) {
		time->minutes -= 60;
		time->hours += 1;
	}
	if(time->hours > 12) {
		time->hours = 1;
	}
	if(time->hours == 0) {
		time->hours = 12;
	}
}

static void increment_time(struct localtime *time) {
	time->seconds += second_increment;
	normalize_time(time);
}


static void init_lid_pin_interrupt() {
	uint8_t tmp = EICRA;
	// Any logical change on INT1 generates an interrupt request.
	tmp |= _BV(ISC10);
	tmp &= ~_BV(ISC11);
	EICRA = tmp;
	
	EIMSK |= _BV(INT1);
}

static void init_lid_pin() {
	DDRD &= ~_BV(PD3);
	// PORTD |= _BV(PD3); // replaced with external pullup
	init_lid_pin_interrupt();
}

static bool lid_is_open() {
	return !!(PIND & _BV(PD3));
}

static void init_button_pin_interrupt() {
	uint8_t tmp = EICRA;
	// Any logical change on INT0 generates an interrupt request.
	tmp |= _BV(ISC00);
	tmp &= ~_BV(ISC01);
	EICRA = tmp;
	
	EIMSK |= _BV(INT0);
}

static void init_button_pin() {
	DDRD &= ~_BV(PD2);
	// PORTD |= _BV(PD2); // replaced with external pullup
	init_button_pin_interrupt();
}

static bool button_is_down() {
	return !(PIND & _BV(PD2));
}

static bool button_is_up() {
	return !button_is_down();
}

static void lid_closed_action() {
	watch_state = JUST_SHOW_TIME;
	set_timer2_prescaler(TIME_COUNTING_PRESCALER);
	leds_off();
}

static void switch_time_setting_state() {
	switch(watch_state) {
		case JUST_SHOW_TIME:
			watch_state = SET_HOURS;
			set_timer2_prescaler(TIME_SETTING_PRESCALER);
			show_leds_to_set = false;
			return;
		case SET_HOURS:
			watch_state = SET_MINUTES;
			set_timer2_prescaler(TIME_SETTING_PRESCALER);
			show_leds_to_set = false;
			return;
		case SET_MINUTES:
			watch_state = JUST_SHOW_TIME;
			set_timer2_prescaler(TIME_COUNTING_PRESCALER);
			if(lid_is_open()) {
				show_time(watch_time);
			}
			return;
	};
	
	// reset seconds on every state change
	watch_time.seconds = 0;
}

static void long_press_action() {
	switch_time_setting_state();
}

static uint8_t increment_with_overflow(uint8_t value, uint8_t maxValue, uint8_t minValue) {
	value += 1;
	if(value > maxValue) {
		value = minValue;
	}
	return value;
}

static void time_setting_increment_hours() {
	watch_time.hours = increment_with_overflow(watch_time.hours, 12, 1);
	show_time(watch_time);
}

static void time_setting_increment_minutes() {
	watch_time.minutes = increment_with_overflow(watch_time.minutes, 59, 0);
	show_time(watch_time);
}

static void short_press_action() {
	switch(watch_state) {
		case JUST_SHOW_TIME:
			// do intentionally nothing
			return;
		case SET_HOURS:
			time_setting_increment_hours();
			return;
		case SET_MINUTES:
			time_setting_increment_minutes();
			return;
	};
}

static void button_up_action(uint8_t down_timer_value, uint8_t up_timer_value) {
	const uint16_t milliseconds_pressed = time_difference_in_ms(down_timer_value, up_timer_value, timer_overflows_with_button_pressed);
	
	if(milliseconds_pressed < 180) {
		// too short, do nothing
		return;
	} else if(milliseconds_pressed < 1000) {
		// short press
		short_press_action();
	} else if(milliseconds_pressed < 4000) {
		// uncanny valley between short and long, do nothing
	} else if(milliseconds_pressed < 10000) {
		// long press
		long_press_action();
	} else {
		// too long, do nothing
	}
}

// interrupt of button pin
ISR(INT0_vect) {
	if(!lid_is_open()) {
		// lid is closed, do nothing and reset every time setting states
		lid_closed_action();
		return;
	}

	static uint8_t button_down_at = 0;
	if(button_is_down()) {
		button_down_at = current_timer_value();
	} else {
		uint8_t button_up_at = current_timer_value();
		button_up_action(button_down_at, button_up_at);
		
		// set button_down_at, so that when multiple "up"-interrupts in a row happen,
		// they do not use the same "down"-interrupt tick value
		button_down_at = button_up_at;
		timer_overflows_with_button_pressed = 0;
	}
}

// interrupt of lid pin
ISR(INT1_vect) {
	if(lid_is_open()) {
		show_time(watch_time);
	} else {
		lid_closed_action();
	}
}

static void timer2_write_zero() {
	OCR2A = 0; 
	loop_until_bit_is_clear(ASSR, OCR2AUB);
}

static void timer_overflow_action_in_show_time_mode() {
	// typecast to call this function despite volatile qualifier.
	increment_time((struct localtime*) &watch_time);
	
	if(lid_is_open()) {
		show_time(watch_time);
	} else {
		lid_closed_action();
	}
}

static void timer_overflow_action_in_set_hours_mode() {
	if(show_leds_to_set) {
		led_show_hours(watch_time.hours);
	} else {
		led_hours_off();
	}
	led_show_minutes(watch_time.minutes);
	
	show_leds_to_set = !show_leds_to_set;
}


static void timer_overflow_action_in_set_minutes_mode() {
	led_show_hours(watch_time.hours);
	if(show_leds_to_set) {
		led_show_minutes(watch_time.minutes);
	} else {
		led_minutes_off();
	}
	
	show_leds_to_set = !show_leds_to_set;
}

static void timer_overflow_action() {
	switch(watch_state) {
		case JUST_SHOW_TIME:
			timer_overflow_action_in_show_time_mode();
			return;
		case SET_HOURS:
			timer_overflow_action_in_set_hours_mode();
			return;
		case SET_MINUTES:
			timer_overflow_action_in_set_minutes_mode();
			return;
	};
}

ISR(TIMER2_OVF_vect) {
	timer2_write_zero();
	
	timer_overflow_action();
	
	if(button_is_down()) {
		timer_overflows_with_button_pressed += 1;
	} else {
		timer_overflows_with_button_pressed = 0;
	}
}

static void show_time(const struct localtime time) {
	led_show_time(time.hours, time.minutes);
}

static bool all_bits_are_set(uint8_t value, uint8_t mask) {
	return (value & mask) == mask;
}

static uint16_t current_timer_prescaler() {
	const uint8_t control_register_value = TCCR2B;
	if(all_bits_are_set(control_register_value, _BV(CS22) | _BV(CS21) | _BV(CS20))) {
		return 1024;
	} else if(all_bits_are_set(control_register_value, _BV(CS22) | _BV(CS21))) {
		return 256;
	} else if(all_bits_are_set(control_register_value, _BV(CS22) | _BV(CS20))) {
		return 128;
	} else if(all_bits_are_set(control_register_value, _BV(CS22))) {
		return 64;
	} else if(all_bits_are_set(control_register_value, _BV(CS21) | _BV(CS20))) {
		return 32;
	} else if(all_bits_are_set(control_register_value, _BV(CS20))) {
		return 8;
	} else {
		return 0;
	}
}

static void set_timer2_prescaler(uint16_t prescaler) {
	if(current_timer_prescaler() == prescaler) {
		// prescaler already set, we do nothing
		return;
	}
	
	uint8_t new_prescaler_bits = 0;
	
	switch(prescaler) {
	case 0:
		new_prescaler_bits = 0;
		break;
	case 8:
		new_prescaler_bits = _BV(CS20);
		break;
	case 32:
		new_prescaler_bits = _BV(CS21) | _BV(CS20);
		break;
	case 64:
		new_prescaler_bits = _BV(CS22);
		break;
	case 128:
		new_prescaler_bits = _BV(CS22) | _BV(CS20);
		break;
	case 256:
		new_prescaler_bits = _BV(CS22) | _BV(CS21);
		break;
	case 1024:
		new_prescaler_bits = _BV(CS22) | _BV(CS21) | _BV(CS20);
		break;
	default:
		return;
	}
	
	TCNT2 = 0;
	
	TCCR2B = (TCCR2B & ~(_BV(CS22) | _BV(CS21) | _BV(CS20))) | new_prescaler_bits;
	loop_until_bit_is_clear(ASSR, TCR2BUB);
}

static void init_timer2(uint16_t prescaler) {
	GTCCR |= _BV(TSM) | _BV(PSRASY);  // Stop timer and reset prescaler
	ASSR |= _BV(AS2); // Set asynchronous mode
	set_timer2_prescaler(prescaler);
	TIMSK2 |= _BV(TOIE2); // Enable overflow-interrupt
	GTCCR &= ~_BV(TSM); // start timer
}

static uint8_t current_timer_value() {
	return TCNT2;
}

static uint16_t time_difference_in_ms(uint8_t timer_value_from, uint8_t timer_value_to, uint8_t timer_overflows) {
	const uint16_t prescaler = current_timer_prescaler();
	if(prescaler == 0) {
		return 0;
	}
	
	const uint16_t quartz_frequency = 32768;
	const uint16_t timer_overflow_ticks = 256;
	const uint32_t ticks_total = (timer_overflow_ticks * timer_overflows) + timer_value_to - timer_value_from;
	const uint16_t milliseconds = (ticks_total * 1000) / (quartz_frequency / prescaler);
	return milliseconds;
}

static void disable_adc() {
	ACSR |= _BV(ACD);
}

static void blink_on_startup() {
	const uint8_t iterations = 3;
	for(uint8_t i = 0; i < iterations; ++i) {
		leds_on();
		_delay_ms(200);
		leds_off();
		_delay_ms(200);
	}
}

int main (void) {
	led_init_outputs();
	init_lid_pin();
	init_button_pin();
	disable_adc();
	init_timer2(TIME_COUNTING_PRESCALER);
	
	blink_on_startup();
	
	sei();
	
	while(1) {
		// dummy write to prevent uninterruptible sleep mode
		timer2_write_zero();
		
		set_sleep_mode(SLEEP_MODE_PWR_SAVE);
		sleep_mode();
	}
	
	return 0;
}
