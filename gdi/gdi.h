#include "../bmp.h"

#include "../app.h"

#define FPS 33

/* You can find the values for various keys in winuser.h
   (they all start with VK_*)
   Don't use them directly; Rather use the KCODE(k) macro below.
*/
#define KEY_ESCAPE  0x1B
#define KEY_SPACE   0x20
#define KEY_LEFT    0x25
#define KEY_UP      0x26
#define KEY_RIGHT   0x27
#define KEY_DOWN    0x28

#define KEY_SHIFT   0x10
#define KEY_LSHIFT  0xA0
#define KEY_RSHIFT  0xA1
#define KEY_CTRL    0x11

#define KEY_HOME    0x24
#define KEY_END     0x23
#define KEY_BKSP    0x08
#define KEY_DELETE      0x2E
#define KEY_PAGEDOWN    0x22
#define KEY_PAGEUP  0x21

#define KEY_F1  0x70
#define KEY_F2  0x71
#define KEY_F3  0x72
#define KEY_F4  0x73
#define KEY_F5  0x74
#define KEY_F6  0x75
#define KEY_F7  0x76
#define KEY_F8  0x77
#define KEY_F9  0x78
#define KEY_F10 0x79
#define KEY_F11 0x7A
#define KEY_F12 0x7B

#define KEY_a 0x41
#define KEY_b 0x42
#define KEY_c 0x43
#define KEY_d 0x44
#define KEY_e 0x45
#define KEY_f 0x46
#define KEY_g 0x47
#define KEY_h 0x48
#define KEY_i 0x49
#define KEY_j 0x4A
#define KEY_k 0x4B
#define KEY_l 0x4C
#define KEY_m 0x4D
#define KEY_n 0x4E
#define KEY_o 0x4F
#define KEY_p 0x50
#define KEY_q 0x51
#define KEY_r 0x52
#define KEY_s 0x53
#define KEY_t 0x54
#define KEY_u 0x55
#define KEY_v 0x56
#define KEY_w 0x57
#define KEY_x 0x58
#define KEY_y 0x59
#define KEY_z 0x5A

#define KEY_LEFTBRACKET     0xDB
#define KEY_RIGHTBRACKET    0xDD
#define KEY_COMMA           0xBC
#define KEY_PERIOD          0xBE
#define KEY_SLASH           0xBF
#define KEY_SEMICOLON       0xBA
#define KEY_BACKSLASH       0xDC
#define KEY_APOSTROPHE      0xDE

#define KCODE(x) KEY_ ## x
#define KCODEA(x,X) KEY_ ## x

extern Bitmap *screen;

/* See the KEY_* defines above */
extern char keys[];

extern int quit;

extern int show_debug();

/*
If the return value < 128 it is an ASCII code,
otherwise it is special.
*/
int key_pressed();

extern int mouse_clicked();
extern int mouse_released();
extern int mouse_down();
extern int mouse_moved();
extern int mouse_x, mouse_y;

extern void set_cursor(Bitmap *b, int hsx, int hsy);

extern void rlog(const char *fmt, ...);

extern void rerror(const char *fmt, ...);

extern void exit_error(const char *msg, ...);

extern char *readfile(const char *fname);

extern Bitmap *get_bmp(const char *filename);

/* These functions should be provided elsewhere */
extern void init_game(int argc, char *argv[]);
extern void deinit_game();
extern int render(double elapsedSeconds);
