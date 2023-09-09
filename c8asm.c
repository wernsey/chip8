/**
 *  CHIP8 assembler
 * ==================
 *
 * ![toc]
 *
 * ## Overview
 *
 * The syntax is mostly based on that of [Cowgod's Chip-8 Technical Reference v1.0][cowgod],
 * by Thomas P. Greene
 *
 * Semicolons `;` denote the start of a comment.
 *
 * Identifiers start with an alphabetic character, and are
 * followed by zero or more alphanumeric digits or underscores.
 *
 * Identifiers are not case sensitive. Identifiers, labels and instruction
 * Mnemonics can be written in upper or lower case. For example `sub`, `Sub` and
 * `SUB` are all equivalent.
 *
 * There are a couple of reserved special identifiers:
 *
 * * `I` for the index register.
 * * `DT` for the delay timer register.
 * * `ST` for the sound timer register.
 * * `K` for the key press register[^fake].
 * * `F` for the address of the 8&times;5 fonts[^fake].
 * * `B` for storing BCD values[^fake].
 * * `HF` for the address of the larger 8&times;10 fonts[^fake].
 * * `R` for the reserved RPL storage space[^fake][^rpl].
 *
 * [^fake]: there aren't actual registers `K`, `F`, `B`, `HF` and `R` in the interpreter, in but they're used in special syntax, particularly the `LD` instruction.
 *
 * These keywords are also reserved:
 *
 * * `define` to define new symbols
 * * `offset` to control where in the ROM output is placed
 * * `db` to write bytes to the ROM
 * * `dw` to write 16-bit words to the ROM
 *
 * [cowgod]: http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
 *
 * ### Notation
 *
 * In the purposes of this document:
 *
 * * `Vx`, `Vy` and `Vn` refer to any of the 16 Chip8 registers, `V0` through `VF`
 * * `addr` refers to a 12-bit address in the Chip8 interpreter's RAM.
 * * `nnn` refers to a 12-bit value.
 * * `n` refers to a 4-bit nibble value.
 * * `kk` is a byte value.
 *
 * Decimal literals are a sequence of the characters `0-9`, for example `254`.
 *
 * Binary literals start with `%` followed by several `0` or `1` symbols,
 * for example `%11111110`.
 *
 * Hexadecimal literals start with a `#` followed by characters from
 * `0-9`, `a-f` or `A-F`, for example `#FE` for 254.
 *
 * ## Summary
 *
 * | Chip8 Instruction  |  Mnemonic         | Description                                        |
 * |--------------------|-------------------|----------------------------------------------------|
 * |  0nnn              |   `sys nnn`       | System Call                                        |
 * |  00E0              |   `cls`           | Clear Screen                                       |
 * |  00EE              |   `ret`           | Return                                             |
 * |  1nnn              |   `jp addr`       | Jump to `addr`                                     |
 * |  2nnn              |   `call addr`     | Call routine at `addr`                             |
 * |  3nkk              |   `se Vn kk`      | Skip if `Vn` equals `kk`                           |
 * |  4nkk              |   `sne Vn kk`     | Skip if `Vn` does not equal `kk`                   |
 * |  5xy0              |   `se Vx Vy`      | Skip if `Vn` equals `Vy`                           |
 * |  6xkk              |   `ld Vx, kk`     | Loads a literal value `kk` into `Vx`               |
 * |  7nkk              |   `add Vn, kk`    | Add `kk` to register `Vx`                          |
 * |  8xy0              |   `ld Vx, Vy`     | Loads register `Vy` into `Vx`                      |
 * |  8xy1              |   `or Vx, Vy`     | Bitwise OR the value in `Vy` with register `Vx`    |
 * |  8xy2              |   `and Vx, Vy`    | Bitwise AND the value in `Vy` with register `Vx`   |
 * |  8xy3              |   `xor Vx, Vy`    | Bitwise XOR the value in `Vy` with register `Vx`   |
 * |  8xy4              |   `add Vx, Vy`    | Add the value in `Vy` to register `Vx`             |
 * |  8xy5              |   `xor Vx, Vy`    | Subtract the value in `Vy` from register `Vx`      |
 * |  8xy6              |   `shr Vx [, Vy]` | Shift `Vx` to the right with the value in `Vy`     |
 * |  8xy7              |   `subn Vx, Vy`   | Subtract the value in `Vy` from `Vx`, no carry     |
 * |  8xyE              |   `shl Vx [, Vy]` | Shift `Vx` to the left with the value in `Vy`      |
 * |  9xy0              |   `sne Vx Vy`     | Skip if `Vn` does not equal `Vy`                   |
 * |  Annn              |   `ld I, nnn`     | Loads `nnn` into register `I`                      |
 * |  Bnnn              |   `jp v0, addr`   | Jump to `v0 + addr`                                |
 * |  Cnkk              |   `rnd Vn, kk`    | random number AND `kk` into `Vn`                   |
 * |  Dxyn              |   `drw Vx, Vy, n` | Draw a sprite of `n` rows at `Vx,Vy`               |
 * |  En9E              |   `skp Vn`        | Skip if key in `Vn` pressed                        |
 * |  EnA1              |   `sknp Vn`       | Skip if key in `Vn` not pressed                    |
 * |  Fn0A              |   `ld Vn, K`      | loads a key pressed into `Vn`                      |
 * |  Fn1E              |   `add I, Vn`     | Add the value in `Vn` to register `I`              |
 * |  Fx07              |   `ld Vx, DT`     | Loads the delay timer into register `Vx`           |
 * |  Fx15              |   `delay Vx`      | Loads register `Vx` into the delay timer           |
 * |  Fx18              |   `sound Vx`      | Loads register `Vx` into the sound timer           |
 * |  Fx29              |   `hex Vx`        | Loads the 8&times;5 font sprite of `Vx` into `I`   |
 * |  Fx30[^super]      |   `hexx Vx`       | Loads the 8&times;10 font sprite of `Vx` into `I`  |
 * |  Fx33              |   `bcd Vx`        | Load BCD value of `Vx` into `I` to `I+2`           |
 * |  Fx55              |   `stor Vx`       | Stores `V0` through `Vx` to the address in `I`     |
 * |  Fx65              |   `rstr Vx`       | Restores `V0` through `Vx` from the address in `I` |
 * |  Fx75[^super]      |   `storx Vx`      | Stores `V0` through `Vx` to the reserved space     |
 * |  Fx85[^super]      |   `rstrx Vx`      | Restores `V0` through `Vx` from the reserved space |
 * |  00Cn[^super]      |   `scd n`         | Scroll down `n` pixels                             |
 * |  00FB[^super]      |   `scr`           | Scroll right                                       |
 * |  00FC[^super]      |   `scl`           | Scroll Left                                        |
 * |  00FD[^super]      |   `exit`          | Exits the SuperChip48 interpreter                  |
 * |  00FE[^super]      |   `low`           | Low res mode                                       |
 * |  00FF[^super]      |   `high`          | Enable 128&times;64 high-res mode                  |
 *
 * [^super]: Denotes a Super Chip48 instruction.
 */

/* CHIP-8 Assembler. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>

#include "chip8.h"

#define TOK_SIZE    64

#define MAX_DEFS    512
#define MAX_LOOKUP  2048

c8_include_callback_t c8_include_callback = c8_load_txt;

typedef enum {
	SYM_END,
	SYM_IDENTIFIER,
	SYM_INSTRUCTION,
	SYM_REGISTER,
	SYM_NUMBER,
	SYM_STRING,
	SYM_I,
	SYM_DT,
	SYM_ST,
	SYM_K,
	SYM_F,
	SYM_B,
	SYM_HF,
	SYM_R,
	SYM_DEFINE,
	SYM_OFFSET,
	SYM_DB,
	SYM_DW,
	SYM_TEXT,
	SYM_INCLUDE,
} SYMBOL;

/* List of instruction names.
(NB Keep this list sorted because of `bsearch()`) */
static const char *inst_names[] = {
	"add",
	"and",
	"bcd",
	"call",
	"cls",
	"delay",
	"drw",
	"exit",
	"hex",
	"hexx",
	"high",
	"jp",
	"key",
	"ld",
	"low",
	"or",
	"ret",
	"rnd",
	"rstr",
	"rstrx",
	"scd",
	"scl",
	"scr",
	"se",
	"shl",
	"shr",
	"sknp",
	"skp",
	"sne",
	"sound",
	"stor",
	"storx",
	"sub",
	"subn",
	"sys",
	"xor",
};
/* TIL https://stackoverflow.com/a/15824981/115589 */
static int inst_cmp(const void *s1, const void *s2) {
  const char *key = s1;
  const char * const *arg = s2;
  return strcmp(key, *arg);
}

typedef struct {
	const char * in;
	const char * last;
	SYMBOL sym;
	int linenum;
	char token[TOK_SIZE];
} Stepper;

#define BITNESS_BITMASK 0b0011
#define EXPRESSION_BITMASK 0b0100
#define EMIT8_BITMASK 0b1000
typedef enum {
	CONTINUED=0,
	ET_IMM4=0b10000,
	ET_IMM8=0b10001,
	ET_IMM4_EMIT8=0b11000,
	ET_IMM8_EMIT8=0b11001,
	ET_IMM12=0b10010,
	ET_IMM16=0b10011,
	ET_EXP4=0b10100,
	ET_EXP8=0b10101,
	ET_EXP4_EMIT8=0b11100,
	ET_EXP8_EMIT8=0b11101,
	ET_EXP12=0b10110,
	ET_EXP16=0b10111
} EMITTED_TYPE;

typedef struct {
	EMITTED_TYPE type;
	uint16_t value;
} Emitted;

/* Generated instructions before binary output */
static struct {
	struct {
		uint8_t byte;
		EMITTED_TYPE type;
		int linenum;
		char expression[TOK_SIZE];
	} bytes[TOTAL_RAM];

	uint16_t next_instr; /* Address of next instruction */
	uint16_t max_instr;  /* Largest instruction address for output */
} program;

/* Lookup table for labels for JP and CALL instructions */
static struct {
	char label[TOK_SIZE];
	uint16_t addr;
} lookup[MAX_LOOKUP];
static int n_lookup;

/* Lookup table for DEFINE identifier value statements */
static struct {
	char name[TOK_SIZE];
	SYMBOL type;
	char value[TOK_SIZE];
} defs[MAX_DEFS];
static int n_defs;

static void exit_error(const char *msg, ...) {
	char buffer[MAX_MESSAGE_TEXT];
	if(msg) {
		va_list arg;
		va_start (arg, msg);
		vsnprintf (buffer, MAX_MESSAGE_TEXT - 1, msg, arg);
		va_end (arg);
		c8_message("%s", buffer);
	}
	exit(1);
}
static bool is_arith(char c){
	switch (c){
		case '0' ... '9':
		case '(':
		case '#':
		case '%':
		case '+':
		case '-':
		case '*':
		case '/':
		case '|':
		case '&':
		case '^':
		case '<':
		case '>':
		case '~':
			return true;
		default:
			return false;
	}
}
static bool is_unary_operator(const  char * exp){
	if ((*exp=='-'||*exp=='+') && exp[1] == '(')
		return true;
	else if (*exp=='~')
		return true;
	else
		return false;
}
static int get_precedence(const char * expr){
	char c = *expr;
	switch (c){
		case '<':
		case '>':
			if(c==expr[1])
				return 1;
			else
				return 0;
		case '&':
		case '|':
		case '^':
			return 2;
		case '-':
		case '+':
			return 3;
		case '*':
		case '/':
		case '%':
			return 4;
		case '(':
			return 000;
		default:
			return 0;
	}
}
static int get_base(const char * a){
	if (isdigit(*a) )
		return 10;
	else if ((*a == '-' || *a == '+') && isdigit(a[1]))
		return 10;
	else if (*a == '#' && isxdigit(a[1]))
		return 16;
	 else if (*a == '%' && (a[1] == '0' || a[1] == '1'))
	 	return 2;
	else if (*a == '(')
		return -1;
	else
		return 0;
}
static int parse_int(char **expression, const int linenum){
	int base = get_base(*expression);
	if(base <= 0)
		exit_error("error:%d: Invalid Immediate\n", linenum);
	if (base!=10)
		(*expression)++;
	return (int)strtol(*expression, expression, base);
}

static void copy_arithmetic_expression(char * buffer, const char ** in){

	while (**in && **in != ',' && **in !='\n' && **in !=';'){
		if (**in == ' ') {
			(*in)++;
			continue;
		}
		(*buffer++)=(*(*in)++);
	}
	*buffer='\0';
}

static  int apply_unary_op (const unsigned char op, const  int val, const int linenum){
	switch (op)
	{
	case '+' | 0x80:
		return val;
	case '-' | 0x80 :
		return -val;
	case '~' | 0x80:
		return ~val;

	default:
		exit_error("error:%d: Invalid Arithmetic Expression\n",linenum);
	}
	/*unreachable*/
	return -1;
}
#define STACK_HEIGHT 64
static  int apply_binary_op(const  int l_op, const char op, const  int r_op, const int linenum){

	switch (op)
	{
	case '+':
		return l_op+r_op;
	case '-':
		return l_op-r_op;
	case '*':
		return l_op*r_op;
	case '/':
		return l_op/r_op;
	case '|':
		return l_op|r_op;
	case '&':
		return l_op&r_op;
	case '^':
		return l_op^r_op;
	case '<':
		return l_op<<r_op;
	case '>':
		return l_op>>r_op;
	default:
		exit_error("error:%d: Invalid Arithmetic Expression\n",linenum);
	}
	/*unreachable*/
	return -1;

}

static int evaluate_arithmetic_expression(char *expression, const int linenum){

	struct {
		unsigned char stack[STACK_HEIGHT];
		unsigned char *top;
	} operators;
// GCC doesn't like that `operators.top` points to before `operators.stack`
// but upon reviewing it, I decided it is fine.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
	operators.top = operators.stack - 1;
#pragma GCC diagnostic pop
	struct {
		int stack[STACK_HEIGHT];
		int *top;
	} figures;

	*figures.stack = 0;
	figures.top = figures.stack;
	bool is_prev_figure = true;
	bool is_first_char_of_clause = true;
	while (*expression){
		int prec;
		int base;

		if (strchr(" \t\r", *expression)) {
			expression++;
			continue;
		} else if(*expression == '(') {
			//if it's the first char I want to make sure it doesn't try to "bracket" the initial 0
			if(is_first_char_of_clause) *(++operators.top)='+';
			*(++operators.top)=*expression;
			*(++figures.top)=0;
			is_prev_figure=true;
			expression++;
			is_first_char_of_clause=true;
		} else if(*expression == ')') {
			while((*operators.top) != '(')
			{
				if (operators.top<operators.stack)
					exit_error("error:%d: Unbalanced Brackets\n", linenum);
				const  int r_op = *(figures.top--);
				const  int l_op = *(figures.top--);
				const unsigned char op = *(operators.top--);
				*(++figures.top)=apply_binary_op(l_op, op, r_op,linenum);
			}
			is_prev_figure=true;
			operators.top--;
			expression++;
			is_first_char_of_clause=false;
		} else if (is_unary_operator(expression) && (!is_prev_figure || is_first_char_of_clause)){
			if (is_first_char_of_clause) *(++operators.top) = '+';
			*(++operators.top)=((unsigned char) *expression)|0x80;
			is_prev_figure=false;
			expression++;
			is_first_char_of_clause=false;
		} else if (((prec=get_precedence(expression))>0) | ((base=get_base(expression))>0)){
			unsigned char operator_extended[2]={*operators.top,*operators.top};
			while(
				figures.top>=figures.stack &&
				is_prev_figure &&
				prec > 0 &&
				get_precedence((char *)operator_extended)>prec
			){
				const  int r_op = *(figures.top--);
				const  int l_op = *(figures.top--);
				const unsigned char op = *(operators.top--);
				*(++figures.top) = apply_binary_op(l_op, op, r_op,linenum);
				memset(operator_extended,*operators.top, 2);
			}
			if (base>0){
				if(is_prev_figure) {
					*(++operators.top)='+';
				}
				*(++figures.top)=parse_int(&expression, linenum);
				is_prev_figure=true;
				if (*operators.top&0x80){
					(*figures.top) = apply_unary_op(*operators.top, *figures.top, linenum);
					operators.top--;
				}


			} else {
				if (is_first_char_of_clause)
					exit_error("error:%d: Invalid Arithmetic Expression\n", linenum);
				*(++operators.top)=*expression;
				expression++;
				if (*expression=='>' || *expression=='<') expression++;
				is_prev_figure=false;

			}
			is_first_char_of_clause=false;
		} else {
			bool success = false;
			char buffer[64];
			char * bufptr=buffer;
			while ((*expression) && (!ispunct(*expression) || *expression == '_'))
				*bufptr++=*expression++;
			*bufptr='\0';
			for(int i = 0; i < n_lookup; i++) {
				if(!strcmp(lookup[i].label, buffer)) {
					if(is_prev_figure) {
						*(++operators.top)='+';
					}
					*(++figures.top)=lookup[i].addr;
					if (*operators.top&0x80){
						(*figures.top) = apply_unary_op(*operators.top, *figures.top, linenum);
						operators.top--;
					}
					success=true;
					is_first_char_of_clause=false;
					is_prev_figure=true;
					break;
				}
			}
			if(!success) exit_error("error:%d: Invalid Identifier %s in arithmetic expression\n", linenum, buffer);
		}
	}

	while(operators.top>=operators.stack){

		if (operators.top<operators.stack)
			exit_error("error%d: Unbalanced Brackets\n", linenum);
		const  int r_op = *(figures.top--);
		const  int l_op = *(figures.top--);
		const unsigned char op = *(operators.top--);
		*(++figures.top)=apply_binary_op(l_op, op, r_op,linenum);
	}
	return *figures.stack;

}

static void emit_b(const Stepper * stepper, uint8_t byte, const EMITTED_TYPE type) {
	if(program.next_instr >= TOTAL_RAM)
		exit_error("error: program too large\n");

	program.bytes[program.next_instr].linenum = stepper->linenum;
	program.bytes[program.next_instr].type=type;

	if (type & EXPRESSION_BITMASK)
		strcpy(program.bytes[program.next_instr].expression, stepper->token);

	program.bytes[program.next_instr++].byte = byte;
	if(program.next_instr > program.max_instr)
		program.max_instr = program.next_instr;
}

static void emit(const Stepper * stepper, const Emitted emitted){
	if (emitted.type == CONTINUED)
		exit_error("Continued is reserved\n");
	else if (emitted.type & EMIT8_BITMASK) {
		emit_b(stepper, emitted.value & 0xff, emitted.type);
	} else {
		emit_b(stepper, emitted.value >> 8, emitted.type);
		emit_b(stepper, emitted.value & 0xff, CONTINUED);
	}
}
static inline void emit_w(const Stepper * stepper, uint16_t word){
	const Emitted e = (Emitted){
		.type=ET_IMM16,
		.value=word
	};
	emit(stepper, e);
}
static inline void emit_e(const Stepper * stepper, uint16_t word, size_t nibble_count){
	const Emitted e = (Emitted) {
		.type=0b10000 | EXPRESSION_BITMASK | (nibble_count-1),
		.value=word
	};
	emit(stepper, e);
}

static void add_label(const char *label, const int linenum) {
	if(n_lookup == MAX_LOOKUP)
		exit_error("error: too many entries in lookup\n");
	for(int i = 0; i < n_lookup; i++)
		if(!strcmp(lookup[i].label, label))
			exit_error("error:%d: duplicate label '%s'\n", linenum, label);
	strcpy(lookup[n_lookup].label, label);
	lookup[n_lookup].addr = program.next_instr;
	n_lookup++;
}

static void add_definition(const Stepper * stepper, char *name) {
	if(n_defs == MAX_DEFS)
		exit_error("error:%d: too many definitions\n", stepper->linenum);
	strcpy(defs[n_defs].name, name);
	defs[n_defs].type = stepper->sym;
	strcpy(defs[n_defs].value,stepper->token);
	n_defs++;
}

static int nextsym(Stepper * stepper) {
	/* FIXME: Ought to guard against buffer overruns in tok, but not today. */
	char *tok = stepper->token;

	stepper->sym = SYM_END;
	*tok = '\0';

scan_start:
	while(isspace(*stepper->in)) {
		if(*stepper->in == '\n')
			stepper->linenum++;
		stepper->in++;
	}

	stepper->last=stepper->in;
	if(!*stepper->in)
		return SYM_END;

	if(*stepper->in == ';') {
		while(*stepper->in && *stepper->in != '\n')
			stepper->in++;
		goto scan_start;
	}

	if(isalpha(*stepper->in)) {
		while(isalnum(*stepper->in) || *stepper->in == '_')
			*tok++ = tolower(*stepper->in++);
		*tok = '\0';
		if(bsearch(stepper->token, inst_names,  (sizeof inst_names)/(sizeof inst_names[0]), sizeof inst_names[0], inst_cmp)) {
			stepper->sym = SYM_INSTRUCTION;
		} else if(stepper->sym != SYM_INSTRUCTION) {
			if(stepper->token[0] == 'v' && isxdigit(stepper->token[1]) && !stepper->token[2])
				stepper->sym = SYM_REGISTER;
			else if(!strcmp(stepper->token, "i"))
				stepper->sym = SYM_I;
			else if(!strcmp(stepper->token, "dt"))
				stepper->sym = SYM_DT;
			else if(!strcmp(stepper->token, "st"))
				stepper->sym = SYM_ST;
			else if(!strcmp(stepper->token, "k"))
				stepper->sym = SYM_K;
			else if(!strcmp(stepper->token, "f"))
				stepper->sym = SYM_F;
			else if(!strcmp(stepper->token, "b"))
				stepper->sym = SYM_B;
			else if(!strcmp(stepper->token, "hf"))
				stepper->sym = SYM_HF;
			else if(!strcmp(stepper->token, "r"))
				stepper->sym = SYM_R;
			else if(!strcmp(stepper->token, "define"))
				stepper->sym = SYM_DEFINE;
			else if(!strcmp(stepper->token, "offset"))
				stepper->sym = SYM_OFFSET;
			else if(!strcmp(stepper->token, "db"))
				stepper->sym = SYM_DB;
			else if(!strcmp(stepper->token, "dw"))
				stepper->sym = SYM_DW;
			else if(!strcmp(stepper->token, "text"))
				stepper->sym = SYM_TEXT;
			else if(!strcmp(stepper->token, "include"))
				stepper->sym = SYM_INCLUDE;
			else {
				if (is_arith(*stepper->in)){
					char arith_exp[64];
					copy_arithmetic_expression(arith_exp, &stepper->in);
					strcat (stepper->token,arith_exp);
					stepper->sym=SYM_NUMBER;

				} else {
					stepper->sym = SYM_IDENTIFIER;
					for(int i = 0; i < n_defs; i++) {
						if(!strcmp(defs[i].name, stepper->token)) {
							stepper->sym = defs[i].type;
							strcpy(stepper->token, defs[i].value);
							break;
						}
					}
				}

			}
		}
	} else if(is_arith(*stepper->in) ) {
		copy_arithmetic_expression(stepper->token, &stepper->in);
		stepper->sym = SYM_NUMBER;
	} else if(*stepper->in == '\"') {
		stepper->in++;
		for(;;) {
			if(!*stepper->in || strchr("\r\n", *stepper->in))
				exit_error("error:%d: unterminated string literal\n", stepper->linenum);
			if(*stepper->in == '\"') break;
			if(*stepper->in == '\\') {
				switch(*(++stepper->in)) {
					case '\0':
						exit_error("error:%d: bad escape in string literal\n", stepper->linenum);
					case 'a': *tok++ = '\a'; break;
					case 'b': *tok++ = '\b'; break;
					case 'e': *tok++ = 0x1B; break;
					case 'v': *tok++ = '\v'; break;
					case 'f': *tok++ = '\f'; break;
					case 'n': *tok++ = '\n'; break;
					case 'r': *tok++ = '\r'; break;
					case 't': *tok++ = '\t'; break;
					case '\\': *tok++ = '\\'; break;
					default: *tok++ = *stepper->in; break;
				}
				stepper->in++;
			} else
				*tok++ = *(stepper->in++);
		}
		*tok++ = '\0';
		stepper->in++;
		stepper->sym = SYM_STRING;
	} else {
		stepper->token[0] = *stepper->in;
		stepper->token[1] = '\0';
		stepper->sym = *stepper->in++;
	}

	return stepper->sym;
}

void expect(Stepper * stepper, int what) {
	SYMBOL sym = nextsym(stepper);
	if(sym != what)
		exit_error("error:%d: '%c' expected (got %d)\n", stepper->linenum, what, sym);
	nextsym(stepper);
}

static int get_register(const Stepper * stepper) {
	int reg = stepper->token[1];
	if(stepper->sym != SYM_REGISTER)
		exit_error("error:%d: register expected\n", stepper->linenum);
	assert(isxdigit(reg));
	if(reg >= 'a') {
		reg = reg - 'a' + 0xA;
	} else {
		reg -= '0';
	}
	assert(reg >= 0 && reg <= 0xF);
	return reg;
}

static int get_num(char *token, size_t nibble_count, const int linenum) {
	int a = evaluate_arithmetic_expression(token, linenum);
	int bound=1<<(4*nibble_count);
	if(a < -(bound/2) || a > (bound-1)){
		char format[128];
		sprintf(format,"error:%%d: number %%d takes more than %zd nibbles (%%0%zdX)\n",nibble_count,nibble_count);
		exit_error(format, linenum, a, a);
	}
	return a&(bound-1);
}

static int c8_assemble_internal(Stepper *stepper);

int c8_assemble(const char *text) {

	if(c8_verbose) c8_message("Assembling...\n");

	program.max_instr = 0;

	static Stepper theStepper;
	Stepper *stepper = &theStepper;

	stepper->in = text;
	stepper->linenum = 1;
	stepper->last = NULL;

	memset(program.bytes, 0, sizeof program);
	program.next_instr = 512;

	n_lookup = 0;
	n_defs = 0;

	int r = c8_assemble_internal(stepper);
	if(r)
		return r;

	if(c8_verbose)
		c8_message("Resolving labels...\n");

	size_t n = PROG_OFFSET;
	bool success=false;
	for(int i = PROG_OFFSET; i < program.max_instr; i++) {
		uint16_t result=0;
		if(program.bytes[i].type & EXPRESSION_BITMASK) {
			result = get_num(program.bytes[i].expression, (program.bytes[i].type & BITNESS_BITMASK)+1,program.bytes[i].linenum);

		}
		if ((program.bytes[i].type != CONTINUED) && (program.bytes[i].type & EMIT8_BITMASK)) {
			program.bytes[i].byte |= result &0xff;
		} else {
			program.bytes[i].byte |= result >> 8;
			program.bytes[i+1].byte |= result & 0xff;
		}

		if(c8_verbose > 1) {
			if(!(i & 0x01))
				c8_message("%03X: %02X", i, program.bytes[i].byte);
			else
				c8_message("%02X\n", program.bytes[i].byte);
		}

		c8_set(n++, program.bytes[i].byte);
	}
	//Stupid Off by one
	if (program.max_instr < TOTAL_RAM && program.bytes[program.max_instr].byte != 0){
		c8_set(n++, program.bytes[program.max_instr].byte);
	}
	if(c8_verbose > 1 && success)
		c8_message("\n");

	if(c8_verbose) c8_message("Assembled; %d bytes.\n", program.max_instr - PROG_OFFSET);

	return 0;
}

int c8_assemble_internal(Stepper *stepper) {

	nextsym(stepper);
	while(stepper->sym != SYM_END) {
		switch(stepper->sym){
		/**
		 * ## Directives
		 *
		 * ### define
		 *
		 * Syntax: `define ID VALUE`
		 *
		 * Associates a value with an identifier.
		 *
		 * For example, definitions like these
		 *
		 * ```
		 * ; Sprite X,Y position
		 * define sprite_x V0
         * define sprite_y V1
		 * ```
		 *
		 * will cause the identifiers `sprite_x` and `sprite_y` to be associated with the
		 * registers `V0` and `V1`
		 *
		 */
		case SYM_DEFINE:
		{
			char name[TOK_SIZE];
			nextsym(stepper);
			/* "Identifier expected" may also mean that the name has
				already been used, eg. if aaa is already defined as 123
				then define aaa 456 looks like define 123 456 */
			if(stepper->sym != SYM_IDENTIFIER)
				exit_error("error:%d: identifier expected, found %s\n", stepper->linenum, stepper->token);
			strcpy(name,stepper->token);
			nextsym(stepper); /*
			if(stepper->sym != SYM_NUMBER && stepper->sym != SYM_REGISTER)
				exit_error("error:%d: value expected\n", stepper->linenum);
			*/
			add_definition(stepper, name);
			nextsym(stepper);
		}
		break;
		/**
		 * ### offset
		 *
		 * Syntax: `offset EXPRESSION`
		 *
		 * Determines where in the program memory the next instruction will be emitted
		 *
		 * For example, the statement
		 *
		 * ```
		 * offset #280
		 * ```
		 *
		 * will cause the next bytes emitted by the assembler to start at 0x280
		 *
		 * The initial value is 0x200 (512), which is the default starting address
		 * for Chip8 programs.
		 */
		case SYM_OFFSET:
			nextsym(stepper);
			if(stepper->sym != SYM_NUMBER)
				exit_error("error:%d: offset expected\n", stepper->linenum);
			program.next_instr = get_num(stepper->token,3,stepper->linenum);
			nextsym(stepper);
		break;
		/**
		 * ### db
		 *
		 * Syntax: `db byte, byte, byte, ...`
		 *
		 * Emits a sequence of bytes.
		 *
		 * For example, this is how you create a sprite's graphic
		 * by emitting a sequence of bytes:
		 *
		 * ```
		 * sprite1:
  		 * db  %01111110,
  		 *     %10000001,
  		 *     %10100101,
  		 *     %10111101,
  		 *     %10111101,
  		 *     %10011001,
  		 *     %10000001,
  		 *     %01111110,
		 * ```
		 *
		 * (the label `sprite1` allows you to find those bytes later
		 * through a `LD I, sprite1` instruction)
		 *
		 */
		case SYM_DB:
			do {
				nextsym(stepper);
				if(stepper->sym == SYM_END)
					break;
				if(stepper->sym != SYM_NUMBER)
					exit_error("error:%d: byte value expected\n", stepper->linenum);
				Emitted e={/*
					.type=ET_IMM8,
					.value.imm8=get_num(stepper,2),
					.tlabel=LT_NONE*/
					.type=ET_EXP8_EMIT8,
					.value=0
				};

				emit(stepper,e);
				nextsym(stepper);
			} while(stepper->sym == ',');
		break;
		/**
		 * ### `dw`
		 *
		 * Syntax: `dw word, word, word, ...`
		 *
		 * Like `db`, but emits a sequence of 16-bit words.
		 *
		 */
		case SYM_DW:
			do {
				nextsym(stepper);
				if(stepper->sym == SYM_END)
					break;
				if(stepper->sym != SYM_NUMBER && stepper->sym != SYM_IDENTIFIER)
					exit_error("error:%d: byte value expected\n", stepper->linenum);
				emit_e(stepper,0, 4);
				nextsym(stepper);
			} while(stepper->sym == ',');
		break;
		/**
		 * ### text
		 *
		 * Syntax: `text "A String"`
		 *
		 * Writes all the bytes in the string, terminated with a null (`'\0'`) character, to the output.
		 *
		 * For example `text "hello"` is equivalent to `db #68, #65, #6C, #6C, #6F, #00`.
		 */
		case SYM_TEXT: {
			nextsym(stepper);
			if(stepper->sym != SYM_STRING)
				exit_error("error:%d: string value expected\n", stepper->linenum);
			Emitted e = { .type=EMIT8_BITMASK };
			for(char *c = stepper->token; *c; c++) {
				e.value = *c;
				emit(stepper, e);
			}
			e.value = '\0';
			emit(stepper, e);
			nextsym(stepper);
		} break;
		/**
		 * ### include
		 *
		 * Syntax: `include "filename"`
		 *
		 * Assembles file `filename` and adds the results to the output bytecode.
		 *
		 */
		case SYM_INCLUDE: {
			nextsym(stepper);
			if(stepper->sym != SYM_STRING)
				exit_error("error:%d: file name expected\n", stepper->linenum);

			if(c8_verbose)
				c8_message("including '%s'\n", stepper->token);

			if(!c8_include_callback) {
				exit_error("error:%d: `include` directive disabled\n", stepper->linenum);
			} else {
				char *intext = c8_include_callback(stepper->token);
				if(!intext) {
					exit_error("error:%d: couldn't read %s\n", stepper->linenum, stepper->token);
				}
				Stepper nextStepper;
				nextStepper.in = intext;
				nextStepper.linenum = 1;
				nextStepper.last = NULL;
				c8_assemble_internal(&nextStepper);

				free(intext);
			}

			nextsym(stepper);
		} break;
		/**
		 * ## Labels
		 *
		 * Labels for jump targets are lines that start start with
		 * an identifier followed by a colon. The identifier can be
		 * used elsewhere in the program as a target for jump
		 * instructions. For example:
		 *
		 * ```
		 * loop:
		 *     CLS
		 *     ...
		 *     JP loop
		 * ```
		 *
		 */
		case SYM_IDENTIFIER:
			add_label(stepper->token, stepper->linenum);
			SYMBOL sym = nextsym(stepper);
			if(sym != ':') {
				/* It's more likely that the user got the mnemonic wrong than forgot the ':' */
				exit_error("error:%d: Unknown instruction `%s`\n", stepper->linenum, lookup[n_lookup-1].label);
			}
			nextsym(stepper);
		break;
		case SYM_INSTRUCTION:
			/**
			 * ## Instructions
			 *
			 * ### SYS - System call
			 *
			 * `sys nnn` - calls the system subroutine at location `nnn` (`0nnn`)
			 *
			 * (Unused at the moment; *TODO* We need a mechanism to hook `sys`
			 * calls into the interpreter in the future)
			 */
			if(!strcmp("sys", stepper->token)) {
				nextsym(stepper);
				emit_e(stepper, 0x0000,3);
			/**
			 * ### CLS - Clear screen
			 *
			 * `cls` - Clears the screen (`00E0`).
			 */
			} else if(!strcmp("cls", stepper->token)) {
				emit_w(stepper, 0x00E0);
			/**
			 * ### CALL - Call subroutine
			 *
			 * `call addr` - Calls the subroutine at address `nnn` (`2nnn`).
			 */
			} else if(!strcmp("call", stepper->token)) {
				nextsym(stepper);
				if(stepper->sym != SYM_IDENTIFIER && stepper->sym != SYM_NUMBER){
					exit_error("error:%d: address expected", stepper->linenum);
				}
				const Emitted e={
					.type=ET_EXP16,
					.value=0x2000
				};
				emit(stepper, e);
			/**
			 * ### RET - Return
			 *
			 * `ret` - Returns from a subroutine (`00EE`).
			 */
			} else if(!strcmp("ret", stepper->token)) {
				emit_w(stepper, 0x00EE);
			/**
			 * ### JP - Jump
			 *
			 * Unconditional jump instruction.
			 *
			 * * `jp nnn` - Jumps to the program location `nnn` (`1nnn`).
			 * * `jp v0, nnn` - Jumps to the program location calculated from `v0 + nnn` (`Bnnn`).
			 */
			} else if(!strcmp("jp", stepper->token)) {
				nextsym(stepper);
				if(stepper->sym == SYM_IDENTIFIER || stepper->sym == SYM_NUMBER){
					const Emitted e={
						.type=ET_EXP16,
						.value=0x1000
					};
					emit(stepper, e);
				}
				else if(stepper->sym == SYM_REGISTER) {
					if(strcmp(stepper->token, "v0"))
						exit_error("error:%d: JP applies to V0 only\n", stepper->linenum);
					expect(stepper, ',');
					if(stepper->sym == SYM_IDENTIFIER || stepper->sym == SYM_NUMBER){
						const Emitted e={
							.type=ET_EXP16,
							.value=0xB000
						};
						emit(stepper, e);
					}
					else
						emit_e(stepper, 0xB000 ,3);
				} else
					emit_e(stepper, 0x1000 , 3);

			/**
			 * ### SE - Skip if Equal
			 *
			 * Skip if equal:
			 *
			 * * `se Vn, kk` - skips the next instruction if the value in `Vn` equals `kk` (`3nkk`)
			 * * `se Vx, Vy` - skips the next instruction if the value in `Vx` equals
			 *     the value in `Vy` (`5xy0`)
			 */
			} else if(!strcmp("se", stepper->token)) {
				nextsym(stepper);
				int regx = get_register(stepper);
				expect(stepper, ',');
				if(stepper->sym == SYM_NUMBER || stepper->sym == SYM_IDENTIFIER)
					emit_e(stepper, 0x3000 | (regx << 8), 2);
				else if(stepper->sym == SYM_REGISTER) {
					int regy = get_register(stepper);
					emit_w(stepper, 0x5000 | (regx << 8) | (regy << 4));
				} else
					exit_error("error:%d: operand expected\n", stepper->linenum);
			/**
			 * ### SNE - Skip Not Equal
			 *
			 * Skip if not equal:
			 *
			 * * `sne Vn, kk` - skips the next instruction if the value in `Vn` is
			 *     not equal to `kk` (`4nkk`)
			 * * `sne Vx, Vy` - skips the next instruction if the value in `Vx` is
			 *     not equal to the value in `Vy` (`9xy0`)
			 */
			} else if(!strcmp("sne", stepper->token)) {
				nextsym(stepper);
				int regx = get_register(stepper);
				expect(stepper, ',');
				if(stepper->sym == SYM_NUMBER || stepper->sym==SYM_IDENTIFIER)
					emit_e(stepper, 0x4000 | (regx << 8) ,2);
				else if(stepper->sym == SYM_REGISTER) {
					int regy = get_register(stepper);
					emit_w(stepper, 0x9000 | (regx << 8) | (regy << 4));
				} else
					exit_error("error:%d: operand expected\n", stepper->linenum);
			/**
			 * ### LD - Load
			 *
			 * Load instructions:
			 *
			 * * `ld I, nnn` - loads the 12-bit literal value `nnn` into register `I` (`Annn`).
			 * * `ld Vx, kk` - loads the literal value `kk` into `Vx` (`6xkk`).
			 * * `ld Vx, Vy` - loads the value in `Vy` into `Vx` (`8xy0`).
			 * * `ld Vx, DT` - loads the value of the _Delay Timer_ into `Vx` (`Fx07`).
			 *
			 * These variations are provided for compatibility with [Cowgod][]'s syntax, but
			 * the alternative syntax should be preferred:
			 *
			 * * `ld DT, Vx` - loads the value in `Vx` into the _Delay Timer_ (`Fx15`).
			 * 		Use [`delay Vx`](#delay---delay) instead.
			 * * `ld ST, Vx` - loads the value in `Vx` into the _Sound Timer_ (`Fx18`).
			 * 		Use [`sound Vx`](#sound---play-a-sound) instead.
			 * * `ld F, Vx` - loads the location of the 8&times;5 font sprite for `Vx` into `I` (`Fx29`).
			 * 		Use [`hex Vx `](#hex---prepare-a-hex-sprite) instead.
			 * * `ld B, Vx` - stores the BCD representation of `Vx` in the memory locations `I`, `I+1` and `I+2` (`Fx33`).
			 * 		Use [`bcd Vx`](#bcd---bcd-representation) instead.
			 * * `ld [I], Vx` - saves `V0` through `Vx` to the memory addresses `I` through `I+x` (`Fx55`).
			 * 		Use [`stor Vx`](#stor---store-registers) instead.
			 * * `ld Vx, [I]` - loads `V0` through `Vx` from the memory addresses `I` through `I+x` (`Fx65`).
			 * 		Use [`rstr Vx`](#rstr---restore-registers) instead.
			 * * `ld Vx, K` - Waits for a keypress, then loads the key pressed into `Vx` (`Fx0A`).
			 * 		Use [`key Vx`](#key---wait-for-key-press) instead.
			 *
			 * #### Super Chip48 LD additions
			 *
			 * These forms are also provided for compatibility with [Cowgod][]'s syntax, but
			 * the alternative syntax should be preferred:
			 *
			 * * `ld HF, Vx` - loads the location of the larger 8&times;10 font sprite for `Vx` into `I` (`Fx30`).
			 * 		Use [`hexx Vx`](#hexx---prepare-a-large-hex-sprite) instead.
			 * * `ld R, Vx` - saves `V0` through `Vx` to a special reserved memory space `R`[^rpl]  (`Fx75`)
			 * 		Use [`storx Vx`](#storx---store-registers-extended) instead.
			 * * `ld Vx, R` - loads `V0` through `Vx` from the special reserved memory space `R`[^rpl] (`Fx85`)
			 * 		Use [`rstrx Vx`](#rstrx---restore-registers-extended) instead.
			 *
			 * [^rpl]: The original SUPER-CHIP stored these in the calculator on which it ran's RPL registers
			 */
			} else if(!strcmp("ld", stepper->token)) {
				nextsym(stepper);
				if(stepper->sym == SYM_I) {
					expect(stepper, ',');
					if(stepper->sym == SYM_IDENTIFIER || stepper->sym == SYM_NUMBER){
						const Emitted e={
							.type=ET_EXP16,
							.value=0xA000
						};
						emit(stepper, e);
					} else
						emit_e(stepper, 0xA000,3);
				} else if(stepper->sym == SYM_DT) {
					expect(stepper, ',');
					emit_w(stepper, 0xF015 | (get_register(stepper) << 8));
				} else if(stepper->sym == SYM_ST) {
					expect(stepper, ',');
					emit_w(stepper, 0xF018 | (get_register(stepper) << 8));
				} else if(stepper->sym == SYM_F) {
					expect(stepper, ',');
					emit_w(stepper, 0xF029 | (get_register(stepper) << 8));
				} else if(stepper->sym == SYM_B) {
					expect(stepper, ',');
					emit_w(stepper, 0xF033 | (get_register(stepper) << 8));
				} else if(stepper->sym == '[') {
					if(nextsym(stepper) != SYM_I || nextsym(stepper) != ']')
						exit_error("error:%d: [I] expected\n", stepper->linenum);
					if(nextsym(stepper) != ',')
						exit_error("error:%d: ',' expected\n", stepper->linenum);
					nextsym(stepper);
					emit_w(stepper, 0xF055 | (get_register(stepper) << 8));
				} else if(stepper->sym == SYM_HF) {
					expect(stepper, ',');
					emit_w(stepper, 0xF030 | (get_register(stepper) << 8));
				} else if(stepper->sym == SYM_R) {
					expect(stepper, ',');
					emit_w(stepper, 0xF075 | (get_register(stepper) << 8));
				} else {
					int regx = get_register(stepper);
					expect(stepper, ',');
					if(stepper->sym == SYM_NUMBER || stepper->sym == SYM_IDENTIFIER)
						emit_e(stepper, 0x6000 | (regx << 8) , 2);
					 else if(stepper->sym == SYM_REGISTER) {
						int regy = get_register(stepper);
						emit_w(stepper, 0x8000 | (regx << 8) | (regy << 4));
					} else if(stepper->sym == SYM_DT)
						emit_w(stepper, 0xF007 | (regx << 8));
					else if(stepper->sym == SYM_K)
						emit_w(stepper, 0xF00A | (regx << 8));

					else if(stepper->sym == '[') {
						if(nextsym(stepper) != SYM_I || nextsym(stepper) != ']')
							exit_error("error:%d: [I] expected\n", stepper->linenum);
						emit_w(stepper, 0xF065 | (regx << 8));
					} else if(stepper->sym == SYM_R) {
						emit_w(stepper, 0xF085 | (regx << 8));
					} else
						exit_error("error:%d: operand expected, found %s[%d]\n", stepper->linenum, stepper->token, stepper->sym);
				}
			/**
			 * ### ADD - Add values
			 *
			 * * `add Vn, kk` - Adds `kk` to `Vn`;
			 *       The result is stored in `Vn` (`7nkk`)
			 * * `add Vx, Vy` - Adds the value in `Vy` to `Vx`;
			 *       The result is stored in `Vx` (`8xy4`)
			 * * `add I, Vn` - Adds the value in `Vn` to `I`;
			 *       The result is stored in `I` (`Fn1E`)
			 */
			} else if(!strcmp("add", stepper->token)) {
				nextsym(stepper);
				if(stepper->sym == SYM_I) {
					expect(stepper, ',');
					emit_w(stepper, 0xF01E | (get_register(stepper) << 8));
				} else {
					int regx = get_register(stepper);
					expect(stepper, ',');
					if(stepper->sym == SYM_NUMBER || stepper->sym == SYM_IDENTIFIER)
						emit_e(stepper, 0x7000 | (regx << 8) ,2);
					 else if(stepper->sym == SYM_REGISTER) {
						int regy = get_register(stepper);
						emit_w(stepper, 0x8004 | (regx << 8) | (regy << 4));
					} else
						exit_error("error:%d: operand expected\n", stepper->linenum);
				}
			/**
			 * ### OR - Bitwise OR
			 *
			 * `or Vx, Vy` - biwise OR of the value in `Vy` with `Vx` (`8xy1`).
			 *
			 * The result is stored in `Vx`
			 */
			} else if(!strcmp("or", stepper->token)) {
				nextsym(stepper);
				int regx = get_register(stepper);
				expect(stepper, ',');
				int regy = get_register(stepper);
				emit_w(stepper, 0x8001 | (regx << 8) | (regy << 4));
			/**
			 * ### AND - Bitwise AND
			 *
			 * `and Vx, Vy` - biwise AND of the value in `Vy` with `Vx` (`8xy2`).
			 *
			 * The result is stored in `Vx`
			 */
			} else if(!strcmp("and", stepper->token)) {
				nextsym(stepper);
				int regx = get_register(stepper);
				expect(stepper, ',');
				int regy = get_register(stepper);
				emit_w(stepper, 0x8002 | (regx << 8) | (regy << 4));
			/**
			 * ### XOR - Bitwise Exclusive OR
			 *
			 * `xor Vx, Vy` - biwise exclusive-OR of the value in `Vy` with `Vx` (`8xy3`).
			 *
			 * The result is stored in `Vx`
			 */
			} else if(!strcmp("xor", stepper->token)) {
				nextsym(stepper);
				int regx = get_register(stepper);
				expect(stepper, ',');
				int regy = get_register(stepper);
				emit_w(stepper, 0x8003 | (regx << 8) | (regy << 4));
			/**
			 * ### SUB - Subtract
			 *
			 * `sub Vx, Vy` - Subtracts the value in `Vy` from `Vx` (`8xy5`).
			 *
			 * The result is stored in `Vx`
			 */
			} else if(!strcmp("sub", stepper->token)) {
				nextsym(stepper);
				int regx = get_register(stepper);
				expect(stepper, ',');
				int regy = get_register(stepper);
				emit_w(stepper, 0x8005 | (regx << 8) | (regy << 4));
			/**
			 * ### SHR - Shift Right
			 *
			 * `shr Vx [, Vy]` maps to the `8xy6` Chip8 instruction.
			 *
			 * The assembler will translate `shr Vx` to `8xx6` to work both
			 * in cases where the interpreter moves `Vy` into `Vx` before the
			 * and shift and in cases where they don't.
			 *
			 * This is a well known quirk between different CHIP8 implementations.
			 * [More information](https://tobiasvl.github.io/blog/write-a-chip-8-emulator/#8xy6-and-8xye-shift).
			 */
			} else if(!strcmp("shr", stepper->token)) {
				nextsym(stepper);
				int regx = get_register(stepper);
				int regy = 0;
				nextsym(stepper);
				if(stepper->sym == ',') {
					nextsym(stepper);
					regy = get_register(stepper);
				} else
					stepper->in=stepper->last;
				emit_w(stepper, 0x8006 | (regx << 8) | (regy << 4));
			/**
			 * ### SUBN - Subtract No Borrow
			 *
			 * `subn Vx, Vy` maps to the `8xy7` Chip8 instruction.
			 */
			} else if(!strcmp("subn", stepper->token)) {
				nextsym(stepper);
				int regx = get_register(stepper);
				expect(stepper, ',');
				int regy = get_register(stepper);
				emit_w(stepper, 0x8007 | (regx << 8) | (regy << 4));
			/**
			 * ### SHL - Shift Left
			 *
			 * `shl Vx [, Vy]` maps to the `8xyE` Chip8 instruction.
			 *
			 * The assembler will translate `shl Vx` to `8xxE` to work both
			 * in cases where the interpreter moves `Vy` into `Vx` before the
			 * and shift and in cases where they don't.
			 *
			 * This is a well known quirk between different CHIP8 implementations.
			 * [More information](https://tobiasvl.github.io/blog/write-a-chip-8-emulator/#8xy6-and-8xye-shift).
			 */
			} else if(!strcmp("shl", stepper->token)) {
				nextsym(stepper);
				int regx = get_register(stepper);
				int regy=0;
				nextsym(stepper);
				if(stepper->sym == ',') {
					nextsym(stepper);
					regy = get_register(stepper);
				} else
					stepper->in=stepper->last;
				emit_w(stepper, 0x800E | (regx << 8) | (regy << 4));
			/**
			 * ### RND - Random number
			 *
			 * `rnd Vn, kk` - generates a random number masked with `kk` and stores it in `Vn` (`Cnkk`).
			 *
			 * It creates a random number that is bitwise AND'ed with `kk`.
			 */
			} else if(!strcmp("rnd", stepper->token)) {
				nextsym(stepper);
				int regx = get_register(stepper);
				expect(stepper, ',');
				if(stepper->sym == SYM_NUMBER || stepper->sym == SYM_IDENTIFIER){
					emit_e(stepper, 0xC000 | (regx << 8), 2);
				}
				else exit_error("error:%d: operand expected\n", stepper->linenum);
			/**
			 * ### DRW - Draw Sprite
			 *
			 * `drw Vx, Vy, n` - Draws the `n`-byte sprite at memory location `I`
			 * to the screen position `Vx`,`Vy`. (`Dxyn`)
			 */
			}  else if(!strcmp("drw", stepper->token)) {
				nextsym(stepper);
				int regx = get_register(stepper);
				expect(stepper, ',');
				int regy = get_register(stepper);
				expect(stepper, ',');
				emit_e(stepper, 0xD000 | (regx << 8) | (regy << 4),1);
			/**
			 * ### SKP - Skip if Key Pressed
			 *
			 * `skp Vn` - Skips the next instruction if the key identified by `Vn` is pressed (`En9E`)
			 */
			} else if(!strcmp("skp", stepper->token)) {
				nextsym(stepper);
				emit_w(stepper, 0xE09E | (get_register(stepper) << 8));
			/**
			 * ### SKNP - Skip if Key Not Pressed
			 *
			 * `sknp Vn` - Skips the next instruction if the key identified by `Vn` is not pressed (`EnA1`)
			 */
			} else if(!strcmp("sknp", stepper->token)) {
				nextsym(stepper);
				emit_w(stepper, 0xE0A1 | (get_register(stepper) << 8));
			/**
			 * ### DELAY - Delay
			 *
			 * `delay Vx` - loads the value in `Vx` into the _Delay Timer_ (`Fx15`).
			 */
			} else if(!strcmp("delay", stepper->token)) {
				nextsym(stepper);
				emit_w(stepper, 0xF015 | (get_register(stepper) << 8));
			/**
			 * ### SOUND - Play a sound
			 *
			 * `sound Vx` - loads the value in `Vx` into the _Sound Timer_ (`Fx18`).
			 */
			} else if(!strcmp("sound", stepper->token)) {
				nextsym(stepper);
				emit_w(stepper, 0xF018 | (get_register(stepper) << 8));
			/**
			 * ### HEX - prepare a hex sprite
			 *
			 * `hex Vx` - loads the location of the 8&times;5 font sprite for the hex value
			 * represented by `Vx` into `I` (`Fx29`).
			 *
			 */
			} else if(!strcmp("hex", stepper->token)) {
				nextsym(stepper);
				emit_w(stepper, 0xF029 | (get_register(stepper) << 8));
			/**
			 * ### BCD - BCD representation
			 *
			 * `bcd Vx` - stores the BCD representation of `Vx` in the memory locations `I`, `I+1` and `I+2` (`Fx33`).
			 *
			 */
			} else if(!strcmp("bcd", stepper->token)) {
				nextsym(stepper);
				emit_w(stepper, 0xF033 | (get_register(stepper) << 8));
			/**
			 * ### KEY - Wait for key press
			 *
			 * `key Vx` - Waits for a keypress, then loads the key pressed into `Vx` (`Fx0A`).
			 */
			} else if(!strcmp("key", stepper->token)) {
				nextsym(stepper);
				emit_w(stepper, 0xF00A | (get_register(stepper) << 8));
			/**
			 * ### STOR - Store registers
			 *
			 * `stor Vx` - Stores registers `V0` through `Vx` to the memory addresses `I` through `I+x` (`Fx55`).
			 */
			} else if(!strcmp("stor", stepper->token)) {
				nextsym(stepper);
				emit_w(stepper, 0xF055 | (get_register(stepper) << 8));
			/**
			 * ### RSTR - Restore registers
			 *
			 * `rstr Vx` - Restores registers `V0` through `Vx` from the memory addresses `I` through `I+x` (`Fx65`).
			 */
			} else if(!strcmp("rstr", stepper->token)) {
				nextsym(stepper);
				emit_w(stepper, 0xF065 | (get_register(stepper) << 8));
			/**
			 *
			 * ## Super Chip48 Instructions
			 *
			 * ### SCD - Scroll Down
			 *
			 * `scd n` - scrolls the screen down by `n` rows (`00Cn`)
			 *
			 */
			} else if(!strcmp("scd", stepper->token)) {
				nextsym(stepper);
				emit_e(stepper, 0x00C0 ,1);
			/**
			 * ### SCR - Scroll Right
			 *
			 * `scr` - scrolls the screen to the right by 4 pixels (`00FB`)
			 */
			} else if(!strcmp("scr", stepper->token)) {
				emit_w(stepper, 0x00FB);
			/**
			 * ### SCL - Scroll Left
			 *
			 * `scl` - scrolls the screen to the left by 4 pixels (`00FC`)
			 */
			} else if(!strcmp("scl", stepper->token)) {
				emit_w(stepper, 0x00FC);
			/**
			 * ### EXIT - Exit
			 *
			 * `exit` - stops the interpreter (`00FD`)
			 */
			} else if(!strcmp("exit", stepper->token)) {
				emit_w(stepper, 0x00FD);
			/**
			 * ### HEXX - prepare a large hex sprite
			 *
			 * `hexx Vx` - loads the location of the 8&times;10 font sprite of the hex value
			 * represented by `Vx` into `I` (`Fx30`).
			 *
			 * See also the [`hex`](#hex---prepare-a-hex-sprite) instruction.
			 */
			} else if(!strcmp("hexx", stepper->token)) {
				nextsym(stepper);
				emit_w(stepper, 0xF030 | (get_register(stepper) << 8));
			/**
			 * ### LOW - Low Resolution Mode
			 *
			 * `low` maps to the `00FE` Chip8 instruction.
			 */
			} else if(!strcmp("low", stepper->token)) {
				emit_w(stepper, 0x00FE);
			/**
			 * ### HIGH - High Resolution Mode
			 *
			 * `high` maps to the `00FF` Chip8 instruction.
			 */
			} else if(!strcmp("high", stepper->token)) {
				emit_w(stepper, 0x00FF);
			/**
			 * ### STORX - Store registers, extended
			 *
			 * `storx Vx` - Saves registers `V0` through `Vx` to a special reserved memory space `R`[^rpl]  (`Fx75`)
			 *
			 * See also the [`stor`](#stor---store-registers) instruction.
			 */
			} else if(!strcmp("storx", stepper->token)) {
				nextsym(stepper);
				emit_w(stepper, 0xF075 | (get_register(stepper) << 8));
			/**
			 * ### RSTRX - Restore registers, extended
			 *
			 * `rstrx Vx` - Restores registers `V0` through `Vx` from the special reserved memory space `R`[^rpl] (`Fx85`)
			 *
			 * See also the [`rstr`](#rstr---restore-registers) instruction.
			 */
			} else if(!strcmp("rstrx", stepper->token)) {
				nextsym(stepper);
				emit_w(stepper, 0xF085 | (get_register(stepper) << 8));
			}

			nextsym(stepper);
		break;
		default:
			exit_error("error:%d: unexpected token [%d]: '%s'\n", stepper->linenum, stepper->sym, stepper->token);
		}
	}

	return 0;
}

/**
 * ## References
 *
 * * [Cowgod's Chip-8 Technical Reference v1.0][cowgod]
 * * The Octo project's [Chip8 reference PDF](https://github.com/JohnEarnest/Octo/blob/gh-pages/docs/chip8ref.pdf) by John Earnest.
 * * The Octo project's [Mastering SuperChip](https://github.com/JohnEarnest/Octo/blob/gh-pages/docs/SuperChip.md) reference.
 * * [Guide to making a CHIP-8 emulator][Langhoff] by Tobias V. Langhoff
 *
 * [Langhoff]: https://tobiasvl.github.io/blog/write-a-chip-8-emulator/#8xy6-and-8xye-shift
 * [cowgod]: http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
 */
