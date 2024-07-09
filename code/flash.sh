#!/bin/bash

BINARY=binaeruhr.hex
FLASH_TIME_FILE=flash_time.bin

function dec2hex() {
    printf '%x' "$1"
}

function dec2bin() {
    printf '%b' "\\x$(dec2hex "$1")"
}

function writeCurrentTime() {
    rm -f "$FLASH_TIME_FILE"
    
    date '+%-H %-M %-S' | tr ' ' "\n" | while read val; do
        dec2bin $val >>"$FLASH_TIME_FILE"
    done
}

writeCurrentTime

avrdude -p m48pa -c usbasp -U flash:w:${BINARY}:i -U eeprom:w:${FLASH_TIME_FILE}:r -U lfuse:w:0x62:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m
