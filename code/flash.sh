#!/bin/bash

BINARY=binaeruhr.hex
FLASH_TIME_FILE=flash_time.bin

# if you want to flash a distinct time instead of the current time, fill in this variable.
# The format is "hours minutes seconds", without leading zeroes. For example, if you want
# to write 14:07:05, the line would be
# FLASH_TIME="14 7 5"
FLASH_TIME=""

function dec2hex() {
    printf '%x' "$1"
}

function dec2bin() {
    printf '%b' "\\x$(dec2hex "$1")"
}

function writeFlashTime() {
    rm -f "$FLASH_TIME_FILE"
    
    if [ ! -n "$FLASH_TIME" ]; then
        FLASH_TIME="$(date '+%-H %-M %-S')"
    fi
    
    echo "$FLASH_TIME" | tr ' ' "\n" | while read val; do
        dec2bin $val >>"$FLASH_TIME_FILE"
    done
}

writeFlashTime

avrdude -p m48pa -c usbasp -U flash:w:${BINARY}:i -U eeprom:w:${FLASH_TIME_FILE}:r -U lfuse:w:0x62:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m
