# Makefile for using TCC
# It is intended for the UnxUtils version of GNU make

#CC=C:\Tools\tcc\tcc.exe

# Add your source files here:
#SOURCES=c8asm.c bmp.c getopt.c
#OBJECTS=$(SOURCES:.c=.o)

CFLAGS=-c -Wall
LDFLAGS=

all: c8asm.exe c8dasm.exe gdi.exe runner.exe

# chip8.exe: chip8.o
#	$(CC) $(LDFLAGS) -o $@ $^

c8asm.exe: asmmain.o c8asm.o chip8.o getopt.o
	$(CC) $(LDFLAGS) -o $@ $^

c8dasm.exe: dasmmain.o c8dasm.o chip8.o getopt.o
	$(CC) $(LDFLAGS) -o $@ $^

gdi.exe: gdi.o render.o chip8.o bmp.o getopt.o
	$(CC) $(LDFLAGS) -o $@ $^ -mwindows

runner.exe: runner.o c8asm.o chip8.o getopt.o bmp.o
	$(CC) $(LDFLAGS) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) $< -o $@

c8asm.o: c8asm.c chip8.h
c8dasm.o: c8dasm.c chip8.h
chip8.o: chip8.c chip8.h
bmp.o: bmp.c bmp.h
asmmain.o: asmmain.c chip8.h getopt.h
dasmmain.o: dasmmain.c chip8.h getopt.h
render.o: render.c chip8.h gdi.h bmp.h getopt.h
gdi.o: gdi.c gdi.h bmp.h
runner.o: runner.c chip8.h getopt.h bmp.h

.PHONY : clean

clean:
	-rm -f *.o
	-rm -rf c8asm.exe c8dasm.exe runner.exe gdi.exe
