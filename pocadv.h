
#define WINDOW_CAPTION "CHIP-8 Interpreter"

#ifdef ANDROID
#  include <SDL.h>
#  ifndef SDL2
#    define SDL2
#  endif
#else
#  ifdef SDL2
#    include <SDL2/SDL.h>
#  else
#    include <SDL/SDL.h>
#  endif
#endif

#ifdef SDL2
#  define FILEOBJ   SDL_RWops
#  define FOPEN     SDL_RWFromFile
#  define FCLOSE    SDL_RWclose
#  define FSEEK(ctx, offs, orig)        SDL_RWseek(ctx, offs, RW_ ## orig)
#  define FTELL     SDL_RWtell
#  define FREAD(ptr, size, num, ctx)    SDL_RWread(ctx, ptr, size, num)
#  define REWIND(ctx)                   SDL_RWseek(ctx, 0, RW_SEEK_SET)
#else
#  define FILEOBJ   FILE
#  define FOPEN     fopen
#  define FCLOSE    fclose
#  define FSEEK     fseek
#  define FTELL     ftell
#  define FREAD     fread
#  define REWIND    rewind
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   (64 + 24)

/* Set to one to make ESC quit the game - when debugging */
#define ESCAPE_QUITS	1

#include "bmp.h"

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

extern Bitmap *screen;

#ifndef SDL2
extern char keys[SDLK_LAST];
#define KCODE(k) SDLK_ ## k
#define KCODEA(k,K) SDLK_ ## k
#else
extern char keys[SDL_NUM_SCANCODES];
#define KCODE(k) SDL_SCANCODE_ ## k
#define KCODEA(k,K) SDL_SCANCODE_ ## K
#endif

extern int quit;

extern Bitmap *get_bmp(const char *filename);

extern char *read_text_file(const char *fname);

extern void rlog(const char *fmt, ...); 

extern void rerror(const char *fmt, ...);

extern void exit_error(const char *msg, ...);

/* These functions should be provided elsewhere */
extern void init_game(int argc, char *argv[]);
extern void deinit_game();
extern int render(double deltaTime);

/* Implement these to handle mouse pointer / touchscreen events */
extern int pointer_down(int x, int y, int id);
extern int pointer_up(int x, int y, int id);
extern int pointer_move(int x, int y, int id);
extern int pointer_click(int x, int y, int id);
extern int key_press(int code);
