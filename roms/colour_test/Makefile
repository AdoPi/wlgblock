default: all

all: colour_test.gbc

colour_test.gbc: colour_test.o
	rgblink -o $@ $^
	rgbfix -v -p0 $@

colour_test.o: colour_test.rgbasm font.rgbasm
	rgbasm -o $@ $<

clean:
	rm -f colour_test.o colour_test.gbc
