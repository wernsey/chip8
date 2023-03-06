/*
CHIP8 assembler.
Mostly based on the syntax of "Cowgod's Chip-8 Technical Reference v1.0",
by Thomas P. Greene, http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
Hexadecimal constants can be written as
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

#define MAX_DEFS    256
#define MAX_LOOKUP  256

typedef enum {
	SYM_END,
	SYM_IDENTIFIER,
	SYM_INSTRUCTION,
	SYM_REGISTER,
	SYM_NUMBER,
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
	SYM_HI,
	SYM_LO
} SYMBOL;
/* List of instruction names. */
static const char *inst_names[] = {
	"add",
	"and",
	"call",
	"cls",
	"drw",
	"jp",
	"ld",
	"or",
	"ret",
	"rnd",
	"se",
	"shl",
	"shr",
	"sknp",
	"skp",
	"sne",
	"sub",
	"subn",
	"sys",
	"xor",
	"scd",
	"scr",
	"scl",
	"exit"
};

//static int sym;
typedef struct {
	const char * in;
	const char * last;
	SYMBOL sym;
	int linenum;
	char token[TOK_SIZE];
} Stepper;

typedef enum {
	LT_NONE,
	LT_FULL,
	LT_HI,
	LT_LO

} LABEL_TYPE; 
/* Generated instructions before binary output */
static struct {
	struct {
		uint8_t byte;
		LABEL_TYPE tlabel;
		char label[TOK_SIZE];
		int linenum;
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

static long int parse_int(const char ** expression,const int linenum){


	
	int base = get_base(*expression);
	if(base <= 0) 
		exit_error("error%d: Invalid Immediate\n", linenum);
	if (base!=10)
		(*expression)++;
	return strtol(*expression, expression, base);

	
	
	
}
static int get_precedence(const char c){
	switch (c){
		case '&':
		case '|':
		case '^':
			return 1;
		case '#':
		case '%':
		case '-':
		case '+':
		case '0'...'9':
			return 2;
		case '*':
		case '/':
			return 3;
		default:
			return 0;
	}
}
static bool is_unary_operator(const  char * exp){
	if ((*exp=='-'||*exp=='+') && exp[1] == '(')
		return true;
	if (*exp=='~')
		return true;
	else 
		return false;
	
}
static long int apply_unary_op (const unsigned char op, const long int val, const int linenum){
	switch (op)
	{
	case '+' | 0x80:
		return val;
	case '-' | 0x80 :
		return -val;
	case '~' | 0x80:
		return ~val;
	
	default:
		exit_error("error%d: Invalid Arithmetic Expression\n",linenum);
	}
	/*unreachable*/
	return -1;
}
#define STACK_HEIGHT 64
static long int apply_binary_op(const long int l_op, const char op, const long int r_op, const int linenum){
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
		return l_op|r_op;
	case '^':
		return l_op^r_op;
	
	default:
		exit_error("error%d: Invalid Arithmetic Expression\n",linenum);
	}
	/*unreachable*/
	return -1;

}
static long int evaluate_arithmetic_expression(const char ** expression, const int linenum){

	struct {unsigned char stack[STACK_HEIGHT]; unsigned char *top;} operators; operators.top=operators.stack-1;
	struct {long int stack[STACK_HEIGHT]; long int *top;} figures; 
	*figures.stack=0;figures.top=figures.stack;
	bool is_prev_figure = true;
	bool is_first_char_of_clause = true;
	while (1){
		int prec;
		if (**expression == ' '){
			(*expression)++;
			continue;
		} else if(**expression == '(') {
			//if it's the first char I want to make sure it doesn't try to "bracket" the initial 0 
			if(is_first_char_of_clause) *(++operators.top)='+'; 
			*(++operators.top)=**expression;
			*(++figures.top)=0;
			is_prev_figure=true;
			(*expression)++;
			is_first_char_of_clause=true;
		} else if(**expression == ')') {
			while((*operators.top) != '(')
			{
				if (operators.top<operators.stack)
					exit_error("error%d: Unbalanced Brackets\n", linenum);
				const long int r_op = *(figures.top--);
				const long int l_op = *(figures.top--);
				const unsigned char op = *(operators.top--);
				*(++figures.top)=apply_binary_op(l_op, op, r_op,linenum);
			} 
			is_prev_figure=true;
			operators.top--;
			(*expression)++;
			is_first_char_of_clause=false;
		} else if (is_unary_operator(*expression) && (!is_prev_figure || is_first_char_of_clause)){
			if (is_first_char_of_clause) *(++operators.top) = '+';
			*(++operators.top)=((unsigned char) **expression)|0x80;
			is_prev_figure=false;
			(*expression)++;
			is_first_char_of_clause=false;
		} else if ((prec=get_precedence(**expression))>0){
			while( 
				figures.top>=figures.stack &&
				is_prev_figure && 
				get_precedence(*operators.top)>prec 
			){
				const long int r_op = *(figures.top--);
				const long int l_op = *(figures.top--);
				const unsigned char op = *(operators.top--);
				*(++figures.top) = apply_binary_op(l_op, op, r_op,linenum);
			}
			if (get_base(*expression)>0){
				if(is_prev_figure) {
					*(++operators.top)='+';
				}
				*(++figures.top)=parse_int(expression, linenum); 
				is_prev_figure=true;
				if (*operators.top&0x80){
					(*figures.top) = apply_unary_op(*operators.top, *figures.top, linenum);
					operators.top--;
				}

				
			} else {
				if (is_first_char_of_clause) 
					exit_error("error%d: Invalid Arithmetic Expression\n", linenum);
				*(++operators.top)=(**expression);
				(*expression)++;
				is_prev_figure=false;

			} 
			is_first_char_of_clause=false;
		} else break;
	}

	while(operators.top>=operators.stack){

	if (operators.top<operators.stack)
		exit_error("error%d: Unbalanced Brackets\n", linenum);
		const long int r_op = *(figures.top--);
		const long int l_op = *(figures.top--);
		const unsigned char op = *(operators.top--);
		*(++figures.top)=apply_binary_op(l_op, op, r_op,linenum);
	}
	return *figures.stack;

}

static void emit_b(const Stepper * stepper, uint8_t byte) {
	if(program.next_instr >= TOTAL_RAM)
		exit_error("error: program too large\n");
	program.bytes[program.next_instr].linenum = stepper->linenum;
	program.bytes[program.next_instr++].byte = byte;
	if(program.next_instr > program.max_instr)
		program.max_instr = program.next_instr;
}

static inline void emit(const Stepper * stepper, uint16_t inst) {
	emit_b(stepper, inst >> 8);
	emit_b(stepper, inst & 0xFF);
}
static void emit_lb(const Stepper * stepper, LABEL_TYPE tlabel){
	if(program.next_instr >= TOTAL_RAM)
		exit_error("error: program too large\n");
	program.bytes[program.next_instr].linenum = stepper->linenum;
	strcpy(program.bytes[program.next_instr].label, stepper->token);
	program.bytes[program.next_instr].tlabel=tlabel;
	program.bytes[program.next_instr++].byte = 0;
	if(program.next_instr > program.max_instr)
		program.max_instr = program.next_instr;
}
static void emit_l(const Stepper * stepper, uint16_t inst, LABEL_TYPE tlabel){
	if (tlabel == LT_NONE){
		exit_error("emit_l called on NONE label\n");
	} else if (tlabel == LT_FULL){
		strcpy(program.bytes[program.next_instr].label, stepper->token);
		program.bytes[program.next_instr].tlabel=LT_FULL;
		emit(stepper, inst & 0xf000);
	} else {
		emit_b(stepper, inst>>8);
		emit_lb(stepper, tlabel);
	}
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
	/* TODO: Ought to guard against buffer overruns in tok, but not today. */
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
		for(int i = 0; i < (sizeof inst_names)/(sizeof inst_names[0]); i++)
			if(!strcmp(inst_names[i], stepper->token)) {
				stepper->sym = SYM_INSTRUCTION;
				break;
			}

		if(stepper->sym != SYM_INSTRUCTION) {
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
			else if(!strcmp(stepper->token, "hi"))
				stepper->sym = SYM_HI;
			else if(!strcmp(stepper->token, "lo"))
				stepper->sym = SYM_LO;

			else {
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
	} else if(get_base(stepper->in) || is_unary_operator(stepper->in)) {
		sprintf(stepper->token,"%ld",evaluate_arithmetic_expression(&stepper->in, stepper->linenum));
		stepper->sym=SYM_NUMBER;
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
		exit_error("error:%d: '%c'%d expected\n", stepper->linenum, what,sym); 
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

static int get_addr(const Stepper * stepper) {
	int a = atoi(stepper->token);
	if(a < 0 || a > 0xFFF)
		exit_error("error:%d: invalid addr %d (%03X)\n", stepper->linenum, a, a);
	return a;
}

static int get_byte(const Stepper * stepper) {
	int a = atoi(stepper->token);
	if(a < -128 || a > 0xFF)
		exit_error("error:%d: invalid byte value %d (%02X)\n", stepper->linenum, a, a);
	return a&0xff;
}

static int get_word(const Stepper * stepper) {
	int a = atoi(stepper->token);
	if(a < 0 || a > 0xFFFF)
		exit_error("error:%d: invalid word value %d (%04X)\n", stepper->linenum, a, a);
	return a;
}

#define EMIT_IMMEDIATE_INSTRUCTION(_base_)																\
	if(stepper.sym == SYM_NUMBER)																		\
		emit(&stepper, _base_ | (regx << 8) | get_byte(&stepper));										\
	else if (stepper.sym == SYM_HI){																	\
		nextsym(&stepper);																				\
		if (stepper.sym!=SYM_IDENTIFIER) 																\
			exit_error("error:%d: identifier expected, found %s\n", stepper.linenum, stepper.token);	\
		emit_l(&stepper, _base_ | (regx << 8),LT_HI);													\
	}																									\
	else if (stepper.sym == SYM_LO){ 																	\
		nextsym(&stepper);																				\
		if (stepper.sym!=SYM_IDENTIFIER)																\
			exit_error("error:%d: identifier expected, found %s\n", stepper.linenum, stepper.token); 	\
		emit_l(&stepper, _base_ | (regx << 8),LT_LO);													\
	}

int c8_assemble(const char *text) {

	static Stepper stepper;
	stepper.in = text;

	if(c8_verbose) c8_message("Assembling...\n");


	program.max_instr = 0;

	stepper.linenum = 1;
	stepper.last = NULL;

	n_lookup = 0;
	n_defs = 0;

	memset(program.bytes, 0, sizeof program);
	program.next_instr = 512;

	nextsym(&stepper);
	while(stepper.sym != SYM_END) {
		switch(stepper.sym){
		case SYM_DEFINE:
		{
			char name[TOK_SIZE];
			nextsym(&stepper);
			/* "Identifier expected" may also mean that the name has
				already been used, eg. if aaa is already defined as 123
				then define aaa 456 looks like define 123 456 */
			if(stepper.sym != SYM_IDENTIFIER)
				exit_error("error:%d: identifier expected, found %s\n", stepper.linenum, stepper.token);
			strcpy(name,stepper.token);
			nextsym(&stepper);
			if(stepper.sym != SYM_NUMBER && stepper.sym != SYM_REGISTER)
				exit_error("error:%d: value expected\n", stepper.linenum);
			add_definition(&stepper, name);
			nextsym(&stepper);
		}
		break;
		case SYM_OFFSET:
			nextsym(&stepper);
			if(stepper.sym != SYM_NUMBER)
				exit_error("error:%d: offset expected\n", stepper.linenum);
			program.next_instr = get_addr(&stepper);
			nextsym(&stepper);
		break;
		case SYM_DB:
			do {
				nextsym(&stepper);
				if(stepper.sym == SYM_END)
					break;
				if(stepper.sym != SYM_NUMBER)
					exit_error("error:%d: byte value expected\n", stepper.linenum);
				emit_b(&stepper,get_byte(&stepper));
				nextsym(&stepper);
			} while(stepper.sym == ',');
		break;
		case SYM_DW:
			do {
				nextsym(&stepper);
				if(stepper.sym == SYM_END)
					break;
				if(stepper.sym != SYM_NUMBER)
					exit_error("error:%d: byte value expected\n", stepper.linenum);
				uint16_t word = get_word(&stepper);
				emit(&stepper, word);
				nextsym(&stepper);
			} while(stepper.sym == ',');
		break;
		case SYM_IDENTIFIER:
			add_label(stepper.token, stepper.linenum);
			expect(&stepper, ':');
		break;
		case SYM_INSTRUCTION:
			if(!strcmp("cls", stepper.token))
				emit(&stepper, 0x00E0);
			else if(!strcmp("ret", stepper.token))
				emit(&stepper, 0x00EE);
			else if(!strcmp("jp", stepper.token)) {
				nextsym(&stepper);
				if(stepper.sym == SYM_IDENTIFIER)
					emit_l(&stepper, 0x1000, LT_FULL);
				else if(stepper.sym == SYM_REGISTER) {
					if(strcmp(stepper.token, "v0"))
						exit_error("error:%d: JP applies to V0 only\n", stepper.linenum);
					expect(&stepper, ',');
					if(stepper.sym == SYM_IDENTIFIER)
						emit_l(&stepper, 0xB000, LT_FULL);
					else
						emit(&stepper, 0xB000 | get_addr(&stepper));
				} else
					emit(&stepper, 0x1000 | get_addr(&stepper));
			} else if(!strcmp("call", stepper.token)) {
				nextsym(&stepper);
				if(stepper.sym == SYM_IDENTIFIER)
					emit_l(&stepper, 0x2000, LT_FULL);
				else {
					if(stepper.sym != SYM_NUMBER)
						exit_error("error:%d: address expected", stepper.linenum);
					emit(&stepper, 0x2000 | get_addr(&stepper));
				}
			} else if(!strcmp("se", stepper.token)) {
				nextsym(&stepper);
				int regx = get_register(&stepper);
				expect(&stepper, ',');
				EMIT_IMMEDIATE_INSTRUCTION(0x3000)
				else if(stepper.sym == SYM_REGISTER) {
					int regy = get_register(&stepper);
					emit(&stepper, 0x5000 | (regx << 8) | (regy << 4));
				} else
					exit_error("error:%d: operand expected\n", stepper.linenum);
			} else if(!strcmp("sne", stepper.token)) {
				nextsym(&stepper);
				int regx = get_register(&stepper);
				expect(&stepper, ',');
				EMIT_IMMEDIATE_INSTRUCTION(0x4000)
				else if(stepper.sym == SYM_REGISTER) {
					int regy = get_register(&stepper);
					emit(&stepper, 0x9000 | (regx << 8) | (regy << 4));
				} else
					exit_error("error:%d: operand expected\n", stepper.linenum);
			} else if(!strcmp("add", stepper.token)) {
				nextsym(&stepper);
				if(stepper.sym == SYM_I) {
					expect(&stepper, ',');
					emit(&stepper, 0xF01E | (get_register(&stepper) << 8));
				} else {
					int regx = get_register(&stepper);
					expect(&stepper, ',');
					
					EMIT_IMMEDIATE_INSTRUCTION(0x7000)
					else if(stepper.sym == SYM_REGISTER) {
						int regy = get_register(&stepper);
						emit(&stepper, 0x8004 | (regx << 8) | (regy << 4));
					} else
						exit_error("error:%d: operand expected\n", stepper.linenum);
				}
			} else if(!strcmp("ld", stepper.token)) {
				nextsym(&stepper);
				if(stepper.sym == SYM_I) {
					expect(&stepper, ',');
					if(stepper.sym == SYM_IDENTIFIER)
						emit_l(&stepper, 0xA000, LT_FULL);
					else
						emit(&stepper, 0xA000 | get_addr(&stepper));
				} else if(stepper.sym == SYM_DT) {
					expect(&stepper, ',');
					emit(&stepper, 0xF015 | (get_register(&stepper) << 8));
				} else if(stepper.sym == SYM_ST) {
					expect(&stepper, ',');
					emit(&stepper, 0xF018 | (get_register(&stepper) << 8));
				} else if(stepper.sym == SYM_F) {
					expect(&stepper, ',');
					emit(&stepper, 0xF029 | (get_register(&stepper) << 8));
				} else if(stepper.sym == SYM_B) {
					expect(&stepper, ',');
					emit(&stepper, 0xF033 | (get_register(&stepper) << 8));
				} else if(stepper.sym == '[') {
					if(nextsym(&stepper) != SYM_I || nextsym(&stepper) != ']')
						exit_error("error:%d: [I] expected\n", stepper.linenum);
					if(nextsym(&stepper) != ',')
						exit_error("error:%d: ',' expected\n", stepper.linenum);
					nextsym(&stepper);
					emit(&stepper, 0xF055 | (get_register(&stepper) << 8));
				} else if(stepper.sym == SYM_HF) {
					expect(&stepper, ',');
					emit(&stepper, 0xF030 | (get_register(&stepper) << 8));
				} else if(stepper.sym == SYM_R) {
					expect(&stepper, ',');
					emit(&stepper, 0xF075 | (get_register(&stepper) << 8));
				} else {
					int regx = get_register(&stepper);
					expect(&stepper, ',');
					
					EMIT_IMMEDIATE_INSTRUCTION(0x6000)
					else if(stepper.sym == SYM_REGISTER) {
						int regy = get_register(&stepper);
						emit(&stepper, 0x8000 | (regx << 8) | (regy << 4));
					} else if(stepper.sym == SYM_DT)
						emit(&stepper, 0xF007 | (regx << 8));
					else if(stepper.sym == SYM_K)
						emit(&stepper, 0xF00A | (regx << 8));

					else if(stepper.sym == '[') {
						if(nextsym(&stepper) != SYM_I || nextsym(&stepper) != ']')
							exit_error("error:%d: [I] expected\n", stepper.linenum);
						emit(&stepper, 0xF065 | (regx << 8));
					} else if(stepper.sym == SYM_R) {
						emit(&stepper, 0xF085 | (regx << 8));
					} else
						exit_error("error:%d: operand expected, found %s[%d]\n", stepper.linenum, stepper.token, stepper.sym);
				}
			} else if(!strcmp("or", stepper.token)) {
				nextsym(&stepper);
				int regx = get_register(&stepper);
				expect(&stepper, ',');
				int regy = get_register(&stepper);
				emit(&stepper, 0x8001 | (regx << 8) | (regy << 4));
			} else if(!strcmp("and", stepper.token)) {
				nextsym(&stepper);
				int regx = get_register(&stepper);
				expect(&stepper, ',');
				int regy = get_register(&stepper);
				emit(&stepper, 0x8002 | (regx << 8) | (regy << 4));
			} else if(!strcmp("xor", stepper.token)) {
				nextsym(&stepper);
				int regx = get_register(&stepper);
				expect(&stepper, ',');
				int regy = get_register(&stepper);
				emit(&stepper, 0x8003 | (regx << 8) | (regy << 4));
			} else if(!strcmp("sub", stepper.token)) {
				nextsym(&stepper);
				int regx = get_register(&stepper);
				expect(&stepper, ',');
				int regy = get_register(&stepper);
				emit(&stepper, 0x8005 | (regx << 8) | (regy << 4));
			} else if(!strcmp("shr", stepper.token)) {
				nextsym(&stepper);
				int regx = get_register(&stepper);
				int regy = 0;
				nextsym(&stepper);
				if(stepper.sym == ',') {
					nextsym(&stepper);
					regy = get_register(&stepper);
				} else
					stepper.in=stepper.last;
				emit(&stepper, 0x8006 | (regx << 8) | (regy << 4));
			} else if(!strcmp("subn", stepper.token)) {
				nextsym(&stepper);
				int regx = get_register(&stepper);
				expect(&stepper, ',');
				int regy = get_register(&stepper);
				emit(&stepper, 0x8007 | (regx << 8) | (regy << 4));
			} else if(!strcmp("shl", stepper.token)) {
				nextsym(&stepper);
				int regx = get_register(&stepper);
				int regy=0;
				nextsym(&stepper);
				if(stepper.sym == ',') {
					nextsym(&stepper);
					regy = get_register(&stepper);
				} else
					stepper.in=stepper.last;
				emit(&stepper, 0x800E | (regx << 8) | (regy << 4));
			} else if(!strcmp("rnd", stepper.token)) {
				nextsym(&stepper);
				int regx = get_register(&stepper);
				expect(&stepper, ',');
				EMIT_IMMEDIATE_INSTRUCTION(0xC000)
				else exit_error("error:%d: operand expected\n", stepper.linenum);
			}  else if(!strcmp("drw", stepper.token)) {
				nextsym(&stepper);
				int regx = get_register(&stepper);
				expect(&stepper, ',');
				int regy = get_register(&stepper);
				expect(&stepper, ',');
				int nib = get_byte(&stepper);
				if(nib < 0 || nib > 0xF)
					exit_error("error:%d: invalid value %d\n", stepper.linenum, nib);
				emit(&stepper, 0xD000 | (regx << 8) | (regy << 4) | nib);
			} else if(!strcmp("skp", stepper.token)) {
				nextsym(&stepper);
				emit(&stepper, 0xE09E | (get_register(&stepper) << 8));
			} else if(!strcmp("sknp", stepper.token)) {
				nextsym(&stepper);
				emit(&stepper, 0xE0A1 | (get_register(&stepper) << 8));
			} else if(!strcmp("scd", stepper.token)) {
				nextsym(&stepper);
				int nib = get_byte(&stepper);
				if(nib < 0 || nib > 0xF)
					exit_error("error:%d: invalid value %d\n", stepper.linenum, nib);
				emit(&stepper, 0x00C0 | nib);
			} else if(!strcmp("scr", stepper.token)) {
				emit(&stepper, 0x00FB);
			} else if(!strcmp("scl", stepper.token)) {
				emit(&stepper, 0x00FC);
			} else if(!strcmp("exit", stepper.token)) {
				emit(&stepper, 0x00FD);
			} else if(!strcmp("low", stepper.token)) {
				emit(&stepper, 0x00FE);
			} else if(!strcmp("high", stepper.token)) {
				emit(&stepper, 0x00FF);
			} else if(!strcmp("sys", stepper.token)) {
				nextsym(&stepper);
				emit(&stepper, 0x0000 | get_addr(&stepper));
			}

			nextsym(&stepper);
		break;
		default:
			exit_error("error:%d: unexpected token [%d]: '%s'\n", stepper.linenum, stepper.sym, stepper.token);
		}
	}

	if(c8_verbose) c8_message("Resolving labels...\n");
	size_t n = PROG_OFFSET;
	bool success=false;
	for(int i = PROG_OFFSET; i < program.max_instr; i++) {
		if(program.bytes[i].tlabel) {
			for(int j = 0; j < n_lookup; j++) {
				if(!strcmp(lookup[j].label, program.bytes[i].label)) {
					assert(lookup[j].addr <= 0xFFF);
					switch (program.bytes[i].tlabel)
					{
						case LT_FULL:
						program.bytes[i].byte |= (lookup[j].addr >> 8);
						program.bytes[i + 1].byte = lookup[j].addr & 0xFF;
						break;   
						case LT_HI:
						program.bytes[i].byte = (lookup[j].addr >> 8);
						break;
						case LT_LO:
						program.bytes[i].byte = lookup[j].addr & 0xFF;
						break;

					}
					success=true;
					break;
				}
			}
			if(!success)
				exit_error("error:%d: unresolved label '%s'\n", program.bytes[i].linenum, program.bytes[i].label);
		}
		if(c8_verbose > 1) {
			if(!(i & 0x01))
				c8_message("%03X: %02X", i, program.bytes[i].byte);
			else
				c8_message("%02X\n", program.bytes[i].byte);
		}

		c8_set(n++, program.bytes[i].byte);
	}
	if(c8_verbose > 1 && success)
		c8_message("\n");

	if(c8_verbose) c8_message("Assembled; %d bytes.\n", program.max_instr - PROG_OFFSET);

	return 0;
}
