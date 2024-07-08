#!/bin/bash

OUTPUT_FILENAME=flash_time.bin

function dec2hex() {
    printf '%x' "$1"
}

function dec2bin() {
    printf '%b' "\\x$(dec2hex "$1")"
}

function writeCurrentTime() {
    rm -f "$OUTPUT_FILENAME"
    
    date '+%-H %-M %-S' | tr ' ' "\n" | while read val; do
        dec2bin $val >>"$OUTPUT_FILENAME"
    done
}

writeCurrentTime

avrdude -p m48pa -c usbasp -U flash:w:binaeruhr.hex:i -U eeprom:w:flash_time.bin:r -U lfuse:w:0x62:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m
