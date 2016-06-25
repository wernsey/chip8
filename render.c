#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <windows.h>

#include "chip8.h"
#include "bmp.h"
#include "gdi.h"

static Bitmap *background;

static int speed = 10;
static int fg_color = 0xAAAAFF;
static int bg_color = 0x000055;
static int running = 1;

static FILE *logfile = NULL;
static int gdi_puts(const char* s) {
	if(!logfile)
		return 0;
	return fputs(s, logfile);
}

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

	c8_puts = gdi_puts;

	logfile = fopen("gdi.log", "w");
	c8_verbose++;

	c8_message("Initializing...\n");
	
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
		c8_message("No input file specified.\n");
        exit_error("You need to specify a CHIP-8 file.");
    }
	infile = argv[optind++];

	c8_message("Loading %s...\n", infile);
	if(!c8_load_file(infile)) {
		c8_message("Unable to load '%s': %s\n", infile, strerror(errno));
		exit_error("Unable to load '%s': %s\n", infile, strerror(errno));
	}

	bm_set_color(screen, 0x202020);
	bm_clear(screen);

	background = bm_create(128,64);
	int i, j, c;
	for(i = 0; i < 64; i++) {
		c = (32 - i);
		if(c < 0)
			c = -c;
		c = (32 - c) * 4;
		c = (c >> 3) << 3;
		c += 32;
		c = (c << 16) | (c << 8) | c;
		for(j = 0; j < 128; j++)
			bm_set(background, j, i, c);
	}
	bm_blit(screen, 0, 0, background, 0, 0, 128, 64);
	draw_screen();
	
	c8_message("Initialized.\n");
}

void deinit_game() {
	c8_message("Done.\n");
	fclose(logfile);
}

static void draw_screen() {
	static int last_res = 1;
	int x, y, w, h, ox = 0, oy = 0;
	int hi_res = c8_resolution(&w, &h);
	if(!hi_res) {
		ox = (128 - w)/2;
		oy = (64 - h)/2;
		if(last_res) {
			/* resolution changed back to lowres */
			bm_blit(screen, 0, 0, background, 0, 0, 128, 64);
		}
	}
	for(y = 0; y < h; y++) {
		for(x = 0; x < w; x++) {
			if(c8_get_pixel(x,y))
				bm_set(screen, x + ox, y + oy, fg_color);
			else
				bm_set(screen, x + ox, y + oy, bg_color);
		}
	}
	last_res = hi_res;
}

int render(double elapsedSeconds) {
	int i;
	static double timer = 0.0;

	/* These are the same keybindings Octo [10]'s  */
	int keymapping[16] = {
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

		for(i = 0; i < speed; i++) {			
			if(c8_ended())
				return 0;
			else if(c8_waitkey() && !key_pressed)
				return 1;

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
