#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "chip8.h"

static void usage(const char *name) {
	printf("usage: %s [options] infile.bin\n", name);
	printf("where options are:\n");
	printf(" -d             : Dump bytes\n");
	printf(" -a             : Dump bytes with addresses\n");
	printf(" -v             : Verbose mode\n");
}

int main(int argc, char *argv[]) {

	int opt, dump = 0;
	const char *infile = NULL;

	while((opt = getopt(argc, argv, "vda?")) != -1) {
		switch(opt) {
			case 'd': dump = 1; break;
			case 'a': dump = 2; break;
			case 'v': {
				c8_verbose++;
			} break;
			case '?' : {
				usage(argv[0]);
				return 1;
			}
		}
	}
	if(optind >= argc) {
        usage(argv[0]);
        return 1;
    }
	infile = argv[optind++];

	c8_reset();

	if(!c8_load_file(infile)) {
		fprintf(stderr, "error: Unable to load %s\n", infile);
		return 1;
	}

	if(dump == 0) {
		c8_disasm();
	} else {
		uint16_t pc;
		for(pc = PROG_OFFSET; pc < c8_prog_size(); pc += 2) {
			uint16_t op = c8_opcode(pc);
			if(dump == 2) {
				c8_message("%03X: %04X\n", pc, op);
			} else {
				c8_message("%04X\n", op);
			}
		}
	}
	return 0;
}
