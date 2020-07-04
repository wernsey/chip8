/*
Core of the CHIP-8 interpreter.
This file should be kept platform independent. Everything that is
platform dependent should be moved elsewhere.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>

#include "chip8.h"

/* Where in RAM to load the font.
	The font should be in the first 512 bytes of RAM (see [2]),
	so FONT_OFFSET should be less than or equal to 0x1B0 */
#define FONT_OFFSET		0x1B0
#define HFONT_OFFSET	0x110

int c8_verbose = 0;

static uint8_t V[16];          /* CHIP-8 registers */
static uint8_t RAM[TOTAL_RAM]; /* Interpreter RAM */
static uint16_t PC;            /* Program counter */
static uint16_t I;             /* Index register */
static uint8_t DT, ST;         /* Delay timer, sound timer */

/* Stack, Stack pointer */
static uint16_t stack[16];
static uint8_t SP;

/* Display memory */
static uint8_t pixels[1024];

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

/* Standard 4x5 font */
static uint8_t font[] = {
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
/* 'D' */ 0xE0, 0x80, 0x80, 0x80, 0xE0,
/* 'E' */ 0xF0, 0x80, 0xF0, 0x80, 0xF0,
/* 'F' */ 0xF0, 0x80, 0xF0, 0x80, 0x80,
};

/* SuperChip hi-res 8x10 font */
static uint8_t hfont[] = {
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
	memset(V, 0, sizeof V);
	memset(RAM, 0, sizeof RAM);
	PC = PROG_OFFSET;
	I = 0;
	DT = 0;
	ST = 0;
	SP = 0;
	memset(stack, 0, sizeof stack);

	assert(FONT_OFFSET + sizeof font <= PROG_OFFSET);
	memcpy(RAM + FONT_OFFSET, font, sizeof font);
	assert(HFONT_OFFSET + sizeof hfont <= FONT_OFFSET);
	memcpy(RAM + HFONT_OFFSET, hfont, sizeof hfont);

	hi_res = 0;
	screen_updated = 0;
}

void c8_step() {
	assert(PC < TOTAL_RAM);

	uint16_t opcode = RAM[PC] << 8 | RAM[PC+1];
	PC += 2;

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
				if(SP == 0) return; /* You've got problems */
				PC = stack[--SP];
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
				PC -= 2; /* reset the PC to the 00FD */
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
				/* SYS - not implemented, not supported */
			}
		break;
		case 0x1000:
			/* JP nnn */
			PC = nnn;
			break;
		case 0x2000:
			/* CALL nnn */
			if(SP >= 16) return; /* See RET */
			stack[SP++] = PC;
			PC = nnn;
			break;
		case 0x3000:
			/* SE Vx, kk */
			if(V[x] == kk) PC += 2;
			break;
		case 0x4000:
			/* SNE Vx, kk */
			if(V[x] != kk) PC += 2;
			break;
		case 0x5000:
			/* SE Vx, Vy */
			if(V[x] == V[y]) PC += 2;
			break;
		case 0x6000:
			/* LD Vx, kk */
			V[x] = kk;
			break;
		case 0x7000:
			/* ADD Vx, kk */
			V[x] += kk;
			break;
		case 0x8000: {
			uint16_t ans;
			switch(nibble) {
				case 0x0:
					/* LD Vx, Vy */
					V[x] = V[y];
					break;
				case 0x1:
					/* OR Vx, Vy */
					V[x] |= V[y];
					break;
				case 0x2:
					/* AND Vx, Vy */
					V[x] &= V[y];
					break;
				case 0x3:
					/* XOR Vx, Vy */
					V[x] ^= V[y];
					break;
				case 0x4:
					/* ADD Vx, Vy */
					ans = V[x] + V[y];
					V[0xF] = (ans > 255);
					V[x] = ans & 0xFF;
					break;
				case 0x5:
					/* SUB Vx, Vy */
					V[0xF] = (V[x] > V[y]);
					V[x] -= V[y];
					break;
				case 0x6:
					/* SHR Vx, Vy */
					V[0xF] = (V[x] & 0x01);
					V[x] >>= 1;
					break;
				case 0x7:
					/* SUBN Vx, Vy */
					V[0xF] = (V[y] > V[x]);
					V[x] = V[y] - V[x];
					break;
				case 0xE:
					/* SHL Vx, Vy */
					V[0xF] = ((V[x] & 0x80) != 0);
					V[x] <<= 1;
					break;
			}
		} break;
		case 0x9000:
			/* SNE Vx, Vy */
			if(V[x] != V[y]) PC += 2;
			break;
		case 0xA000:
			/* LD I, nnn */
			I = nnn;
			break;
		case 0xB000:
			/* JP V0, nnn */
			PC = (nnn + V[0]) & 0xFFF;
			break;
		case 0xC000:
			/* RND Vx, kk */
			V[x] = c8_rand() & kk; /* FIXME: Better RNG? */
			break;
		case 0xD000: {
			/* DRW Vx, Vy, nibble */
			int mW, mH, W, p, q;
			if(hi_res) {
				W = 128; mW = 0x7F; mH = 0x3F;
			} else {
				W = 64; mW = 0x3F; mH = 0x1F;
			}

			V[0xF] = 0;
			if(nibble) {
				x = V[x]; y = V[y];
				for(q = 0; q < nibble; q++) {
					for(p = 0; p < 8; p++) {
						int pix = (RAM[I + q] & (0x80 >> p)) != 0;
						if(pix) {
							int tx = (x + p) & mW, ty = (y + q) & mH;
							int byte = ty * W + tx;
							int bit = 1 << (byte & 0x07);
							byte >>= 3;
							if(pixels[byte] & bit)
								V[0x0F] = 1;
							pixels[byte] ^= bit;
						}
					}
				}
			} else {
				/* SCHIP mode has a 16x16 sprite if nibble == 0 */
				x = V[x]; y = V[y];
				for(q = 0; q < 16; q++) {
					for(p = 0; p < 16; p++) {
						int pix;
						if(p >= 8)
							pix = (RAM[I + (q * 2) + 1] & (0x80 >> (p & 0x07))) != 0;
						else
							pix = (RAM[I + (q * 2)] & (0x80 >> p)) != 0;
						if(pix) {
							int tx = (x + p) & mW, ty = (y + q) & mH;
							int byte = ty * W + tx;
							int bit = 1 << (byte & 0x07);
							byte >>= 3;
							if(pixels[byte] & bit)
								V[0x0F] = 1;
							pixels[byte] ^= bit;
						}
					}
				}
			}
			screen_updated = 1;
			} break;
		case 0xE000: {
			if(kk == 0x9E) {
				/* SKP Vx */
				if(keys & (1 << V[x]))
					PC += 2;
			} else if(kk == 0xA1) {
				/* SKNP Vx */
				if(!(keys & (1 << V[x])))
					PC += 2;
			}
		} break;
		case 0xF000: {
			switch(kk) {
				case 0x07:
					/* LD Vx, DT */
					V[x] = DT;
					break;
				case 0x0A: {
					/* LD Vx, K */
					if(!keys) {
						/* subsequent calls will encounter the Fx0A again */
						PC -= 2;
						return;
					}
					for(y = 0; y < 0xF; y++) {
						if(keys & (1 << y)) {
							V[x] = y;
							break;
						}
					}
					keys = 0;
				} break;
				case 0x15:
					/* LD DT, Vx */
					DT = V[x];
					break;
				case 0x18:
					/* LD ST, Vx */
					ST = V[x];
					break;
				case 0x1E:
					/* ADD I, Vx */
					I += V[x];
					/* According to [1] the VF is set if I overflows. */
					if(I > 0xFFF) {
						V[0xF] = 1;
						I &= 0xFFF;
					} else {
						V[0xF] = 0;
					}
					break;
				case 0x29:
					/* LD F, Vx */
					I = FONT_OFFSET + (V[x] & 0x0F) * 5;
					break;
				case 0x30:
					/* LD HF, Vx - Load 8x10 hi-resolution font */
					I = HFONT_OFFSET + (V[x] & 0x0F) * 10;
					break;
				case 0x33:
					/* LD B, Vx */
					RAM[I] = (V[x] / 100) % 10;
					RAM[I + 1] = (V[x] / 10) % 10;
					RAM[I + 2] = V[x] % 10;
					break;
				case 0x55:
					/* LD [I], Vx */
					if(I + x > TOTAL_RAM)
						x = TOTAL_RAM - I;
					assert(I + x <= TOTAL_RAM);
					if(x >= 0)
						memcpy(RAM + I, V, x+1);
					break;
				case 0x65:
					/* LD Vx, [I] */
					if(I + x > TOTAL_RAM)
						x = TOTAL_RAM - I;
					assert(I + x <= TOTAL_RAM);
					if(x >= 0)
						memcpy(V, RAM + I, x+1);
					break;
				case 0x75:
					/* LD R, Vx */
					assert(x <= sizeof hp48_flags);
					memcpy(hp48_flags, V, x);
					break;
				case 0x85:
					/* LD Vx, R */
					assert(x <= sizeof hp48_flags);
					memcpy(V, hp48_flags, x);
					break;
			}
		} break;
	}
}

int c8_ended() {
	/* Check whether the next instruction is 00FD */
	return c8_opcode(PC) == 0x00FD;
}
int c8_waitkey() {
	return (c8_opcode(PC) & 0xF0FF) == 0xF00A;
}

uint8_t c8_get(uint16_t addr) {
	assert(addr < TOTAL_RAM);
	return RAM[addr];
}

void c8_set(uint16_t addr, uint8_t byte) {
	assert(addr < TOTAL_RAM);
	RAM[addr] = byte;
}

uint16_t c8_opcode(uint16_t addr) {
	assert(addr < TOTAL_RAM - 1);
	return RAM[addr] << 8 | RAM[addr+1];
}

uint16_t c8_get_pc() {
	return PC;
}

uint16_t c8_prog_size() {
	uint16_t n;
	for(n = TOTAL_RAM - 1; n > PROG_OFFSET && RAM[n] == 0; n--);
	if(++n & 0x1) // Fix for #4
		return n + 1;
	return n;
}

uint8_t c8_get_reg(uint8_t r) {
	if(r > 0xF) return 0;
	return V[r];
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
	if(DT > 0) DT--;
	if(ST > 0) ST--;
}

int c8_sound() {
	return ST > 0;
}

size_t c8_load_program(uint8_t program[], size_t n) {
	if(n + PROG_OFFSET > TOTAL_RAM)
		n = TOTAL_RAM - PROG_OFFSET;
	assert(n + PROG_OFFSET <= TOTAL_RAM);
	memcpy(RAM + PROG_OFFSET, program, n);
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
	r = fread(RAM + PROG_OFFSET, 1, len, f);
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
	if(fwrite(RAM + PROG_OFFSET, 1, len, f) != len)
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
