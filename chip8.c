/*
Core of the CHIP-8 interpreter.
This file should be kept platform independent. Everything that is
platform dependent should be moved elsewhere.


TODO: Apparently SuperChip 1.0 and 1.1 has a lot of caveats that
I haven't addressed in my implementation
https://chip-8.github.io/extensions/#super-chip-10
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>

#include "chip8.h"


static unsigned int quirks = QUIRKS_DEFAULT;

void c8_set_quirks(unsigned int q) {
	quirks = q;
}

unsigned int c8_get_quirks() {
	return quirks;
}

/* Where in RAM to load the font.
	The font should be in the first 512 bytes of RAM (see [2]),
	so FONT_OFFSET should be less than or equal to 0x1B0 */
#define FONT_OFFSET		0x1B0
#define HFONT_OFFSET	0x110

int c8_verbose = 0;

chip8_t C8;

/* Display memory */
static uint8_t pixels[1024];

static int yield = 0, borked = 0;

static int screen_updated; /* Screen updated */
static int hi_res; /* Hi-res mode? */

/* Keypad buffer */
static uint16_t keys;

/* HP48 flags for SuperChip Fx75 and Fx85 instructions */
static uint8_t hp48_flags[16];

/* Text output function */
char c8_message_text[MAX_MESSAGE_TEXT];
static int _puts_default(const char* s) {
	return fputs(s, stdout);
}
int (*c8_puts)(const char* s) = _puts_default;

int (*c8_rand)() = rand;

c8_sys_hook_t c8_sys_hook = NULL;

/* Standard 4x5 font */
static const uint8_t font[] = {
/* '0' */ 0xF0, 0x90, 0x90, 0x90, 0xF0,
/* '1' */ 0x20, 0x60, 0x20, 0x20, 0x70,
/* '2' */ 0xF0, 0x10, 0xF0, 0x80, 0xF0,
/* '3' */ 0xF0, 0x10, 0xF0, 0x10, 0xF0,
/* '4' */ 0x90, 0x90, 0xF0, 0x10, 0x10,
/* '5' */ 0xF0, 0x80, 0xF0, 0x10, 0xF0,
/* '6' */ 0xF0, 0x80, 0xF0, 0x90, 0xF0,
/* '7' */ 0xF0, 0x10, 0x20, 0x40, 0x40,
/* '8' */ 0xF0, 0x90, 0xF0, 0x90, 0xF0,
/* '9' */ 0xF0, 0x90, 0xF0, 0x10, 0xF0,
/* 'A' */ 0xF0, 0x90, 0xF0, 0x90, 0x90,
/* 'B' */ 0xE0, 0x90, 0xE0, 0x90, 0xE0,
/* 'C' */ 0xF0, 0x80, 0x80, 0x80, 0xF0,
/* 'D' */ 0xE0, 0x90, 0x90, 0x90, 0xE0,
/* 'E' */ 0xF0, 0x80, 0xF0, 0x80, 0xF0,
/* 'F' */ 0xF0, 0x80, 0xF0, 0x80, 0x80,
};

/* SuperChip hi-res 8x10 font. */
static const uint8_t hfont[] = {
/* '0' */ 0x7C, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x7C, 0x00,
/* '1' */ 0x08, 0x18, 0x38, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3C, 0x00,
/* '2' */ 0x7C, 0x82, 0x02, 0x02, 0x04, 0x18, 0x20, 0x40, 0xFE, 0x00,
/* '3' */ 0x7C, 0x82, 0x02, 0x02, 0x3C, 0x02, 0x02, 0x82, 0x7C, 0x00,
/* '4' */ 0x84, 0x84, 0x84, 0x84, 0xFE, 0x04, 0x04, 0x04, 0x04, 0x00,
/* '5' */ 0xFE, 0x80, 0x80, 0x80, 0xFC, 0x02, 0x02, 0x82, 0x7C, 0x00,
/* '6' */ 0x7C, 0x82, 0x80, 0x80, 0xFC, 0x82, 0x82, 0x82, 0x7C, 0x00,
/* '7' */ 0xFE, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x20, 0x20, 0x00,
/* '8' */ 0x7C, 0x82, 0x82, 0x82, 0x7C, 0x82, 0x82, 0x82, 0x7C, 0x00,
/* '9' */ 0x7C, 0x82, 0x82, 0x82, 0x7E, 0x02, 0x02, 0x82, 0x7C, 0x00,
/* 'A' */ 0x10, 0x28, 0x44, 0x82, 0x82, 0xFE, 0x82, 0x82, 0x82, 0x00,
/* 'B' */ 0xFC, 0x82, 0x82, 0x82, 0xFC, 0x82, 0x82, 0x82, 0xFC, 0x00,
/* 'C' */ 0x7C, 0x82, 0x80, 0x80, 0x80, 0x80, 0x80, 0x82, 0x7C, 0x00,
/* 'D' */ 0xFC, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0xFC, 0x00,
/* 'E' */ 0xFE, 0x80, 0x80, 0x80, 0xF8, 0x80, 0x80, 0x80, 0xFE, 0x00,
/* 'F' */ 0xFE, 0x80, 0x80, 0x80, 0xF8, 0x80, 0x80, 0x80, 0x80, 0x00,
};

void c8_reset() {
	memset(C8.V, 0, sizeof C8.V);
	memset(C8.RAM, 0, sizeof C8.RAM);
	C8.PC = PROG_OFFSET;
	C8.I = 0;
	C8.DT = 0;
	C8.ST = 0;
	C8.SP = 0;
	memset(C8.stack, 0, sizeof C8.stack);

	assert(FONT_OFFSET + sizeof font <= PROG_OFFSET);
	memcpy(C8.RAM + FONT_OFFSET, font, sizeof font);
	assert(HFONT_OFFSET + sizeof hfont <= FONT_OFFSET);
	memcpy(C8.RAM + HFONT_OFFSET, hfont, sizeof hfont);

	hi_res = 0;
	screen_updated = 0;
	yield = 0;
	borked = 0;
}

void c8_step() {
	assert(C8.PC < TOTAL_RAM);

	if(yield || borked) return;

	uint16_t opcode = C8.RAM[C8.PC] << 8 | C8.RAM[C8.PC+1];
	C8.PC += 2;

	uint8_t x = (opcode >> 8) & 0x0F;
	uint8_t y = (opcode >> 4) & 0x0F;
	uint8_t nibble = opcode & 0x0F;
	uint16_t nnn = opcode & 0x0FFF;
	uint8_t kk = opcode & 0xFF;

	int row, col;

	screen_updated = 0;

	switch(opcode & 0xF000) {
		case 0x0000:
			if(opcode == 0x00E0) {
				/* CLS */
				memset(pixels, 0, sizeof pixels);
				screen_updated = 1;
			} else if(opcode == 0x00EE) {
				/* RET */
				if(C8.SP == 0){
					/* You've got problems */
					borked = 1;
					return;
				}
				C8.PC = C8.stack[--C8.SP];
			} else if((opcode & 0xFFF0) == 0x00C0) {
				/* SCD nibble */
				c8_resolution(&col, &row);
				row--;
				col >>= 3;
				while(row - nibble >= 0) {
					memcpy(pixels + row * col, pixels + (row - nibble) * col, col);
					row--;
				}
				memset(pixels, 0x0, nibble * col);
				screen_updated = 1;
			} else if(opcode == 0x00FB) {
				/* SCR */
				c8_resolution(&col, &row);
				col >>= 3;
				for(y = 0; y < row; y++) {
					for(x = col - 1; x > 0; x--) {
						pixels[y * col + x] = (pixels[y * col + x] << 4) | (pixels[y * col + x - 1] >> 4);
					}
					pixels[y * col] <<= 4;
				}
				screen_updated = 1;
			} else if(opcode == 0x00FC) {
				/* SCL */
				c8_resolution(&col, &row);
				col >>= 3;
				for(y = 0; y < row; y++) {
					for(x = 0; x < col - 1; x++) {
						pixels[y * col + x] = (pixels[y * col + x] >> 4) | (pixels[y * col + x + 1] << 4);
					}
					pixels[y * col + x] >>= 4;
				}
				screen_updated = 1;
			} else if(opcode == 0x00FD) {
				/* EXIT */
				C8.PC -= 2; /* reset the PC to the 00FD */
				/* subsequent calls will encounter the 00FD again,
					and c8_ended() will return 1 */
				return;
			} else if(opcode == 0x00FE) {
				/* LOW */
				if(hi_res)
					screen_updated = 1;
				hi_res = 0;
			} else if(opcode == 0x00FF) {
				/* HIGH */
				if(!hi_res)
					screen_updated = 1;
				hi_res = 1;
			} else {
				/* SYS: If there's a hook, call it otherwise treat it as a no-op */
				if(c8_sys_hook) {
					int result = c8_sys_hook(nnn);
					if(!result)
						borked = 1;
				}
			}
		break;
		case 0x1000:
			/* JP nnn */
			C8.PC = nnn;
			break;
		case 0x2000:
			/* CALL nnn */
			if(C8.SP >= 16) return; /* See RET */
			C8.stack[C8.SP++] = C8.PC;
			C8.PC = nnn;
			break;
		case 0x3000:
			/* SE Vx, kk */
			if(C8.V[x] == kk) C8.PC += 2;
			break;
		case 0x4000:
			/* SNE Vx, kk */
			if(C8.V[x] != kk) C8.PC += 2;
			break;
		case 0x5000:
			/* SE Vx, Vy */
			if(C8.V[x] == C8.V[y]) C8.PC += 2;
			break;
		case 0x6000:
			/* LD Vx, kk */
			C8.V[x] = kk;
			break;
		case 0x7000:
			/* ADD Vx, kk */
			C8.V[x] += kk;
			break;
		case 0x8000: {
			uint16_t ans, carry;
			switch(nibble) {
				case 0x0:
					/* LD Vx, Vy */
					C8.V[x] = C8.V[y];
					break;
				case 0x1:
					/* OR Vx, Vy */
					C8.V[x] |= C8.V[y];
					if(quirks & QUIRKS_VF_RESET)
						C8.V[0xF] = 0;
					break;
				case 0x2:
					/* AND Vx, Vy */
					C8.V[x] &= C8.V[y];
					if(quirks & QUIRKS_VF_RESET)
						C8.V[0xF] = 0;
					break;
				case 0x3:
					/* XOR Vx, Vy */
					C8.V[x] ^= C8.V[y];
					if(quirks & QUIRKS_VF_RESET)
						C8.V[0xF] = 0;
					break;
				case 0x4:
					/* ADD Vx, Vy */
					ans = C8.V[x] + C8.V[y];
					C8.V[x] = ans & 0xFF;
					C8.V[0xF] = (ans > 255);
					break;
				case 0x5:
					/* SUB Vx, Vy */
					ans = C8.V[x] - C8.V[y];
					carry = (C8.V[x] > C8.V[y]);
					C8.V[x] = ans & 0xFF;
					C8.V[0xF] = carry;
					break;
				case 0x6:
					/* SHR Vx, Vy */
					if(!(quirks & QUIRKS_SHIFT))
						C8.V[x] = C8.V[y];
					carry = (C8.V[x] & 0x01);
					C8.V[x] >>= 1;
					C8.V[0xF] = carry;
					break;
				case 0x7:
					/* SUBN Vx, Vy */
					ans = C8.V[y] - C8.V[x];
					carry = (C8.V[y] > C8.V[x]);
					C8.V[x] = ans & 0xFF;
					C8.V[0xF] = carry;
					break;
				case 0xE:
					/* SHL Vx, Vy */
					if(!(quirks & QUIRKS_SHIFT))
						C8.V[x] = C8.V[y];
					carry = ((C8.V[x] & 0x80) != 0);
					C8.V[x] <<= 1;
					C8.V[0xF] = carry;
					break;
			}
		} break;
		case 0x9000:
			/* SNE Vx, Vy */
			if(C8.V[x] != C8.V[y]) C8.PC += 2;
			break;
		case 0xA000:
			/* LD I, nnn */
			C8.I = nnn;
			break;
		case 0xB000:
			/* JP V0, nnn */
			if(quirks & QUIRKS_JUMP)
				C8.PC = (nnn + C8.V[x]) & 0xFFF;
			else
				C8.PC = (nnn + C8.V[0]) & 0xFFF;
			break;
		case 0xC000:
			/* RND Vx, kk */
			C8.V[x] = c8_rand() & kk; /* FIXME: Better RNG? */
			break;
		case 0xD000: {
			/* DRW Vx, Vy, nibble */
			int mW, mH, W, H, p, q;
			int tx, ty, byte, bit, pix;

			/* TODO: [17] mentions that V[x] and V[y] gets modified by
			this instruction... */

			if(hi_res) {
				W = 128; H = 64; mW = 0x7F; mH = 0x3F;
			} else {
				W = 64; H = 32; mW = 0x3F; mH = 0x1F;
			}

			C8.V[0xF] = 0;
			if(nibble) {
				x = C8.V[x]; y = C8.V[y];
				x &= mW;
				y &= mH;
				for(q = 0; q < nibble; q++) {
					ty = (y + q);
					if((quirks & QUIRKS_CLIPPING) && (ty >= H))
						break;

					for(p = 0; p < 8; p++) {
						tx = (x + p);
						if((quirks & QUIRKS_CLIPPING) && (tx >= W))
							break;
						pix = (C8.RAM[C8.I + q] & (0x80 >> p)) != 0;
						if(pix) {
							tx &= mW;
							ty &= mH;
							byte = ty * W + tx;
							bit = 1 << (byte & 0x07);
							byte >>= 3;
							if(pixels[byte] & bit)
								C8.V[0x0F] = 1;
							pixels[byte] ^= bit;
						}
					}
				}
			} else {
				/* SCHIP mode has a 16x16 sprite if nibble == 0 */
				x = C8.V[x]; y = C8.V[y];
				x &= mW;
				y &= mH;
				for(q = 0; q < 16; q++) {
					ty = (y + q);
					if((quirks & QUIRKS_CLIPPING) && (ty >= H))
						break;

					for(p = 0; p < 16; p++) {
						tx = (x + p);
						if((quirks & QUIRKS_CLIPPING) && (tx >= W))
							break;

						if(p >= 8)
							pix = (C8.RAM[C8.I + (q * 2) + 1] & (0x80 >> (p & 0x07))) != 0;
						else
							pix = (C8.RAM[C8.I + (q * 2)] & (0x80 >> p)) != 0;
						if(pix) {
							byte = ty * W + tx;
							bit = 1 << (byte & 0x07);
							byte >>= 3;
							if(pixels[byte] & bit)
								C8.V[0x0F] = 1;
							pixels[byte] ^= bit;
						}
					}
				}
			}
			screen_updated = 1;
			if(quirks & QUIRKS_DISP_WAIT) {
				yield = 1;
			}
			} break;
		case 0xE000: {
			if(kk == 0x9E) {
				/* SKP Vx */
				if(keys & (1 << C8.V[x]))
					C8.PC += 2;
			} else if(kk == 0xA1) {
				/* SKNP Vx */
				if(!(keys & (1 << C8.V[x])))
					C8.PC += 2;
			}
		} break;
		case 0xF000: {
			switch(kk) {
				case 0x07:
					/* LD Vx, DT */
					C8.V[x] = C8.DT;
					break;
				case 0x0A: {
					/* LD Vx, K */
					if(!keys) {
						/* subsequent calls will encounter the Fx0A again */
						C8.PC -= 2;
						return;
					}
					for(y = 0; y < 0xF; y++) {
						if(keys & (1 << y)) {
							C8.V[x] = y;
							break;
						}
					}
					keys = 0;
				} break;
				case 0x15:
					/* LD DT, Vx */
					C8.DT = C8.V[x];
					break;
				case 0x18:
					/* LD ST, Vx */
					C8.ST = C8.V[x];
					break;
				case 0x1E:
					/* ADD I, Vx */
					C8.I += C8.V[x];
					/* According to [wikipedia][] the VF is set if I overflows. */
					if(C8.I > 0xFFF) {
						C8.V[0xF] = 1;
						C8.I &= 0xFFF;
					} else {
						C8.V[0xF] = 0;
					}
					break;
				case 0x29:
					/* LD F, Vx */
					C8.I = FONT_OFFSET + (C8.V[x] & 0x0F) * 5;
					break;
				case 0x30:
					/* LD HF, Vx - Load 8x10 hi-resolution font */
					C8.I = HFONT_OFFSET + (C8.V[x] & 0x0F) * 10;
					break;
				case 0x33:
					/* LD B, Vx */
					C8.RAM[C8.I] = (C8.V[x] / 100) % 10;
					C8.RAM[C8.I + 1] = (C8.V[x] / 10) % 10;
					C8.RAM[C8.I + 2] = C8.V[x] % 10;
					break;
				case 0x55:
					/* LD [I], Vx */
					if(C8.I + x > TOTAL_RAM)
						x = TOTAL_RAM - C8.I;
					assert(C8.I + x <= TOTAL_RAM);
					if(x >= 0)
						memcpy(C8.RAM + C8.I, C8.V, x+1);
					if(quirks & QUIRKS_MEM_CHIP8)
						C8.I += x + 1;
					break;
				case 0x65:
					/* LD Vx, [I] */
					if(C8.I + x > TOTAL_RAM)
						x = TOTAL_RAM - C8.I;
					assert(C8.I + x <= TOTAL_RAM);
					if(x >= 0)
						memcpy(C8.V, C8.RAM + C8.I, x+1);
					if(quirks & QUIRKS_MEM_CHIP8)
						C8.I += x + 1;
					break;
				case 0x75:
					/* LD R, Vx */
					assert(x <= sizeof hp48_flags);
					memcpy(hp48_flags, C8.V, x);
					break;
				case 0x85:
					/* LD Vx, R */
					assert(x <= sizeof hp48_flags);
					memcpy(C8.V, hp48_flags, x);
					break;
			}
		} break;
	}
}

int c8_ended() {
	/* Check whether the next instruction is 00FD */
	return borked || c8_opcode(C8.PC) == 0x00FD;
}
int c8_waitkey() {
	return (c8_opcode(C8.PC) & 0xF0FF) == 0xF00A;
}

uint8_t c8_get(uint16_t addr) {
	assert(addr < TOTAL_RAM);
	return C8.RAM[addr];
}

void c8_set(uint16_t addr, uint8_t byte) {
	assert(addr < TOTAL_RAM);
	C8.RAM[addr] = byte;
}

uint16_t c8_opcode(uint16_t addr) {
	assert(addr < TOTAL_RAM - 1);
	return C8.RAM[addr] << 8 | C8.RAM[addr+1];
}

uint16_t c8_get_pc() {
	return C8.PC;
}

uint16_t c8_prog_size() {
	uint16_t n;
	for(n = TOTAL_RAM - 1; n > PROG_OFFSET && C8.RAM[n] == 0; n--);
	if(++n & 0x1) // Fix for #4
		return n + 1;
	return n;
}

uint8_t c8_get_reg(uint8_t r) {
	if(r > 0xF) return 0;
	return C8.V[r];
}

int c8_screen_updated() {
	return screen_updated;
}

int c8_resolution(int *w, int *h) {
	if(!w || !h) return hi_res;
	if(hi_res) {
		*w = 128; *h = 64;
	} else {
		*w = 64; *h = 32;
	}
	return hi_res;
}

int c8_get_pixel(int x, int y) {
	int byte, bit, w, h;
	if(hi_res) {
		w = 128; h = 64;
	} else {
		w = 64; h = 32;
	}
	if(x < 0 || x >= w || y < 0 || y >= h) return 0;
	byte = y * w + x;
	bit = byte & 0x07;
	byte >>= 3;
	assert(byte < sizeof pixels);
	assert(bit < 8);
	return (pixels[byte] & (1 << bit)) != 0;
}

void c8_key_down(uint8_t k) {
	if(k > 0xF) return;
	keys |= 1 << k;
}

void c8_key_up(uint8_t k) {
	if(k > 0xF) return;
	keys &= ~(1 << k);
}

void c8_60hz_tick() {
	yield = 0;
	if(C8.DT > 0) C8.DT--;
	if(C8.ST > 0) C8.ST--;
}

int c8_sound() {
	return C8.ST > 0;
}

size_t c8_load_program(uint8_t program[], size_t n) {
	if(n + PROG_OFFSET > TOTAL_RAM)
		n = TOTAL_RAM - PROG_OFFSET;
	assert(n + PROG_OFFSET <= TOTAL_RAM);
	memcpy(C8.RAM + PROG_OFFSET, program, n);
	return n;
}

int c8_load_file(const char *fname) {
	FILE *f;
	size_t len, r;
	if(!(f = fopen(fname, "rb")))
		return 0;
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	if(len == 0 || len + PROG_OFFSET > TOTAL_RAM) {
		fclose(f);
		return 0;
	}
	rewind(f);
	r = fread(C8.RAM + PROG_OFFSET, 1, len, f);
	fclose(f);
	if(r != len)
		return 0;
	return len;
}

char *c8_load_txt(const char *fname) {
	FILE *f;
	size_t len, r;
	char *bytes;

	if(!(f = fopen(fname, "rb")))
		return NULL;

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	rewind(f);

	if(!(bytes = malloc(len+2)))
		return NULL;
	r = fread(bytes, 1, len, f);

	if(r != len) {
		free(bytes);
		return NULL;
	}

	fclose(f);
	bytes[len] = '\0';

	return bytes;
}

int c8_save_file(const char *fname) {
	uint16_t n = c8_prog_size();
	size_t len = n - PROG_OFFSET;
	FILE *f = fopen(fname, "wb");
	if(!f)
		return 0;
	if(fwrite(C8.RAM + PROG_OFFSET, 1, len, f) != len)
		return 0;
	fclose(f);
	return len;
}

int c8_message(const char *msg, ...) {
	if(msg) {
		va_list arg;
		va_start (arg, msg);
		vsnprintf (c8_message_text, MAX_MESSAGE_TEXT-1, msg, arg);
		va_end (arg);
	}
	if(c8_puts)
		return c8_puts(c8_message_text);
	return 0;
}

/**
 * [wikipedia]: https://en.wikipedia.org/wiki/CHIP-8
 *
 */
