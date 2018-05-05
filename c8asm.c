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

#include "chip8.h"

#define TOK_SIZE    64

#define MAX_DEFS    256
#define MAX_LOOKUP  256

enum {
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
};

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
    "exit",
    "low",
    "high",
};

static int sym;
static int line;
static const char *in, *last;
static char token[TOK_SIZE];

/* Generated instructions before binary output */
static struct {
    uint8_t byte;
    char *label;
    int line;
} program[TOTAL_RAM];

static uint16_t next_instr; /* Address of next instruction */
static uint16_t max_instr;  /* Largest instruction address for output */

/* Lookup table for labels for JP and CALL instructions */
static struct {
    char *label;
    uint16_t addr;
} lookup[MAX_LOOKUP];
static int n_lookup;

/* Lookup table for DEFINE identifier value statements */
static struct {
    char *name;
    int type;
    char *value;
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

static void emit(uint16_t inst) {
    if(next_instr >= TOTAL_RAM - 1)
        exit_error("error: program too large\n");
    program[next_instr].line = line;
    program[next_instr++].byte = inst >> 8;
    program[next_instr].line = line;
    program[next_instr++].byte = inst & 0xFF;
    if(next_instr > max_instr)
        max_instr = next_instr;
}

static void emit_l(uint16_t inst, const char *label) {
    if(next_instr == TOTAL_RAM)
        exit_error("error: program too large\n");
    program[next_instr].line = line;
    program[next_instr].byte = inst >> 8;
    program[next_instr].label = strdup(label);
    next_instr++;
    program[next_instr].line = line;
    program[next_instr].byte = 0;
    next_instr++;
    if(next_instr > max_instr)
        max_instr = next_instr;
}

static void emit_b(uint8_t byte) {
    if(next_instr >= TOTAL_RAM - 1)
        exit_error("error: program too large\n");
    program[next_instr].line = line;
    program[next_instr++].byte = byte;
    if(next_instr > max_instr)
        max_instr = next_instr;
}

static void add_label(const char *label) {
    int i;
    if(n_lookup == MAX_LOOKUP)
        exit_error("error: too many entries in lookup\n");
    for(i = 0; i < n_lookup; i++)
        if(!strcmp(lookup[i].label, label))
            exit_error("error:%d: duplicate label '%s'\n", line, label);
    lookup[n_lookup].label = strdup(label);
    lookup[n_lookup].addr = next_instr;
    n_lookup++;
}

static void add_definition(char *name, int type, char *value) {
    if(n_defs == MAX_DEFS)
        exit_error("error:%d: too many definitions\n", line);
    defs[n_defs].name = name;
    defs[n_defs].type = type;
    defs[n_defs].value = value;
    n_defs++;
}

static int nextsym() {
    /* TODO: Ought to guard against buffer overruns in tok, but not today. */
    char *tok = token;

    sym = SYM_END;
    *tok = '\0';

scan_start:
    while(isspace(*in)) {
        if(*in == '\n')
            line++;
        in++;
    }

    last = in;
    if(!*in)
        return SYM_END;

    if(*in == ';') {
        while(*in && *in != '\n')
            in++;
        goto scan_start;
    }

    if(isalpha(*in)) {
        int i;
        while(isalnum(*in) || *in == '_')
            *tok++ = tolower(*in++);
        *tok = '\0';
        for(i = 0; i < (sizeof inst_names)/(sizeof inst_names[0]); i++)
            if(!strcmp(inst_names[i], token)) {
                sym = SYM_INSTRUCTION;
                break;
            }
        /* see http://stackoverflow.com/a/15824981/115589
        if(bsearch(token, inst_names, (sizeof inst_names)/(sizeof inst_names[0]), sizeof inst_names[0], myStrCmp)) {
            sym = SYM_INSTRUCTION;
        } */
        if(sym != SYM_INSTRUCTION) {
            if(token[0] == 'v' && isxdigit(token[1]) && !token[2])
                sym = SYM_REGISTER;
            else if(!strcmp(token, "i"))
                sym = SYM_I;
            else if(!strcmp(token, "dt"))
                sym = SYM_DT;
            else if(!strcmp(token, "st"))
                sym = SYM_ST;
            else if(!strcmp(token, "k"))
                sym = SYM_K;
            else if(!strcmp(token, "f"))
                sym = SYM_F;
            else if(!strcmp(token, "b"))
                sym = SYM_B;
            else if(!strcmp(token, "hf"))
                sym = SYM_HF;
            else if(!strcmp(token, "r"))
                sym = SYM_R;
            else if(!strcmp(token, "define"))
                sym = SYM_DEFINE;
            else if(!strcmp(token, "offset"))
                sym = SYM_OFFSET;
            else if(!strcmp(token, "db"))
                sym = SYM_DB;
            else if(!strcmp(token, "dw"))
                sym = SYM_DW;
            else {
                sym = SYM_IDENTIFIER;
                for(i = 0; i < n_defs; i++) {
                    if(!strcmp(defs[i].name, token)) {
                        sym = defs[i].type;
                        strcpy(token, defs[i].value);
                        break;
                    }
                }
            }
        }
    } else if(isdigit(*in)) {
        while(isdigit(*in))
            *tok++ = *in++;
        if(isalnum(*in))
            exit_error("error:%d: invalid number\n", line);
        *tok = '\0';
        sym = SYM_NUMBER;
    } else if(*in == '#') {
        in++;
        while(isxdigit(*in))
            *tok++ = *in++;
        if(isalnum(*in))
            exit_error("error:%d: invalid #hex number\n", line);
        *tok = '\0';
        long x = strtol(token, NULL, 16);
        sprintf(token, "%ld", x);
        sym = SYM_NUMBER;
    } else if(*in == '%') {
        in++;
        while(strchr("01",*in))
            *tok++ = *in++;
        if(isalnum(*in))
            exit_error("error:%d: invalid %%bin number\n", line);
        *tok = '\0';
        long x = strtol(token, NULL, 2);
        sprintf(token, "%ld", x);
        sym = SYM_NUMBER;
    } else {
        token[0] = *in;
        token[1] = '\0';
        sym = *in++;
    }

    return sym;
}

static void pushback() {
    in = last;
}

static void expect(int what) {
    nextsym();
    if(sym != what)
        exit_error("error:%d: '%c' expected\n", line, what);
    nextsym();
}

static int get_register() {
    int reg = token[1];
    if(sym != SYM_REGISTER)
        exit_error("error:%d: register expected\n", line);
    assert(isxdigit(reg));
    if(reg >= 'a') {
        reg = reg - 'a' + 0xA;
    } else {
        reg -= '0';
    }
    assert(reg >= 0 && reg <= 0xF);
    return reg;
}

static int get_addr() {
    int a = atoi(token);
    if(a < 0 || a > 0xFFF)
        exit_error("error:%d: invalid addr %d (%03X)\n", line, a, a);
    return a;
}

static int get_byte() {
    int a = atoi(token);
    if(a < 0 || a > 0xFF)
        exit_error("error:%d: invalid byte value %d (%02X)\n", line, a, a);
    return a;
}

static int get_word() {
    int a = atoi(token);
    if(a < 0 || a > 0xFFFF)
        exit_error("error:%d: invalid word value %d (%04X)\n", line, a, a);
    return a;
}

int c8_assemble(const char *text) {
    int i, j, regx = -1, regy = 0;
    in = text;

    if(c8_verbose) c8_message("Assembling...\n");

    next_instr = 512;
    max_instr = 0;

    line = 1;
    last = NULL;

    n_lookup = 0;
    n_defs = 0;

    memset(program, 0, sizeof program);

    nextsym();
    while(sym != SYM_END) {
        //c8_message("%d %d %s\n", line, sym, token);
        if(sym == SYM_DEFINE) {
            nextsym();
            char *name;
            /* "Identifier expected" may also mean that the name has
                already been used, eg. if aaa is already defined as 123
                then define aaa 456 looks like define 123 456 */
            if(sym != SYM_IDENTIFIER)
                exit_error("error:%d: identifier expected\n", line);
            name = strdup(token);
            nextsym();
            if(sym != SYM_NUMBER && sym != SYM_REGISTER)
                exit_error("error:%d: value expected\n", line);
            add_definition(name, sym, strdup(token));
            nextsym();
        } else if(sym == SYM_OFFSET) {
            nextsym();
            if(sym != SYM_NUMBER)
                exit_error("error:%d: offset expected\n", line);
            next_instr = get_addr();
            nextsym();
        } else if(sym == SYM_DB) {
            do {
                nextsym();
                if(sym == SYM_END)
                    break;
                if(sym != SYM_NUMBER)
                    exit_error("error:%d: byte value expected\n", line);
                emit_b(get_byte());
                nextsym();
            } while(sym == ',');
        } else if(sym == SYM_DW) {
            do {
                nextsym();
                if(sym == SYM_END)
                    break;
                if(sym != SYM_NUMBER)
                    exit_error("error:%d: byte value expected\n", line);
                //emit_b(get_byte());
                uint16_t word = get_word();
                emit_b(word >> 8);
                emit_b(word & 0xFF);
                nextsym();
            } while(sym == ',');
        } else if(sym == SYM_IDENTIFIER) {
            add_label(token);
            expect(':');
        } else if(sym == SYM_INSTRUCTION) {
            if(!strcmp("cls", token))
                emit(0x00E0);
            else if(!strcmp("ret", token))
                emit(0x00EE);
            else if(!strcmp("jp", token)) {
                nextsym();
                if(sym == SYM_IDENTIFIER)
                    emit_l(0x1000, token);
                else if(sym == SYM_REGISTER) {
                    if(strcmp(token, "v0"))
                        exit_error("error:%d: JP applies to V0 only\n", line);
                    expect(',');
                    if(sym == SYM_IDENTIFIER)
                        emit_l(0xB000, token);
                    else
                        emit(0xB000 | get_addr());
                } else
                    emit(0x1000 | get_addr());
            } else if(!strcmp("call", token)) {
                nextsym();
                if(sym == SYM_IDENTIFIER)
                    emit_l(0x2000, token);
                else {
                    if(sym != SYM_NUMBER)
                        exit_error("error:%d: address expected", line);
                    emit(0x2000 | get_addr());
                }
            } else if(!strcmp("se", token)) {
                nextsym();
                regx = get_register();
                expect(',');
                if(sym == SYM_NUMBER)
                    emit(0x3000 | (regx << 8) | get_byte());
                else if(sym == SYM_REGISTER) {
                    regy = get_register();
                    emit(0x5000 | (regx << 8) | (regy << 4));
                } else
                    exit_error("error:%d: operand expected\n", line);
            } else if(!strcmp("sne", token)) {
                nextsym();
                regx = get_register();
                expect(',');
                if(sym == SYM_NUMBER) {
                    emit(0x4000 | (regx << 8) | get_byte());
                } else if(sym == SYM_REGISTER) {
                    regy = get_register();
                    emit(0x9000 | (regx << 8) | (regy << 4));
                } else
                    exit_error("error:%d: operand expected\n", line);
            } else if(!strcmp("add", token)) {
                nextsym();
                if(sym == SYM_I) {
                    expect(',');
                    emit(0xF01E | (get_register() << 8));
                } else {
                    regx = get_register();
                    expect(',');
                    if(sym == SYM_NUMBER) {
                        emit(0x7000 | (regx << 8) | get_byte());
                    } else if(sym == SYM_REGISTER) {
                        regy = get_register();
                        emit(0x8004 | (regx << 8) | (regy << 4));
                    } else
                        exit_error("error:%d: operand expected\n", line);
                }
            } else if(!strcmp("ld", token)) {
                nextsym();
                if(sym == SYM_I) {
                    expect(',');
                    if(sym == SYM_IDENTIFIER)
                        emit_l(0xA000, token);
                    else
                        emit(0xA000 | get_addr());
                } else if(sym == SYM_DT) {
                    expect(',');
                    emit(0xF015 | (get_register() << 8));
                } else if(sym == SYM_ST) {
                    expect(',');
                    emit(0xF018 | (get_register() << 8));
                } else if(sym == SYM_F) {
                    expect(',');
                    emit(0xF029 | (get_register() << 8));
                } else if(sym == SYM_B) {
                    expect(',');
                    emit(0xF033 | (get_register() << 8));
                } else if(sym == '[') {
                    if(nextsym() != SYM_I || nextsym() != ']')
                        exit_error("error:%d: [I] expected\n", line);
                    if(nextsym() != ',')
                        exit_error("error:%d: ',' expected\n", line);
                    nextsym();
                    emit(0xF055 | (get_register() << 8));
                } else if(sym == SYM_HF) {
                    expect(',');
                    emit(0xF030 | (get_register() << 8));
                } else if(sym == SYM_R) {
                    expect(',');
                    emit(0xF075 | (get_register() << 8));
                } else {
                    regx = get_register();
                    expect(',');
                    if(sym == SYM_NUMBER)
                        emit(0x6000 | (regx << 8) | get_byte());
                    else if(sym == SYM_REGISTER) {
                        regy = get_register();
                        emit(0x8000 | (regx << 8) | (regy << 4));
                    } else if(sym == SYM_DT)
                        emit(0xF007 | (regx << 8));
                    else if(sym == SYM_K)
                        emit(0xF00A | (regx << 8));
                    else if(sym == '[') {
                        if(nextsym() != SYM_I || nextsym() != ']')
                            exit_error("error:%d: [I] expected\n", line);
                        emit(0xF065 | (regx << 8));
                    } else if(sym == SYM_R) {
                        emit(0xF085 | (regx << 8));
                    } else
                        exit_error("error:%d: operand expected\n", line);
                }
            } else if(!strcmp("or", token)) {
                nextsym();
                regx = get_register();
                expect(',');
                regy = get_register();
                emit(0x8001 | (regx << 8) | (regy << 4));
            } else if(!strcmp("and", token)) {
                nextsym();
                regx = get_register();
                expect(',');
                regy = get_register();
                emit(0x8002 | (regx << 8) | (regy << 4));
            } else if(!strcmp("xor", token)) {
                nextsym();
                regx = get_register();
                expect(',');
                regy = get_register();
                emit(0x8003 | (regx << 8) | (regy << 4));
            } else if(!strcmp("sub", token)) {
                nextsym();
                regx = get_register();
                expect(',');
                regy = get_register();
                emit(0x8005 | (regx << 8) | (regy << 4));
            } else if(!strcmp("shr", token)) {
                nextsym();
                regx = get_register();
                nextsym();
                if(sym == ',') {
                    nextsym();
                    regy = get_register();
                } else
                    pushback();
                emit(0x8006 | (regx << 8) | (regy << 4));
            } else if(!strcmp("subn", token)) {
                nextsym();
                regx = get_register();
                expect(',');
                regy = get_register();
                emit(0x8007 | (regx << 8) | (regy << 4));
            } else if(!strcmp("shl", token)) {
                nextsym();
                regx = get_register();
                nextsym();
                if(sym == ',') {
                    nextsym();
                    regy = get_register();
                } else
                    pushback();
                emit(0x800E | (regx << 8) | (regy << 4));
            } else if(!strcmp("rnd", token)) {
                nextsym();
                regx = get_register();
                expect(',');
                if(sym != SYM_NUMBER)
                    exit_error("error:%d: operand expected\n", line);
                emit(0xC000 | (regx << 8) | get_byte());
            }  else if(!strcmp("drw", token)) {
                nextsym();
                regx = get_register();
                expect(',');
                regy = get_register();
                expect(',');
                int nib = get_byte();
                if(nib < 0 || nib > 0xF)
                    exit_error("error:%d: invalid value %d\n", line, nib);
                emit(0xD000 | (regx << 8) | (regy << 4) | nib);
            } else if(!strcmp("skp", token)) {
                nextsym();
                emit(0xE09E | (get_register() << 8));
            } else if(!strcmp("sknp", token)) {
                nextsym();
                emit(0xE0A1 | (get_register() << 8));
            } else if(!strcmp("scd", token)) {
                nextsym();
                int nib = get_byte();
                if(nib < 0 || nib > 0xF)
                    exit_error("error:%d: invalid value %d\n", line, nib);
                emit(0x00C0 | nib);
            } else if(!strcmp("scr", token)) {
                emit(0x00FB);
            } else if(!strcmp("scl", token)) {
                emit(0x00FC);
            } else if(!strcmp("exit", token)) {
                emit(0x00FD);
            } else if(!strcmp("low", token)) {
                emit(0x00FE);
            } else if(!strcmp("high", token)) {
                emit(0x00FF);
            } else if(!strcmp("sys", token)) {
#if 1
                /* SYS is not supported in modern emulators */
                exit_error("error:%d: SYS support is disabled\n", line);
#else
                nextsym();
                emit(0x0000 | get_addr());
#endif
            }

            nextsym();
        } else
            exit_error("error:%d: unexpected token [%d]: '%s'\n", line, sym, token);
    }

    if(c8_verbose) c8_message("Resolving labels...\n");
    size_t n = PROG_OFFSET;
    for(i = PROG_OFFSET; i < max_instr; i++) {
        if(program[i].label) {
            for(j = 0; j < n_lookup; j++) {
                if(!strcmp(lookup[j].label, program[i].label)) {
                    assert(lookup[j].addr <= 0xFFF);
                    program[i].byte |= (lookup[j].addr >> 8);
                    program[i + 1].byte = lookup[j].addr & 0xFF;
                    free(program[i].label);
                    program[i].label = NULL;
                    break;
                }
            }
            if(program[i].label)
                exit_error("error:%d: unresolved label '%s'\n", program[i].line, program[i].label);
        }
        if(c8_verbose > 1) {
            if(!(i & 0x01))
                c8_message("%03X: %02X", i, program[i].byte);
            else
                c8_message("%02X\n", program[i].byte);
        }

        c8_set(n++, program[i].byte);
    }
    if(c8_verbose > 1 && i & 0x01)
        c8_message("\n");

    if(c8_verbose) c8_message("Assembled; %d bytes.\n", max_instr - PROG_OFFSET);

    for(i = 0; i < n_lookup; i++) {
        free(lookup[i].label);
    }
    for(i = 0; i < n_defs; i++) {
        free(defs[i].name);
        free(defs[i].value);
    }

    return 1;
}
