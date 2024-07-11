#pragma once

#include <stdint.h>

struct localtime {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
};

struct localtime normalize_time(struct localtime time);
struct localtime increment_time(struct localtime time, uint8_t second_increment);
