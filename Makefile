# I built the windows version with MinGW

# Yes, it does actually compile with TCC:
#CC=C:\Tools\tcc\tcc.exe

CFLAGS=-c -Wall
LDFLAGS=
ifeq ($(BUILD),debug)
# Debug
CFLAGS += -O0 -g
LDFLAGS +=
else
# Release mode
CFLAGS += -O2 -DNDEBUG
LDFLAGS += -s
endif

all: c8asm.exe c8dasm.exe chip8.exe docs

debug:
	make BUILD=debug

c8asm.exe: asmmain.o c8asm.o chip8.o
	$(CC) $(LDFLAGS) -o $@ $^

c8dasm.exe: dasmmain.o c8dasm.o chip8.o
	$(CC) $(LDFLAGS) -o $@ $^

chip8.exe: gdi.o render.o chip8.o bmp.o
	$(CC) $(LDFLAGS) -o $@ $^ -mwindows

.c.o:
	$(CC) $(CFLAGS) $< -o $@

c8asm.o: c8asm.c chip8.h
c8dasm.o: c8dasm.c chip8.h
chip8.o: chip8.c chip8.h
bmp.o: bmp.c bmp.h
asmmain.o: asmmain.c chip8.h
dasmmain.o: dasmmain.c chip8.h
render.o: render.c chip8.h gdi.h bmp.h
gdi.o: gdi.c gdi.h bmp.h

docs: chip8.html

chip8.html: chip8.h comdown.awk
	awk -f comdown.awk -v Theme=7 chip8.h > $@

.PHONY : clean

clean:
	-rm -f *.o
	-rm -f chip8.html
	-rm -f c8asm.exe c8dasm.exe
	-rm -f chip8.exe
