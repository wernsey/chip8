# Makefile for emscripten

# The command
#   $ make -f emscripten.mak run
#
# will run `python3 -m http.server`
#
# You can then point your browser to
# http://localhost:8080/chip8.html

CC=emcc

# `-s NO_EXIT_RUNTIME=0` is because of the call to `exit()` in `exit_error()`
CFLAGS=-c -I. -Werror -Wall -DABGR=1
#LDFLAGS= -s WASM=0 -s NO_EXIT_RUNTIME=0
LDFLAGS= -s NO_EXIT_RUNTIME=0

SOURCES=sdl/pocadv.c render.c chip8.c bmp.c
OBJECTS=$(SOURCES:.c=.o)

OUTDIR=out
EXECUTABLE=$(OUTDIR)/chip8.js

ifeq ($(BUILD),debug)
  # Debug
  CFLAGS += -O0 -g
  LDFLAGS +=
else
  # Release mode
  CFLAGS += -O2 -DNDEBUG
  LDFLAGS +=
endif

all: $(EXECUTABLE)

# http://stackoverflow.com/a/1400184/115589
THIS_FILE = $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))

debug:
	make -f $(THIS_FILE) BUILD=debug

$(OUTDIR):
	-mkdir -p $(OUTDIR)
	-mkdir -p $(OUTDIR)/GAMES
	-cp -r GAMES/*.ch8 $(OUTDIR)/GAMES
	-cp -r examples/*.ch8 $(OUTDIR)/GAMES

$(EXECUTABLE): $(OUTDIR) $(OBJECTS) $(OUTDIR)/chip8.html
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(OUTDIR)/chip8.html: chip8.html
	cp chip8.html $(OUTDIR)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

pocadv.o: sdl/pocadv.c sdl/pocadv.h sdl/../app.h sdl/../bmp.h
render.o: render.c sdl/pocadv.h sdl/../app.h sdl/../bmp.h chip8.h bmp.h
chip8.o: chip8.c chip8.h
bmp.o: bmp.c bmp.h

.PHONY : clean run deps

deps: $(SOURCES)
	$(CC) -MM $(SOURCES)

run: $(EXECUTABLE)
	-cd $(OUTDIR) && python3 -m http.server

clean:
	-rm -f *.o sdl/*.o
	-rm -rf $(OUTDIR)

