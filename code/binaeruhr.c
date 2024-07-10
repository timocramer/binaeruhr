#include <stdint.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

#include "localtime.h"
#include "led.h"
#include "flash_time.h"

static const uint16_t TIME_SETTING_PRESCALER = 64;

#define TIME_COUNTING_PRESCALER 1024
#if TIME_COUNTING_PRESCALER < 128
    #error "TIME_COUNTING_PRESCALER needs to be at least 128"
#endif

static const uint16_t QUARTZ_FREQUENCY = 32768;
static const uint16_t TIMER_OVERFLOW_TICKS = 256;

// quartz frequency is 32768 Hz and timer has 8 bits, so a timer overflow happens every (prescaler / (32768 / 256)) seconds
static const uint8_t SECOND_INCREMENT = (uint16_t)TIME_COUNTING_PRESCALER / (QUARTZ_FREQUENCY / TIMER_OVERFLOW_TICKS);

// debug
// static const uint8_t SECOND_INCREMENT = 60;

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
static void set_timer2_prescaler(uint16_t prescaler, bool reset_timer_value);


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

static void set_time_showing_mode(bool reset_timer_value) {
    watch_state = JUST_SHOW_TIME;
    set_timer2_prescaler(TIME_COUNTING_PRESCALER, reset_timer_value);
}

static void lid_closed_action() {
    set_time_showing_mode(false);
    leds_off();
}

static void switch_time_setting_state() {
    switch(watch_state) {
    case JUST_SHOW_TIME:
        watch_state = SET_HOURS;
        set_timer2_prescaler(TIME_SETTING_PRESCALER, true);
        show_leds_to_set = false;
        break;
    case SET_HOURS:
        watch_state = SET_MINUTES;
        set_timer2_prescaler(TIME_SETTING_PRESCALER, true);
        show_leds_to_set = false;
        break;
    case SET_MINUTES:
        set_time_showing_mode(true);
        if(lid_is_open()) {
            show_time(watch_time);
        }
        break;
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
        break;
    case SET_HOURS:
        time_setting_increment_hours();
        break;
    case SET_MINUTES:
        time_setting_increment_minutes();
        break;
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

static void wait_for_finished_timer_operation() {
    while(ASSR & (_BV(TCN2UB) | _BV(OCR2AUB) | _BV(OCR2BUB) | _BV(TCR2AUB) | _BV(TCR2BUB))) {
        // do intentionally nothing
    }
}

static void timer2_write_zero() {
    OCR2A = 0;
    wait_for_finished_timer_operation();
}

static void timer_overflow_action_in_show_time_mode() {
    // work on a copy, so that the volatile qualifier is not discarded
    struct localtime copy = watch_time;
    increment_time(&copy, SECOND_INCREMENT);
    watch_time = copy;
    
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
        break;
    case SET_HOURS:
        timer_overflow_action_in_set_hours_mode();
        break;
    case SET_MINUTES:
        timer_overflow_action_in_set_minutes_mode();
        break;
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

static uint16_t current_timer_prescaler() {
    const uint8_t prescaler_bits = TCCR2B & (_BV(CS22) | _BV(CS21) | _BV(CS20));
    
    switch(prescaler_bits) {
    case (_BV(CS22) | _BV(CS21) | _BV(CS20)):
        return 1024;
    case (_BV(CS22) | _BV(CS21)):
        return 256;
    case (_BV(CS22) | _BV(CS20)):
        return 128;
    case (_BV(CS22)):
        return 64;
    case (_BV(CS21) | _BV(CS20)):
        return 32;
    case (_BV(CS21)):
        return 8;
    case (_BV(CS20)):
        return 1;
    default:
        return 0;
    }
}

// prescaler = 0: no clock source
// prescaler = 1: no prescaling
static void set_timer2_prescaler(uint16_t prescaler, bool reset_timer_value) {
    uint8_t new_prescaler_bits = 0;
    
    switch(prescaler) {
    case 0:
        new_prescaler_bits = 0;
        break;
    case 1:
        new_prescaler_bits = _BV(CS20);
        break;
    case 8:
        new_prescaler_bits = _BV(CS21);
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
    
    if(reset_timer_value) {
        TCNT2 = 0;
        wait_for_finished_timer_operation();
    }
    
    const uint8_t current_register_value = TCCR2B;
    const uint8_t current_prescaler_bits = current_register_value & (_BV(CS22) | _BV(CS21) | _BV(CS20));
    
    if(current_prescaler_bits != new_prescaler_bits) {
        TCCR2B = (current_register_value & ~(_BV(CS22) | _BV(CS21) | _BV(CS20))) | new_prescaler_bits;
        wait_for_finished_timer_operation();
    }
}

static void reset_unused_timer_registers() {
    // the datasheet says after setting async mode, these registers (plus the ones set
    // in set_timer2_prescaler) can be corrupted. so we set them to default
    
    wait_for_finished_timer_operation();
    TCCR2A = 0x00;
    wait_for_finished_timer_operation();
    OCR2A = 0x00;
    wait_for_finished_timer_operation();
    OCR2B = 0x00;
    wait_for_finished_timer_operation();
}

static void init_timer2(uint16_t prescaler) {
    GTCCR |= _BV(TSM) | _BV(PSRASY);  // Stop timer and reset prescaler
    ASSR |= _BV(AS2); // Set asynchronous mode
    reset_unused_timer_registers();
    set_timer2_prescaler(prescaler, true);
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
    
    const uint32_t ticks_total = (TIMER_OVERFLOW_TICKS * timer_overflows) + timer_value_to - timer_value_from;
    const uint16_t milliseconds = (ticks_total * 1000) / (QUARTZ_FREQUENCY / prescaler);
    return milliseconds;
}

static void disable_analog_comparator() {
    ACSR |= _BV(ACD);
}

#define STARTUP_BLINK_DELAY_MS 80

static void startup_circle_blink(uint8_t iterations) {
    uint16_t bit_pattern = 1;
    
    for(uint8_t i = 0; i < iterations; ++i) {
        led_show_bit_pattern(bit_pattern);
        
        if(bit_pattern & 1024) {
            bit_pattern = (bit_pattern << 1) | 0;
        } else {
            bit_pattern = (bit_pattern << 1) | 1;
        }
        
        _delay_ms(STARTUP_BLINK_DELAY_MS);
    }
}

// this function should draw a clockwise circle starting with hour8, every call shows another LED, so we can see which part is stuck
static void startup_debug_blink() {
    static uint8_t startup_sequence_index = 0;
    
    led_show_bit_pattern((uint16_t)1 << startup_sequence_index);
    
    startup_sequence_index = increment_with_overflow(startup_sequence_index, 9, 0);
    _delay_ms(STARTUP_BLINK_DELAY_MS);
}

static void init_unused_pins() {
    // from the datasheet:
    // If some pins are unused, it is recommended to ensure that these pins have a defined level. [...]
    // The simplest method to ensure a defined level of an unused pin, is to enable the internal pull-up.
    DDRC &= ~(_BV(PC0) | _BV(PC1) | _BV(PC2) | _BV(PC3) | _BV(PC4));
    PORTC |= (_BV(PC0) | _BV(PC1) | _BV(PC2) | _BV(PC3) | _BV(PC4));
    DDRD &= ~(_BV(PD5) | _BV(PD6) | _BV(PD7));
    PORTD |= (_BV(PD5) | _BV(PD6) | _BV(PD7));
}

static void initialize_time_from_eeprom() {
    struct localtime init_watch_time;
    load_time_from_eeprom(&init_watch_time);
    normalize_time(&init_watch_time);
    watch_time = init_watch_time;
}

int main(void) {
    // pins need to be initialized before we can blink
    init_unused_pins();
    led_init_outputs();
    
    startup_circle_blink(20);
    
    startup_debug_blink();
    init_lid_pin();
    
    startup_debug_blink();
    init_button_pin();
    
    startup_debug_blink();
    disable_analog_comparator();
    
    startup_debug_blink();
    init_timer2(TIME_COUNTING_PRESCALER);
    
    startup_debug_blink();
    initialize_time_from_eeprom();
    
    startup_debug_blink();
    show_time(watch_time);
    
    sei();
    
    while(1) {
        // dummy write to prevent uninterruptible sleep mode
        timer2_write_zero();
        
        set_sleep_mode(SLEEP_MODE_PWR_SAVE);
        sleep_mode();
    }
    
    return 0;
}
