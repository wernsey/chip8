#define TOTAL_RAM 4096

/* Offset of the program in RAM. Should be 512, but
apparently there are some computers where this is 0x600 [1].
 */
#define PROG_OFFSET	512

#define MAX_MESSAGE_TEXT	128

extern int c8_verbose;

void c8_reset();

/* Interpreter */
void c8_step();

int c8_ended();

int c8_waitkey();

/* Debugging */
uint8_t c8_get(uint16_t addr);

void c8_set(uint16_t addr, uint8_t byte);

uint16_t c8_opcode(uint16_t addr);

uint16_t c8_get_pc();

uint16_t c8_prog_size();
uint8_t c8_get_reg(uint8_t r);

/* Graphics */
int c8_screen_updated();

int c8_resolution(int *w, int *h);

int c8_get_pixel(int x, int y);

/* Keyboard routines */
void c8_key_down(uint8_t k);

void c8_key_up(uint8_t k);

void c8_set_keys(uint16_t k);

uint16_t c8_get_keys();

/* Timer and sound functions */
void c8_60hz_tick();

int c8_sound();

/* I/O Routines */
size_t c8_load_program(uint8_t program[], size_t n);

int c8_load_file(const char *fname);

char *c8_load_txt(const char *fname);

int c8_save_file(const char *fname);

/* Error handling */
extern char c8_message_text[];
extern int (*c8_puts)(const char* s);
void c8_message(const char *msg, ...);

/* Assembler */
int c8_assemble(const char *text);

/* Disassembler */
void c8_disasm(); /* FIXME: Output file */
