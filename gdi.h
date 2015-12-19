#include "bmp.h"

#define APPNAME "Win32/GDI CHIP-8 Machine"

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   (64 + 24)
#define WINDOW_WIDTH 	SCREEN_WIDTH * 2
#define WINDOW_HEIGHT 	SCREEN_HEIGHT * 2
/*
#define WINDOW_WIDTH 160
#define WINDOW_HEIGHT 120
*/

#define FPS 66

/* You can find the values for
   various keys in winuser.h */
#define KEY_ESCAPE 0x1B
#define KEY_SPACE 0x20
#define KEY_LEFT 0x25
#define KEY_UP 0x26
#define KEY_RIGHT 0x27
#define KEY_DOWN 0x28

#define KEY_F1 0x70
#define KEY_F2 0x71
#define KEY_F3 0x72
#define KEY_F4 0x73
#define KEY_F5 0x74
#define KEY_F6 0x75
#define KEY_F7 0x76
#define KEY_F8 0x77
#define KEY_F9 0x78
#define KEY_F10 0x79
#define KEY_F11 0x7A
#define KEY_F12 0x7B

#define KCODE(x) KEY_ ## x

extern Bitmap *screen;

extern char keys[];

char *read_text_file(const char *fname);
