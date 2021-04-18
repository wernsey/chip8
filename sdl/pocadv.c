/*
 * Pocket Adventure
 * ================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <assert.h>

#include "pocadv.h"

#if SCREEN_SCALE == 0
/* This has happened to me more than once */
#  error "You set SCREEN_SCALE to 0 again, dummy!"
#endif

#define VSCREEN_WIDTH    (SCREEN_WIDTH * (EPX_SCALE?2:1))
#define VSCREEN_HEIGHT   (SCREEN_HEIGHT * (EPX_SCALE?2:1))
#define WINDOW_WIDTH    (VSCREEN_WIDTH * SCREEN_SCALE)
#define WINDOW_HEIGHT   (VSCREEN_HEIGHT * SCREEN_SCALE)

#ifndef USE_LOG_STREAM
#  define USE_LOG_STREAM 0
#else
#  ifndef LOG_STREAM
#    define LOG_STREAM   stderr
#  endif
#endif

#ifndef LOG_FILE_NAME
#  define LOG_FILE_NAME "sdl-game.log"
#endif

#ifndef SDL2
static SDL_Surface *window;
#else
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static SDL_Window *window;
#endif

Bitmap *screen;
#ifndef SDL2
Bitmap *vscreen;
#endif

#ifndef SDL2
char keys[SDLK_LAST];
#else
char keys[SDL_NUM_SCANCODES];
#endif
static int pressed_key = 0;

int mouse_x, mouse_y;
static int mclick = 0, mdown = 0, mrelease = 0, mmove = 0;

static Bitmap *cursor = NULL;
static int cursor_hsx, cursor_hsy;
static Bitmap *cursor_back = NULL;

#if EPX_SCALE
static Bitmap *scale_epx_i(Bitmap *in, Bitmap *out);
static Bitmap *epx;
#endif

static int dodebug = 0;
static double frameTimes[256];
static Uint32 n_elapsed = 0;

int quit = 0;

/* This leaves a bit to be desired if I'm to
support multi-touch on mobile eventually */
int mouse_clicked() {
    return mclick;
}
int mouse_down() {
    return mdown;
}
int mouse_released() {
    return mrelease;
}
int mouse_moved() {
    return mmove;
}
int key_pressed() {
    return pressed_key;
}

int show_debug() {
// #ifdef NDEBUG
    // return 0;
// #else
    return dodebug;
// #endif
}

/* Handle special keys */
#ifdef SDL2
static int handleKeys(SDL_Scancode key) {
#else
static int handleKeys(SDLKey key) {
#endif
    switch(key) {
#if !defined(__EMSCRIPTEN__) && ESCAPE_QUITS
    case KCODE(ESCAPE) : quit = 1; return 1;
#endif
    /* TODO: F11 for fullscreen, etc. */
#if !defined(__EMSCRIPTEN__)
    case KCODE(F10):  dodebug = !dodebug; return 1;
    case KCODE(F12):  {
        /* F12 for screenshots; Doesn't make sense in the browser. */
        char filename[128];

#ifdef _MSC_VER
        time_t t = time(NULL);
        struct tm buf, *ptr = &buf;
        localtime_s(ptr, &t);
#else
        time_t t;
        struct tm *ptr;
        t = time(NULL);
        ptr = localtime(&t);
#endif
        strftime(filename, sizeof filename, "screen-%Y%m%d%H%M%S.bmp", ptr);

        bm_save(screen, filename);
    } return 1;
#endif
    default : return 0;
    }
}

char *readfile(const char *fname) {
    FILEOBJ *f;
    long len,r;
    char *str;

    if(!(f = FOPEN(fname, "rb")))
        return NULL;

    FSEEK(f, 0, SEEK_END);
    len = (long)FTELL(f);
    REWIND(f);

    if(!(str = malloc(len+2)))
        return NULL;
    r = FREAD(str, 1, len, f);

    if(r != len) {
        free(str);
        return NULL;
    }

    FCLOSE(f);
    str[len] = '\0';
    return str;
}

void set_cursor(Bitmap *b, int hsx, int hsy) {
    cursor_hsx = hsx;
    cursor_hsy = hsy;
    cursor = b;
    int w = bm_width(b), h= bm_height(b);
    if(b) {
        if(!cursor_back)
            cursor_back = bm_create(w, h);
        else if(bm_width(cursor_back) != w || bm_height(cursor_back) != h) {
            bm_free(cursor_back);
            cursor_back = bm_create(w, h);
        }
        SDL_ShowCursor(SDL_DISABLE);
    } else {
        bm_free(cursor_back);
        cursor_back = NULL;
        SDL_ShowCursor(SDL_ENABLE);
    }
}

static const char *lastEvent = "---";
static int finger_id = -1;

static void handle_events() {

    SDL_Event event;

    while(SDL_PollEvent(&event)) {
        switch(event.type) {
            case SDL_KEYDOWN: {
                lastEvent = "Key Down"; finger_id=-1;
#ifdef SDL2
                if(handleKeys(event.key.keysym.scancode))
                    break;
                keys[event.key.keysym.scancode] = 1;
                pressed_key = event.key.keysym.sym;
                if(!(pressed_key & 0x40000000)) {
                    if(event.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT)) {
                        if(isalpha(pressed_key)) {
                            pressed_key = toupper(pressed_key);
                        } else {
                            /* This would not work with different keyboard layouts */
                            static const char *in  = "`1234567890-=[],./;'\\";
                            static const char *out = "~!@#$%^&*()_+{}<>?:\"|";
                            char *p = strchr(in, pressed_key);
                            if(p) {
                                pressed_key = out[p-in];
                            }
                        }
                    } else if (pressed_key == SDLK_DELETE) {
                        // The Del key is a bit special...
                        pressed_key = 0x40000000 | SDL_SCANCODE_DELETE;
                    }
                }
#else
                if(handleKeys(event.key.keysym.sym))
                    break;
                keys[event.key.keysym.sym] = 1;
                pressed_key = event.key.keysym.sym;
                if(pressed_key > 0xFF) {
                    pressed_key |= 0x40000000;
                } else if(event.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT)) {
                    if(isalpha(pressed_key)) {
                        pressed_key = toupper(pressed_key);
                    } else {
                        /* This would not work with different keyboard layouts */
                        static const char *in  = "`1234567890-=[],./;'\\";
                        static const char *out = "~!@#$%^&*()_+{}<>?:\"|";
                        char *p = strchr(in, pressed_key);
                        if(p) {
                            pressed_key = out[p-in];
                        }
                    }
                } else if (pressed_key == SDLK_DELETE) {
                    pressed_key = 0x40000000 | SDLK_DELETE;
                }
#endif
            } break;
            case SDL_KEYUP: {
                lastEvent = "Key Up";finger_id=-1;
#ifdef SDL2
                keys[event.key.keysym.scancode] = 0;
#else
                keys[event.key.keysym.sym] = 0;
#endif
            } break;
#ifndef ANDROID /* Ignore the mouse on android, that's what the touch events are for */
            case SDL_MOUSEBUTTONDOWN: {
                lastEvent = "Mouse Down";finger_id = 0;
                if(event.button.button != SDL_BUTTON_LEFT) break;
                mdown = 1;
                mrelease = 0;
                mclick = 1;
            } break;
            case SDL_MOUSEBUTTONUP: {
                lastEvent = "Mouse Up";finger_id = 0;
                if(event.button.button != SDL_BUTTON_LEFT) break;
                mdown = 0;
                mrelease = 1;
            } break;
            case SDL_MOUSEMOTION: {
                lastEvent = "Mouse Move";finger_id = 0;
                mouse_x = event.button.x * SCREEN_WIDTH / WINDOW_WIDTH;
                mouse_y = event.button.y * SCREEN_HEIGHT / WINDOW_HEIGHT;
                mmove = 1;
            } break;
#endif
#if defined(SDL2) && defined(ANDROID)
            case SDL_FINGERDOWN: {
                lastEvent = "Finger Down";finger_id=event.tfinger.fingerId;
                int x = (int)(event.tfinger.x * SCREEN_WIDTH), y = (int)(event.tfinger.y * SCREEN_HEIGHT);
                if(!pointer_click(x, y,finger_id)) {
                    pointer_down(x, y,finger_id);
                }
            } break;
            case SDL_FINGERUP: {
                lastEvent = "Finger Up";finger_id=event.tfinger.fingerId;
                int x = (int)(event.tfinger.x * SCREEN_WIDTH), y = (int)(event.tfinger.y * SCREEN_HEIGHT);
                pointer_up(x, y,finger_id);
            } break;
            case SDL_FINGERMOTION: {
                lastEvent = "Finger Motion";finger_id=event.tfinger.fingerId;
                int x = (int)(event.tfinger.x * SCREEN_WIDTH), y = (int)(event.tfinger.y * SCREEN_HEIGHT);
                pointer_move(x, y,finger_id);
            } break;
#endif
            case SDL_QUIT: {
                quit = 1;
            } break;
        }
    }
}

Bitmap *get_bmp(const char *filename) {
#ifdef ANDROID
    SDL_RWops *file = SDL_RWFromFile(filename, "rb");
    Bitmap *bmp = bm_load_rw(file);
    SDL_RWclose(file);
#else
    Bitmap *bmp = bm_load(filename);
#endif
    return bmp;
}

static void draw_frame() {
    static Uint32 start = 0;
    static Uint32 elapsed = 0;

    elapsed = SDL_GetTicks() - start;

    /* It is technically possible for the game to run
       too fast, rendering the deltaTime useless */
    if(elapsed < 10)
        return;

    double deltaTime = elapsed / 1000.0;
    if(!render(deltaTime)) {
        quit = 1;
    }

    start = SDL_GetTicks();

    if(dodebug && n_elapsed > 0) {
        double sum = 0;
        int i, n = n_elapsed > 0xFF ? 0xFF : n_elapsed;
        for(i = 0; i < n; i++) sum += frameTimes[i];
        double avg = sum / n;
        double fps = 1.0 / avg;
        BmFont *save = bm_get_font(screen);
        bm_reset_font(screen);
        bm_set_color(screen, bm_atoi("red"));
        bm_fillrect(screen, 0, 0, 50, 10);
        bm_set_color(screen, bm_atoi("yellow"));
        bm_printf(screen, 1, 1, "%3.2f", fps);
        bm_set_font(screen, save);
    }
    frameTimes[(n_elapsed++) & 0xFF] = deltaTime;

#if EPX_SCALE
    scale_epx_i(screen, epx);
#endif

    mclick = 0;
    mrelease = 0;
    mmove = 0;
    pressed_key = 0;
}

#define USE_SDL_LOG 0

static FILE *logfile = NULL;

void rlog(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
#if defined(SDL2) && USE_SDL_LOG
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, arg);
#else
    fputs("INFO: ", logfile);
    vfprintf(logfile, fmt, arg);
    fputc('\n', logfile);
#endif
    va_end(arg);
}

void rerror(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
#if defined(SDL2) && USE_SDL_LOG
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, fmt, arg);
#else
    fputs("ERROR: ", logfile);
    vfprintf(logfile, fmt, arg);
    fputc('\n', logfile);
#endif
    va_end(arg);
}

void exit_error(const char *fmt, ...) {
    if(fmt) {
        va_list arg;
        va_start (arg, fmt);
#if defined(SDL2) && USE_SDL_LOG
        SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, fmt, arg);
#else
        fputs("ERROR: ", logfile);
        vfprintf (logfile, fmt, arg);
#endif
        va_end (arg);
    }
    if(logfile != stdout && logfile != stderr)
        fclose(logfile);
    else {
        fflush(stdout);
        fflush(stderr);
    }
    exit(1);
}

static void do_iteration() {
    int cx = 0, cy = 0;

#ifndef SDL2
    if(SDL_MUSTLOCK(window))
        SDL_LockSurface(window);
    bm_rebind(vscreen, window->pixels);
    handle_events();

    draw_frame();

    if(cursor) {
        cx = mouse_x - cursor_hsx;
        cy = mouse_y - cursor_hsy;

        int cw = bm_width(cursor), ch = bm_height(cursor);

        bm_blit(cursor_back, 0, 0, screen, cx, cy, cw, ch);
        bm_maskedblit(screen, cx, cy, cursor, 0, 0, cw, ch);
    }

#  if EPX_SCALE
    bm_blit_ex(vscreen, 0, 0, bm_width(vscreen), bm_height(vscreen), epx, 0, 0, bm_width(epx), bm_height(epx), 0);
#  else
    bm_blit_ex(vscreen, 0, 0, bm_width(vscreen), bm_height(vscreen), screen, 0, 0, bm_width(screen), bm_height(screen), 0);
#  endif

    if(SDL_MUSTLOCK(window))
        SDL_UnlockSurface(window);
    SDL_Flip(window);
#else
    handle_events();

    draw_frame();

    if(cursor) {
        cx = mouse_x - cursor_hsx;
        cy = mouse_y - cursor_hsy;
        int cw = bm_width(cursor), ch = bm_height(cursor);
        bm_blit(cursor_back, 0, 0, screen, cx, cy, cw, ch);
        bm_maskedblit(screen, cx, cy, cursor, 0, 0, cw, ch);
    }

#  if EPX_SCALE
    SDL_UpdateTexture(texture, NULL, bm_data(epx), bm_width(epx)*4);
#  else
    SDL_UpdateTexture(texture, NULL, bm_raw_data(screen), bm_width(screen)*4);
#  endif
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
#endif

    if(cursor) {
        bm_maskedblit(screen, cx, cy, cursor_back, 0, 0, bm_width(cursor_back), bm_height(cursor_back));
    }
}

int main(int argc, char *argv[]) {

#ifdef __EMSCRIPTEN__
    logfile = stdout;
#else
    //logfile = fopen("pocadv.log", "w");

#  ifdef _MSC_VER
    errno_t err = fopen_s(&logfile, LOG_FILE_NAME, "w");
    if (err != 0)
        return 1;
#  elif USE_LOG_STREAM
	logfile = LOG_STREAM;
#  else
    logfile = fopen(LOG_FILE_NAME, "w");
    if (!logfile)
        return 1;
#  endif
#endif

    rlog("%s: Application Running", WINDOW_CAPTION);

    srand((unsigned int)time(NULL));

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

#ifdef SDL2
    window = SDL_CreateWindow(WINDOW_CAPTION " - SDL2",
                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                          WINDOW_WIDTH, WINDOW_HEIGHT,
                          SDL_WINDOW_SHOWN);
    if(!window) {
        rerror("%s","SDL_CreateWindow()");
        return 0;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if(!renderer) {
        rerror("%s","SDL_CreateRenderer()");
        return 1;
    }

#  if EPX_SCALE
    epx = bm_create(VSCREEN_WIDTH, VSCREEN_HEIGHT);
    screen = bm_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, VSCREEN_WIDTH, VSCREEN_HEIGHT);
    if(!texture) {
        rerror("%s","SDL_CreateTexture()");
        return 1;
    }
#  else
    screen = bm_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    if(!texture) {
        rerror("%s","SDL_CreateTexture()");
        return 1;
    }
#  endif

    init_game(argc, argv);
#else
    /* Using SDL 1.2 */
    SDL_WM_SetCaption(WINDOW_CAPTION " - SDL1.2", "game");
    window = SDL_SetVideoMode(WINDOW_WIDTH, WINDOW_HEIGHT, 32, SDL_SWSURFACE);

    screen = bm_create(SCREEN_WIDTH, SCREEN_HEIGHT);
#  if EPX_SCALE
    epx = bm_create(VSCREEN_WIDTH, VSCREEN_HEIGHT);
#  endif
    if(SDL_MUSTLOCK(window)) {
        SDL_LockSurface(window);
        vscreen = bm_bind(WINDOW_WIDTH, WINDOW_HEIGHT, window->pixels);
        init_game(argc, argv);
        SDL_UnlockSurface(window);
    } else {
        vscreen = bm_bind(WINDOW_WIDTH, WINDOW_HEIGHT, window->pixels);
        init_game(argc, argv);
    }

    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#endif

#ifdef TEST_SDL_LOCK_OPTS
    EM_ASM("SDL.defaults.copyOnLock = false; SDL.defaults.discardOnLock = true; SDL.defaults.opaqueFrontBuffer = false");
#endif

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(do_iteration, 0, 1);
#else
    rlog("%s: Entering main loop", WINDOW_CAPTION);

    while(!quit) {
        do_iteration();
    }

    deinit_game();

#endif
    rlog("%s: Main loop stopped", WINDOW_CAPTION);
#ifdef SDL2
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    bm_free(screen);
#  if EPX_SCALE
    bm_free(epx);
#  endif

#else
    bm_unbind(vscreen);
    bm_free(screen);
#  if EPX_SCALE
    bm_free(epx);
#  endif

#endif

    SDL_Quit();

    rlog("%s","Application Done!\n");
#if !USE_LOG_STREAM
    fclose(logfile);
#endif
    return 0;
}

/* EPX 2x scaling */
#if EPX_SCALE
static Bitmap *scale_epx_i(Bitmap *in, Bitmap *out) {
    int x, y, mx = in->w, my = in->h;
    if(!out) return NULL;
    if(!in) return out;
    if(out->w < (mx << 1)) mx = (out->w - 1) >> 1;
    if(out->h < (my << 1)) my = (out->h - 1) >> 1;
    for(y = 0; y < my; y++) {
        for(x = 0; x < mx; x++) {
            unsigned int P = bm_get(in, x, y);
            unsigned int A = (y > 0) ? bm_get(in, x, y - 1) : P;
            unsigned int B = (x < in->w - 1) ? bm_get(in, x + 1, y) : P;
            unsigned int C = (x > 0) ? bm_get(in, x - 1, y) : P;
            unsigned int D = (y < in->h - 1) ? bm_get(in, x, y + 1) : P;

            unsigned int P1 = P, P2 = P, P3 = P, P4 = P;

            if(C == A && C != D && A != B) P1 = A;
            if(A == B && A != C && B != D) P2 = B;
            if(B == D && B != A && D != C) P4 = D;
            if(D == C && D != B && C != A) P3 = C;

            bm_set(out, (x << 1), (y << 1), P1);
            bm_set(out, (x << 1) + 1, (y << 1), P2);
            bm_set(out, (x << 1), (y << 1) + 1, P3);
            bm_set(out, (x << 1) + 1, (y << 1) + 1, P4);
        }
    }
    return out;
}
#endif
