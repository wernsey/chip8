#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#include "getopt.h"
#include "bmp.h"
#include "chip8.h"

int main(int argc, char *argv[]) {
	srand(time(NULL));

	c8_verbose = 2;

	const char *infile = argv[optind++];

	if(c8_verbose)
		c8_message("Reading input from '%s'...\n", infile);

	char *text = c8_load_txt(infile);
	if(!text) {
		c8_message("error: unable to read '%s'\n", infile);
		return 1;
	}
	if(c8_verbose)
		c8_message("Input read.\n");

	c8_reset();

	c8_assemble(text);

	free(text);

	Bitmap *bm = bm_create(128,64);
	bm_set_color(bm, 0x777777);
	bm_clear(bm);

	while(!c8_ended()) {
		uint16_t pc = c8_get_pc();
		c8_message("%03X: %04X\n", pc, c8_opcode(pc));
		c8_step();
		if(c8_screen_updated()) {
			int x,y, w, h;
			c8_resolution(&w, &h);
			for(y = 0; y < h; y++) {
				for(x = 0; x < w; x++) {
					if(c8_get_pixel(x,y))
						bm_set(bm, x,y,0xFFFFFF);
					else
						bm_set(bm, x,y,0x000000);
				}
			}
		}
	}

	bm_save(bm, "out.bmp");
	bm_free(bm);
	return 0;
}