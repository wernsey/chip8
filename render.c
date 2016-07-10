#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#if defined(SDL2) || defined(SDL) || defined(__EMSCRIPTEN__)
#  include "pocadv.h"
#else
#  include <windows.h>
#  include "gdi.h"
#endif

#include "chip8.h"
#include "bmp.h"

/* number of instructions to execute per second */
static int speed = 1200;

/* Foreground color */
static int fg_color = 0xAAAAFF;

/* Background color */
static int bg_color = 0x000055;

/* Is the interpreter running? Set to 0 to enter "debug" mode */
static int running = 1;

/* These are the same keybindings Octo [10]'s  */
static unsigned int Key_Mapping[16] = {
#if defined(SDL) || defined(SDL2)
	KCODEA(x,X),
	KCODE(1),
	KCODE(2),
	KCODE(3),
	KCODEA(q,Q),
	KCODEA(w,W),
	KCODEA(e,E),
	KCODEA(a,A),
	KCODEA(s,S),
	KCODEA(d,D),
	KCODEA(z,Z),
	KCODEA(c,C),
	KCODE(4),
	KCODEA(r,R),
	KCODEA(f,F),
	KCODEA(v,V)
#else
	0x58, /* '0' -> 'x' */
	0x31, /* '1' -> '1' */
	0x32, /* '2' -> '2' */
	0x33, /* '3' -> '3' */
	0x51, /* '4' -> 'q' */
	0x57, /* '5' -> 'w' */
	0x45, /* '6' -> 'e' */
	0x41, /* '7' -> 'a' */
	0x53, /* '8' -> 's' */
	0x44, /* '9' -> 'd' */
	0x5A, /* 'A' -> 'z' */
	0x43, /* 'B' -> 'c' */
	0x34, /* 'C' -> '4' */
	0x52, /* 'D' -> 'r' */
	0x46, /* 'E' -> 'f' */
	0x56, /* 'F' -> 'v' */
#endif
};

static void draw_screen();

static void usage() {
	exit_error("Use these command line variables:\n"
				"  -f fg  : Foreground color\n"
				"  -b bg  : Background color\n"
				"  -s spd : Specify the speed\n"
				"  -d     : Debug mode\n"
				"  -v     : increase verbosity"
				);
}

void init_game(int argc, char *argv[]) {

	const char *infile = NULL;

	rlog("Initializing...");

	srand(time(NULL));

	c8_reset();

	fg_color = bm_byte_order(fg_color);
	bg_color = bm_byte_order(bg_color);

	int opt;
	while((opt = getopt(argc, argv, "f:b:s:dv?")) != -1) {
		switch(opt) {
			case 'v': c8_verbose++; break;
			case 'f': fg_color = bm_atoi(optarg); break;
			case 'b': bg_color = bm_atoi(optarg); break;
			case 's': speed = atoi(optarg); if(speed < 1) speed = 10; break;
			case 'd': running = 0; break;
			case '?' : {
				usage();
			}
		}
	}

	if(optind >= argc) {
        exit_error("You need to specify a CHIP-8 file.");
    }
	infile = argv[optind++];

	rlog("Loading %s...", infile);
	if(!c8_load_file(infile)) {
		exit_error("Unable to load '%s': %s", infile, strerror(errno));
	}

	bm_set_color(screen, 0x202020);
	bm_clear(screen);

	draw_screen();

#ifdef __EMSCRIPTEN__
	/* I Couldn't figure out why this is necessary on the emscripten port: */
	Key_Mapping[0] = KCODEA(x,X);
	Key_Mapping[1] = KCODE(1);
	Key_Mapping[2] = KCODE(2);
	Key_Mapping[3] = KCODE(3);
	Key_Mapping[4] = KCODEA(q,Q);
	Key_Mapping[5] = KCODEA(w,W);
	Key_Mapping[6] = KCODEA(e,E);
	Key_Mapping[7] = KCODEA(a,A);
	Key_Mapping[8] = KCODEA(s,S);
	Key_Mapping[9] = KCODEA(d,D);
	Key_Mapping[10] = KCODEA(z,Z);
	Key_Mapping[11] = KCODEA(c,C);
	Key_Mapping[12] = KCODE(4);
	Key_Mapping[13] = KCODEA(r,R);
	Key_Mapping[14] = KCODEA(f,F);
	Key_Mapping[15] = KCODEA(v,V);
#endif

	rlog("Initialized.");
}

void deinit_game() {
	rlog("Done.");
}

static void draw_screen() {
	int x, y, w, h, c, ox, oy;
	int hi_res = c8_resolution(&w, &h);
	if(!hi_res) {
		for(y = 0; y < h; y++) {
			for(x = 0; x < w; x++) {
				c = c8_get_pixel(x,y)?fg_color:bg_color;
				ox = x << 1; oy = y << 1;
				bm_set(screen, ox, oy, c);
				bm_set(screen, ox+1, oy, c);
				bm_set(screen, ox, oy+1, c);
				bm_set(screen, ox+1, oy+1, c);
			}
		}
	} else {
		for(y = 0; y < h; y++) {
			for(x = 0; x < w; x++) {
				c = c8_get_pixel(x,y)?fg_color:bg_color;
				bm_set(screen, x, y, c);
			}
		}
	}
}

int render(double elapsedSeconds) {
	int i;
	static double timer = 0.0;

	int key_pressed = 0;
	for(i = 0; i < 16; i++) {
		int k = Key_Mapping[i];
		if(keys[k]) {
			key_pressed = 1;
			c8_key_down(i);
#ifndef NDEBUG
			rlog("key pressed: %X 0x%02X", i, k);
#endif
		} else
			c8_key_up(i);
	}

	timer += elapsedSeconds;
	while(timer > 1.0/60.0) {
		c8_60hz_tick();
		timer -= 1.0/60.0;
	}

	if(running) {
		/* F5 breaks the program and enters debugging mode */
		if(keys[KCODE(F5)])
			running = 0;

		/* instructions per second * elapsed seconds = number of instructions to execute */
		int count = speed * elapsedSeconds;
		for(i = 0; i < count; i++) {
			if(c8_ended())
				return 0;
			else if(c8_waitkey() && !key_pressed)
				return 1;

			c8_step();

			if(c8_screen_updated())
				draw_screen();
		}
	} else {
		/* Debugging mode:
			F6 steps through the program
			F8 resumes
		*/
		if(keys[KCODE(F8)]) {
			bm_set_color(screen, 0x202020);
			bm_fillrect(screen, 0, screen->h - 24, screen->w, screen->h);
			running = 1;
			return 1;
		}
		if(keys[KCODE(F6)]) {
			if(c8_ended())
				return 0;
			else if(c8_waitkey() && !key_pressed)
				return 1;
			c8_step();
			if(c8_screen_updated()) {
				draw_screen();
			}
			keys[KCODE(F6)] = 0;
		}

		uint16_t pc = c8_get_pc();
		uint16_t opcode = c8_opcode(pc);
		bm_set_color(screen, 0x202020);
		bm_fillrect(screen, 0, screen->h - 24, screen->w, screen->h);
		bm_set_color(screen, 0xFFFFFF);
		bm_printf(screen, 1, screen->h - 24, "%03X %04X", pc, opcode);
		for(i = 0; i < 16; i++) {
			bm_printf(screen, (i & 0x07) * 16, (i >> 3) * 8 + screen->h - 16, "%02X", c8_get_reg(i));
		}
	}

	return 1;
}

int pointer_down(int x, int y, int id){return 0;}
int pointer_up(int x, int y, int id){return 0;}
int pointer_move(int x, int y, int id){return 0;}
int pointer_click(int x, int y, int id){return 0;}
int key_press(int code){return 0;}
