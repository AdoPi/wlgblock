CC=gcc
CFLAGS=-Wall -Wextra -pedantic -O2

.PHONY: all
all: romgen bin2screen

romgen: romgen.c
	$(CC) -o $@ $< $(CFLAGS)

bin2screen: bin2screen.c
	$(CC) -o $@ $< $(CFLAGS)

.PHONY: clean
clean:
	rm -f romgen bin2screen
