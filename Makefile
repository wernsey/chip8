# Makefile for Linux and Windows (MinGW)

# Yes, it does actually compile with TCC:
#CC=C:\Tools\tcc\tcc.exe

# Detect operating system:
# More info: http://stackoverflow.com/q/714100
ifeq ($(OS),Windows_NT)
  EXECUTABLES=chip8-gdi chip8-sdl
else
  EXECUTABLES=chip8
endif

CFLAGS=-c -Wall
LDFLAGS=-lm

ifeq ($(BUILD),debug)
# Debug
CFLAGS += -O0 -g -I/local/include
LDFLAGS +=
else
# Release mode
CFLAGS += -O2 -DNDEBUG -I/local/include
LDFLAGS += -s
endif

all: c8asm c8dasm $(EXECUTABLES) docs

debug:
	make BUILD=debug

c8asm: asmmain.o c8asm.o chip8.o
	$(CC) $(LDFLAGS) -o $@ $^

c8dasm: dasmmain.o c8dasm.o chip8.o
	$(CC) $(LDFLAGS) -o $@ $^

# Windows GDI and SDL interpreter executables:
chip8-gdi: gdi.o render-gdi.o chip8.o bmp.o
	$(CC) $(LDFLAGS) -o $@ $^ -mwindows
	
chip8-sdl: pocadv.o render-sdl.o chip8.o bmp.o
	$(CC) $^ $(LDFLAGS) `sdl2-config --libs` -o $@  -mwindows

# Linux executable
chip8: pocadv.o render-sdl.o chip8.o bmp.o
	$(CC) $^ $(LDFLAGS) `sdl2-config --libs` -o $@ 

.c.o:
	$(CC) $(CFLAGS) $< -o $@

c8asm.o: c8asm.c chip8.h
c8dasm.o: c8dasm.c chip8.h
chip8.o: chip8.c chip8.h
bmp.o: bmp.c bmp.h
asmmain.o: asmmain.c chip8.h
dasmmain.o: dasmmain.c chip8.h

# Windows specific:
render-gdi.o: render.c chip8.h gdi.h bmp.h
	$(CC) $(CFLAGS) -DGDI $< -o $@
	
# SDL
render-sdl.o: render.c chip8.h pocadv.h bmp.h
	$(CC) $(CFLAGS) -DSDL2 `sdl2-config --cflags` $< -o $@

gdi.o: gdi.c gdi.h bmp.h

pocadv.o: pocadv.c pocadv.h bmp.h
	$(CC) $(CFLAGS) `sdl2-config --cflags` -I/local/include -DSDL2 $< -o $@

docs: chip8.html

chip8.html: chip8.h comdown.awk
	awk -f comdown.awk -v Theme=7 chip8.h > $@

.PHONY : clean

clean:
	-rm -f *.o
	-rm -f *.exe
	-rm -f chip8.html
	-rm -f *.log
