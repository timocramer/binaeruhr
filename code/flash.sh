#!/bin/bash

BINARY=binaeruhr.hex
FLASH_TIME_FILE=flash_time.bin

# if you want to flash a distinct time instead of the current time, fill in this variable.
# The format is "hours minutes seconds", without leading zeroes. For example, if you want
# to write 14:07:05, the line would be
# FLASH_TIME="14 7 5"
FLASH_TIME=""

function dec2bin() {
    printf "%b" "$(printf '\\x%02x' "$1")$(printf '\\x%02x' "$2")$(printf '\\x%02x' "$3")"
}

function writeFlashTime() {
    if [ ! -n "$FLASH_TIME" ]; then
        FLASH_TIME="$(date '+%-H %-M %-S')"
    fi
    
    dec2bin $FLASH_TIME >"$FLASH_TIME_FILE"
}

writeFlashTime

avrdude -p m48pa -c usbasp -U flash:w:${BINARY}:i -U eeprom:w:${FLASH_TIME_FILE}:r -U lfuse:w:0x62:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m
