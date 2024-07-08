CC=avr-gcc
OBJCOPY=avr-objcopy

CFLAGS=-Os -Wall -Wextra -Wshadow -DF_CPU=1000000 -mmcu=atmega48pa --std=c23
LDFLAGS=

BINARY=binaeruhr.hex
HEADERS=led.h flash_time.h localtime.h

.PHONY: all clean

all: $(BINARY)

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $(CFLAGS) $<

%.elf: binaeruhr.o led.o flash_time.o
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

%.hex: %.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

clean:
	$(RM) *.o *.elf *.hex flash_time.bin
