#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#ifdef SDL2
#  include "pocadv.h"
#else
#  include <windows.h>
#  include "gdi.h"
#endif

#include "chip8.h"
#include "bmp.h"

static int speed = 1200;
static int fg_color = 0xAAAAFF;
static int bg_color = 0x000055;
static int running = 1;

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

	int opt;
	while((opt = getopt(argc, argv, "f:b:s:dv?")) != -1) {
		switch(opt) {
			case 'v': c8_verbose++; break;
			case 'f': fg_color = bm_color_atoi(optarg); break;
			case 'b': bg_color = bm_color_atoi(optarg); break;
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
	
	/* These are the same keybindings Octo [10]'s  */
	int keymapping[16] = {
#ifdef SDL2
		0x1B, /* '0' -> 'x' */
		0x1E, /* '1' -> '1' */
		0x1F, /* '2' -> '2' */
		0x20, /* '3' -> '3' */
		0x14, /* '4' -> 'q' */
		0x1A, /* '5' -> 'w' */
		0x08, /* '6' -> 'e' */
		0x04, /* '7' -> 'a' */
		0x16, /* '8' -> 's' */
		0x07, /* '9' -> 'd' */
		0x1D, /* 'A' -> 'z' */
		0x06, /* 'B' -> 'c' */
		0x21, /* 'C' -> '4' */
		0x15, /* 'D' -> 'r' */
		0x09, /* 'E' -> 'f' */
		0x19, /* 'F' -> 'v' */
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

	int key_pressed = 0;
	for(i = 0; i < 16; i++) {
		int k = keymapping[i];
		if(keys[k]) {
			key_pressed = 1;
			c8_key_down(i);
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
		if(keys[KCODE(F5)]) {
			running = 0;
		}
		
		int count = speed * elapsedSeconds;			
		for(i = 0; i < count; i++) {			
			if(c8_ended())
				return 0;
			else if(c8_waitkey() && !key_pressed) {
				return 1;
			}

			c8_step();

			if(c8_screen_updated()) {
				draw_screen();
			}
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
