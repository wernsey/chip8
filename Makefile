# Makefile for Linux and Windows (MinGW)
CC=gcc
CFLAGS=-c -Wall
LDFLAGS=-lm

# Detect operating system:
# More info: http://stackoverflow.com/q/714100
ifeq ($(OS),Windows_NT)
  EXECUTABLES=chip8 chip8-gdi
  LDFLAGS+=-mwindows
else
  EXECUTABLES=chip8
endif

ifeq ($(BUILD),debug)
  # Debug
  CFLAGS += -O0 -g
  LDFLAGS +=
else
  # Release mode
  CFLAGS += -O2 -DNDEBUG
  LDFLAGS += -s
endif

all: c8asm c8dasm $(EXECUTABLES) docs example

debug:
	make BUILD=debug

c8asm: asmmain.o c8asm.o chip8.o
	$(CC) $(LDFLAGS) -o $@ $^

c8dasm: dasmmain.o c8dasm.o chip8.o
	$(CC) $(LDFLAGS) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) $< -o $@

asmmain.o: asmmain.c chip8.h
bmp.o: bmp.c bmp.h
c8asm.o: c8asm.c chip8.h
c8dasm.o: c8dasm.c chip8.h
chip8.o: chip8.c chip8.h
dasmmain.o: dasmmain.c chip8.h
render.o: render.c gdi/gdi.h gdi/../bmp.h gdi/../app.h chip8.h bmp.h
gdi.o: gdi/gdi.c gdi/../bmp.h gdi/gdi.h gdi/../app.h
pocadv.o: sdl/pocadv.c sdl/pocadv.h sdl/../app.h sdl/../bmp.h

# SDL specific:
chip8: pocadv.o render-sdl.o chip8.o bmp.o
	$(CC) $^ $(LDFLAGS) `sdl2-config --libs` -o $@
render-sdl.o: render.c chip8.h sdl/pocadv.h app.h bmp.h
	$(CC) $(CFLAGS) -DSDL2 `sdl2-config --cflags` $< -o $@
pocadv.o: sdl/pocadv.c sdl/pocadv.h app.h bmp.h
	$(CC) $(CFLAGS) -DSDL2 `sdl2-config --cflags` $< -o $@

# Example
example : examples/CUBE8.ch8
examples/CUBE8.ch8 : examples/cube.asm ./c8asm
	mkdir -p GAMES
	./c8asm -o $@ $<

# Windows GDI-version specific:
chip8-gdi: gdi.o render-gdi.o chip8.o bmp.o
	$(CC) $^ -o $@ $(LDFLAGS)
render-gdi.o: render.c chip8.h gdi/gdi.h app.h bmp.h
	$(CC) $(CFLAGS) -DGDI $< -o $@
gdi.o: gdi/gdi.c gdi/gdi.h app.h bmp.h
	$(CC) $(CFLAGS) -DGDI $< -o $@

# Documentation
docs: chip8-api.html README.html

chip8-api.html: chip8.h d.awk
	awk -f d.awk -v Theme=7 $< > $@

README.html: README.md d.awk
	awk -f d.awk -v Theme=7 -v Clean=1 $< > $@

.PHONY : clean wipe

wipe:
	-rm -f *.o sdl/*.o gdi/*.o

clean: wipe
	-rm -f c8asm chip8 c8dasm *.exe
	-rm -f chip8-api.html README.html
	-rm -f *.log *.bak
