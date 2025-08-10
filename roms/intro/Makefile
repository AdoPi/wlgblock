default: all

all: intro.gbc

intro.gbc: intro.o bounce.o palettes.o font.o
	rgblink -t -o $@ $^
	rgbfix -v -p0 $@

intro.o: intro.rgbasm hardware.inc notes.inc
	rgbasm -o $@ $<

bounce.o: bounce.rgbasm
	rgbasm -o $@ $<

palettes.o: palettes.rgbasm
	rgbasm -o $@ $<

font.o: font.rgbasm
	rgbasm -o $@ $<

clean:
	rm -f *.o *.gbc *.sav *.s[0-9]
