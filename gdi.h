#include "bmp.h"

#include "app.h"

extern void init_game(int argc, char *argv[]);
extern void deinit_game();
extern int render(double elapsedSeconds);

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

#define KCODE(x) KEY_ ## x

extern Bitmap *screen;

/* See the KEY_* defines above */
extern char keys[];

/*
If the return value < 128 it is an ASCII code,
otherwise it is special.
*/
int key_pressed();

int mouse_clicked();

void mouse_pos(int *xp, int *yp);

extern void rlog(const char *fmt, ...);

extern void rerror(const char *fmt, ...);

extern void exit_error(const char *msg, ...);

extern char *readfile(const char *fname);
