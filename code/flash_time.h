#pragma once

#include <stdint.h>

#include "localtime.h"

void load_time_from_eeprom(struct localtime *time);
