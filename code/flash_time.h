#pragma once

#include <stdint.h>

#include "localtime.h"

void load_flash_time_from_eeprom(struct localtime *time);
