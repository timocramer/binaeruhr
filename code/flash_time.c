#include "flash_time.h"

#include <avr/eeprom.h>

struct localtime load_time_from_eeprom() {
    struct localtime time;
    time.hours = eeprom_read_byte((uint8_t *)0);
    time.minutes = eeprom_read_byte((uint8_t *)1);
    time.seconds = eeprom_read_byte((uint8_t *)2);
    
    return normalize_time(time);
}
