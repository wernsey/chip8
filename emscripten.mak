
# Makefile for emscripten
CC=emcc

# `-s WASM=0` seems a safe default at the moment
# `-s NO_EXIT_RUNTIME=0` is because of the call to `exit()` in `exit_error()`
CFLAGS=-c -Werror -Wall -DABGR=1 
LDFLAGS= -s WASM=0 -s NO_EXIT_RUNTIME=0

ifeq ($(BUILD),debug)
  # Debug
  CFLAGS += -O0 -g 
  LDFLAGS +=
else
  # Release mode
  CFLAGS += -O2 -DNDEBUG
  LDFLAGS += 
endif

all: chip8.js

# http://stackoverflow.com/a/1400184/115589
THIS_FILE = $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))

debug:
	make -f $(THIS_FILE) BUILD=debug

chip8.js: pocadv.o render-sdl.o chip8.o bmp.o
	$(CC) $^ $(LDFLAGS) -o $@ --preload-file GAMES/ --preload-file GAMES/*

.c.o:
	$(CC) $(CFLAGS) $< -o $@

chip8.o: chip8.c chip8.h

bmp.o: bmp.c bmp.h

pocadv.o: pocadv.c pocadv.h bmp.h

render-sdl.o: render.c chip8.h pocadv.h bmp.h
	$(CC) $(CFLAGS) $< -o $@

.PHONY : clean

clean:
	-rm -f *.o
	-rm -f chip8.data chip8.js
