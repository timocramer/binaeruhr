avrdude -p m48pa -c usbasp -U flash:w:binaeruhr.hex:i -U lfuse:w:0x62:m -U hfuse:w:0xdf:m -U efuse:w:0xff:m
