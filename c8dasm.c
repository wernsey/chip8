/* CHIP-8 Disassembler. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "chip8.h"

#define MAX_BRANCHES	64

#define REACHABLE(addr) (reachable[(addr) >> 3] & (1 << ((addr) & 0x07)))
#define SET_REACHABLE(addr) reachable[(addr) >> 3] |= (1 << ((addr) & 0x07))
#define IS_LABEL(addr) (labels[(addr) >> 3] & (1 << ((addr) & 0x07)))
#define SET_LABEL(addr) labels[(addr) >> 3] |= (1 << ((addr) & 0x07))

void c8_disasm() {
	/* Some of the reported errors can conceivably happen
		if you try to disassembe a buggy program */
	uint16_t branches[MAX_BRANCHES];
	int bsp = 0,
		odata = 0,
		out = 0;
	uint8_t reachable[TOTAL_RAM/8];
	uint16_t addr, max_addr = 0;
	uint8_t labels[TOTAL_RAM/8];

	memset(reachable, 0, sizeof reachable);
	memset(labels, 0, sizeof labels);

	/* Step 1: Determine which instructions are reachable.
	We run through the program instruction by instruction.
	If we encouter a branch, we push one path onto a stack and
	continue along the other. We continue until everything is
	marked as reachable and/or the stack is empty.
	Also, we mark branch destinations as labels to prettify the
	output - to have labels instead of adresses.
	*/
	branches[bsp++] = PROG_OFFSET;
	while(bsp > 0) {
		addr = branches[--bsp];

		while(addr < TOTAL_RAM - 1 && !REACHABLE(addr)) {

			SET_REACHABLE(addr);

			uint16_t opcode = c8_opcode(addr);
			if(addr < PROG_OFFSET || !opcode) {
				/* Program ended up where it shouldn't; assumes the RAM is initialised to 0 */
				c8_message("error: bad jump: program at #%03X\n",addr);
				return;
			}

			addr += 2;
			if(addr >= TOTAL_RAM) {
				c8_message("error: program overflows RAM\n");
				return;
			}

			uint16_t nnn = opcode & 0x0FFF;

			if(opcode == 0x00EE) { /* RET */
				break;
			} else if((opcode & 0xF000) == 0x1000) { /* JP addr */
				addr = nnn;
				assert(addr < TOTAL_RAM);
				SET_LABEL(addr);
			} else if((opcode & 0xF000) == 0x2000) { /* CALL addr */
				if(bsp == MAX_BRANCHES) {
					/* Basically, this program is too complex to disassemble,
						but you can increase MAX_BRANCHES to see if it helps. */
					c8_message("error: Too many branches to follow (%u)\n", bsp);
					return;
				}
				branches[bsp++] = addr; /* For the RET */
				addr = nnn;
				assert(addr < TOTAL_RAM);
				SET_LABEL(addr);
			} else if((opcode & 0xF000) == 0x3000 || (opcode & 0xF00F) == 0x5000) { /* SE */
				if(bsp == MAX_BRANCHES) {c8_message("error: Too many branches to follow (%u)\n", bsp);return;}
				branches[bsp++] = addr + 2;
			} else if((opcode & 0xF000) == 0x4000 || (opcode & 0xF00F) == 0x9000) { /* SNE */
				if(bsp == MAX_BRANCHES) {c8_message("error: Too many branches to follow (%u)\n", bsp);return;}
				branches[bsp++] = addr + 2;
			} else if((opcode & 0xF0FF) == 0xE09E) { /* SKP */
				if(bsp == MAX_BRANCHES) {c8_message("error: Too many branches to follow (%u)\n", bsp);return;}
				branches[bsp++] = addr + 2;
			} else if((opcode & 0xF0FF) == 0xE0A1) { /* SKNP */
				if(bsp == MAX_BRANCHES) {c8_message("error: Too many branches to follow (%u)\n", bsp); return;}
				branches[bsp++] = addr + 2;
			} else if((opcode & 0xF000) == 0xB000) {
				/* I don't think we can realistically disassemble this, because
					we can't know V0 without actually running the program. */
				break;
			}
		}
		if(addr >= TOTAL_RAM - 1) {
			c8_message("error: program overflows RAM\n");
			return;
		}
	}

	/* Find the largest non-null address so that we don't write a bunch of
		unnecessary zeros at the end of our output */
	for(max_addr = TOTAL_RAM - 1; c8_get(max_addr) == 0; max_addr--);

	/* Step 2: Loop through all the reachable instructions and print them. */
	for(addr = PROG_OFFSET; addr < TOTAL_RAM; addr += REACHABLE(addr)?2:1) {
		/* The REACHABLE(addr)?2:1 above is to handle non-aligned instructions properly. */
		char buffer[64];

		if(!REACHABLE(addr)) {
			if(addr <= max_addr) {
				/* You've reached data that is non-null, but not code either. */
				if(!odata++) {
					c8_message("L%03X: db #%02X", addr, c8_get(addr));
				} else {
					c8_message(", #%02X", c8_get(addr));
					if(odata % 4 == 0) {
						c8_message("\n");
						odata = 0;
					}
				}
			}
			out = 0;
			continue;
		}

		uint16_t opcode = c8_opcode(addr);

		buffer[0] = '\0';

		uint8_t x = (opcode >> 8) & 0x0F;
		uint8_t y = (opcode >> 4) & 0x0F;
		uint8_t nibble = opcode & 0x0F;
		uint16_t nnn = opcode & 0x0FFF;
		uint8_t kk = opcode & 0xFF;

		switch(opcode & 0xF000) {
			case 0x0000:
				if(opcode == 0x00E0) sprintf(buffer,"CLS");
				else if(opcode == 0x00EE) sprintf(buffer,"RET");
				else if((opcode & 0xFFF0) == 0x00C0) sprintf(buffer,"SCD  #%01X", nibble);
				else if(opcode == 0x00FB) sprintf(buffer,"SCR");
				else if(opcode == 0x00FC) sprintf(buffer,"SCL");
				else if(opcode == 0x00FD) sprintf(buffer,"EXIT");
				else if(opcode == 0x00FE) sprintf(buffer,"LOW");
				else if(opcode == 0x00FF) sprintf(buffer,"HIGH");
			break;
			case 0x1000: sprintf(buffer,"JP   L%03X", nnn); break;
			case 0x2000: sprintf(buffer,"CALL L%03X", nnn); break;
			case 0x3000: sprintf(buffer,"SE   V%1X, #%02X", x, kk); break;
			case 0x4000: sprintf(buffer,"SNE  V%1X, #%02X", x, kk); break;
			case 0x5000: sprintf(buffer,"SE   V%1X, V%1X", x, y); break;
			case 0x6000: sprintf(buffer,"LD   V%1X, #%02X", x, kk); break;
			case 0x7000: sprintf(buffer,"ADD  V%1X, #%02X", x, kk); break;
			case 0x8000: {
				switch(nibble) {
					case 0x0: sprintf(buffer,"LD   V%1X, V%1X", x, y); break;
					case 0x1: sprintf(buffer,"OR   V%1X, V%1X", x, y); break;
					case 0x2: sprintf(buffer,"AND  V%1X, V%1X", x, y); break;
					case 0x3: sprintf(buffer,"XOR  V%1X, V%1X", x, y); break;
					case 0x4: sprintf(buffer,"ADD  V%1X, V%1X", x, y); break;
					case 0x5: sprintf(buffer,"SUB  V%1X, V%1X", x, y); break;
					case 0x6: sprintf(buffer,"SHR  V%1X, V%1X", x, y); break;
					case 0x7: sprintf(buffer,"SUBN V%1X, V%1X", x, y); break;
					case 0xE: sprintf(buffer,"SHL  V%1X, V%1X", x, y); break;
				}
			} break;
			case 0x9000: sprintf(buffer,"SNE  V%1X, V%1X", x, y); break;
			case 0xA000: sprintf(buffer,"LD   I,  #%03X", nnn); break;
			case 0xB000: sprintf(buffer,"JP   V0, #%03X", nnn); break;
			case 0xC000: sprintf(buffer,"RND  V%1X, #%02X", x, kk); break;
			case 0xD000: sprintf(buffer,"DRW  V%1X, V%1X, #%01X", x, y, nibble); break;
			case 0xE000: {
				if(kk == 0x9E) {
					sprintf(buffer,"SKP V%1X", x);
				} else if(kk == 0xA1) {
					sprintf(buffer,"SKNP V%1X", x);
				}
			} break;
			case 0xF000: {
				switch(kk) {
					case 0x07: sprintf(buffer,"LD   V%1X, DT", x); break;
					case 0x0A: sprintf(buffer,"LD   V%1X, K", x); break;
					case 0x15: sprintf(buffer,"LD   DT, V%1X", x); break;
					case 0x18: sprintf(buffer,"LD   ST, V%1X", x); break;
					case 0x1E: sprintf(buffer,"ADD  I,  V%1X", x); break;
					case 0x29: sprintf(buffer,"LD   F,  V%1X", x); break;
					case 0x33: sprintf(buffer,"LD   B,  V%1X", x); break;
					case 0x55: sprintf(buffer,"LD   [I], V%1X", x); break;
					case 0x65: sprintf(buffer,"LD   V%1X, [I]", x); break;
					case 0x30: sprintf(buffer,"LD   HF, V%1X", x); break;
					case 0x75: sprintf(buffer,"LD   R, V%1X", x); break;
					case 0x85: sprintf(buffer,"LD   V%1X, R", x); break;
				}
			} break;
		}
		if(!buffer[0]) {
			c8_message("error: Disassembler got confused at #%03X\n", addr);
			return;
		}
		if(IS_LABEL(addr) || !out) {
			if(odata) c8_message("\n");
			c8_message("L%03X: %-20s    ; %04X\n", addr, buffer, opcode);
		} else
			c8_message("      %-20s    ; %04X\n", buffer, opcode);
		out = 1;
		odata = 0;
	}
}
