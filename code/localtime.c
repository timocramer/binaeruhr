#include "localtime.h"

void normalize_time(struct localtime *time) {
    while(time->seconds >= 60) {
        time->seconds -= 60;
        time->minutes += 1;
    }
    while(time->minutes >= 60) {
        time->minutes -= 60;
        time->hours += 1;
    }
    while(time->hours > 12) {
        time->hours -= 12;
    }
    if(time->hours == 0) {
        time->hours = 12;
    }
}

void increment_time(struct localtime *time, uint8_t second_increment) {
    time->seconds += second_increment;
    normalize_time(time);
}
