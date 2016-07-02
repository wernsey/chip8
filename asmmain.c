#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "chip8.h"

static void usage(const char *name) {
	printf("usage: %s [options] infile.asm\n", name);
	printf("where options are:\n");
	printf(" -o outfile     : Output file\n");
	printf(" -v             : Verbose mode\n");
}

int main(int argc, char *argv[]) {
	int opt;
	const char *infile = NULL;
	const char *outfile = "a.ch8";
	char *text;

	while((opt = getopt(argc, argv, "vo:?")) != -1) {
		switch(opt) {
			case 'v': {
				c8_verbose++;
			} break;
			case 'o': {
				outfile = optarg;
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

	if(c8_verbose)
		printf("Reading input from '%s'...\n", infile);

	text = c8_load_txt(infile);
	if(!text) {
		fprintf(stderr, "error: unable to read '%s'\n", infile);
		return 1;
	}

	if(c8_verbose)
		printf("Input read.\n");

	c8_reset();

	c8_assemble(text);

	if(c8_verbose)
		printf("Writing output to '%s'...\n", outfile);
	if(!c8_save_file(outfile)) {
		fprintf(stderr, "error: unable to write output to '%s': %s\n", outfile, strerror(errno));
		return 1;
	}
	if(c8_verbose)
		printf("Output written.\n");

	free(text);
	if(c8_verbose)
		printf("Success.\n");

	return 0;
}
