#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#if defined(SDL2) || defined(SDL) || defined(__EMSCRIPTEN__)
#  include "sdl/pocadv.h"
#else
#  include <windows.h>
#  include "gdi/gdi.h"
#endif

#ifndef CRT_BLUR
#  define CRT_BLUR	0
#endif
#ifndef CRT_NOISE
#  define CRT_NOISE	0
#endif

#include "chip8.h"
#include "bmp.h"

/* number of instructions to execute per second */
static int speed = 1200;

/* Foreground color */
static int fg_color = 0xAAAAFF;

/* Background color */
static int bg_color = 0x000055;

/* Is the interpreter running? Set to 0 to enter "debug" mode */
static int running = 1;


static Bitmap *chip8_screen;
static Bitmap *hud;

/* These are the same keybindings Octo [10]'s  */
static unsigned int Key_Mapping[16] = {
#if defined(SDL) || defined(SDL2)
    KCODEA(x,X),
    KCODE(1),
    KCODE(2),
    KCODE(3),
    KCODEA(q,Q),
    KCODEA(w,W),
    KCODEA(e,E),
    KCODEA(a,A),
    KCODEA(s,S),
    KCODEA(d,D),
    KCODEA(z,Z),
    KCODEA(c,C),
    KCODE(4),
    KCODEA(r,R),
    KCODEA(f,F),
    KCODEA(v,V)
#else
    0x58, /* '0' -> 'x' */
    0x31, /* '1' -> '1' */
    0x32, /* '2' -> '2' */
    0x33, /* '3' -> '3' */
    0x51, /* '4' -> 'q' */
    0x57, /* '5' -> 'w' */
    0x45, /* '6' -> 'e' */
    0x41, /* '7' -> 'a' */
    0x53, /* '8' -> 's' */
    0x44, /* '9' -> 'd' */
    0x5A, /* 'A' -> 'z' */
    0x43, /* 'B' -> 'c' */
    0x34, /* 'C' -> '4' */
    0x52, /* 'D' -> 'r' */
    0x46, /* 'E' -> 'f' */
    0x56, /* 'F' -> 'v' */
#endif
};

static void draw_screen();

static void usage() {
    exit_error("Use these command line variables:\n"
                "  -f fg  : Foreground color\n"
                "  -b bg  : Background color\n"
                "  -s spd : Specify the speed\n"
                "  -d     : Debug mode\n"
                "  -v     : increase verbosity\n"
                );
}

#ifdef __EMSCRIPTEN__
static int em_ready;
void loaded_callback_func(const char *infile) {
    rlog("Loading %s...", infile);
    if(!c8_load_file(infile)) {
        exit_error("Unable to load '%s': %s\n", infile, strerror(errno));
        return;
    }
    em_ready = 1;
}
void error_callback_func(const char *s) {
    rerror("Error loading %s", s);
}
#endif

void init_game(int argc, char *argv[]) {

    const char *infile = NULL;

    rlog("Initializing...");

    srand(time(NULL));

    c8_reset();

    fg_color = bm_byte_order(fg_color);
    bg_color = bm_byte_order(bg_color);

    int opt;
    while((opt = getopt(argc, argv, "f:b:s:dvh")) != -1) {
        switch(opt) {
            case 'v': c8_verbose++; break;
            case 'f': fg_color = bm_atoi(optarg); break;
            case 'b': bg_color = bm_atoi(optarg); break;
            case 's': speed = atoi(optarg); if(speed < 1) speed = 10; break;
            case 'd': running = 0; break;
            case 'h': usage(); break;
        }
    }

    if(optind >= argc) {
        exit_error("You need to specify a CHIP-8 file.\n");
    }
    infile = argv[optind++];


#ifdef __EMSCRIPTEN__
    em_ready = 0;
    rlog("emscripten_wget retrieving %s", infile);
    //emscripten_wget(infile, infile);
    emscripten_async_wget(infile, infile, loaded_callback_func, error_callback_func);
#else
    rlog("Loading %s...", infile);
    if(!c8_load_file(infile)) {
        exit_error("Unable to load '%s': %s\n", infile, strerror(errno));
    }
#endif

    bm_set_color(screen, 0x202020);
    bm_clear(screen);

    chip8_screen = bm_create(128, 64);

    draw_screen();

#ifdef __EMSCRIPTEN__
    /* I couldn't figure out why this is necessary on the emscripten port: */
    Key_Mapping[0] = KCODEA(x,X);
    Key_Mapping[1] = KCODE(1);
    Key_Mapping[2] = KCODE(2);
    Key_Mapping[3] = KCODE(3);
    Key_Mapping[4] = KCODEA(q,Q);
    Key_Mapping[5] = KCODEA(w,W);
    Key_Mapping[6] = KCODEA(e,E);
    Key_Mapping[7] = KCODEA(a,A);
    Key_Mapping[8] = KCODEA(s,S);
    Key_Mapping[9] = KCODEA(d,D);
    Key_Mapping[10] = KCODEA(z,Z);
    Key_Mapping[11] = KCODEA(c,C);
    Key_Mapping[12] = KCODE(4);
    Key_Mapping[13] = KCODEA(r,R);
    Key_Mapping[14] = KCODEA(f,F);
    Key_Mapping[15] = KCODEA(v,V);
#endif

    hud = bm_create(128, 24);
    if(!hud)
        exit_error("unable to create HUD");

    rlog("Initialized.");
}

void deinit_game() {
    bm_free(hud);
    bm_free(chip8_screen);
    rlog("Done.");
}

#if CRT_BLUR
static void add_bitmaps(Bitmap *b1, Bitmap *b2) {
    int x,y;
    assert(b1->w == b2->w && b1->h == b2->h);
    for(y = 0; y < b1->h; y++) {
        for(x = 0; x < b1->w; x++) {
            unsigned int c1 = bm_get(b1, x, y);
            unsigned int c2 = bm_get(b2, x, y);
            unsigned int c3 = bm_lerp(c1, c2, 0.6);
            bm_set(b1, x, y, c3);
        }
    }
}

static unsigned char oldscreen_buffer[SCREEN_WIDTH * SCREEN_HEIGHT * 4];
static unsigned char plotscreen_buffer[SCREEN_WIDTH * SCREEN_HEIGHT * 4];
#endif
#if CRT_NOISE
static long nz_seed = 0L;
static long nz_rand() {
    nz_seed = nz_seed * 1103515245L + 12345L;
    return (nz_seed >> 16) & 0x7FFF;
}
static void nz_srand(long seed) {
    nz_seed = seed;
}
static unsigned int noise(int x, int y, unsigned int col_in) {
    unsigned char R, G, B;
    bm_get_rgb(col_in, &R, &G, &B);
    /*
    https://en.wikipedia.org/wiki/Linear_congruential_generator
    */
    int val = (int)(nz_rand() & 0x7) - 4;
    if(x & 0x01) val-=4;
    if(y & 0x02) val-=4;

    int iR = R + val, iG = G + val, iB = B + val;
    if(iR > 0xFF) iR = 0xFF;
    if(iR < 0) iR = 0;
    if(iG > 0xFF) iG = 0xFF;
    if(iG < 0) iG = 0;
    if(iB > 0xFF) iB = 0xFF;
    if(iB < 0) iB = 0;
    return bm_rgb(iR, iG, iB);
}
#endif

static void chip8_to_bmp(Bitmap *sbmp) {
    int x, y, w, h;

    c8_resolution(&w, &h);

    assert(w <= bm_width(sbmp));
    assert(h <= bm_height(sbmp));
    //bm_bind_static(sbmp, chip8_screen_buffer, w, h);

    for(y = 0; y < h; y++) {
        for(x = 0; x < w; x++) {
            unsigned int c = c8_get_pixel(x,y) ? fg_color : bg_color;
            bm_set(sbmp, x, y, c);
        }
    }
}

static void draw_screen() {
    int w, h;

    chip8_to_bmp(chip8_screen);
    c8_resolution(&w, &h);

#if CRT_BLUR
    /* FIXME: This won't work anymore on the new BMP API */
    Bitmap plotscreen;
    bm_bind_static(&plotscreen, plotscreen_buffer, SCREEN_WIDTH, SCREEN_HEIGHT);

    Bitmap oldscreen;
    bm_bind_static(&oldscreen, oldscreen_buffer, SCREEN_WIDTH, SCREEN_HEIGHT);
    memcpy(oldscreen.data, plotscreen.data, SCREEN_WIDTH * SCREEN_HEIGHT * 4);

    bm_smooth(&oldscreen);
    bm_smooth(&oldscreen);
    bm_blit_ex(screen, 0, 0, screen->w, screen->h, &chip8_screen, 0, 0, w, h, 0);
    add_bitmaps(screen, &oldscreen);

    float smooth_kernel[] = { 0.0, 0.1, 0.0,
                              0.1, 0.6, 0.1,
                              0.0, 0.1, 0.0};
    bm_apply_kernel(screen, 3, smooth_kernel);
#else
    bm_blit_ex(screen, 0, 0, bm_width(screen), bm_height(screen), chip8_screen, 0, 0, w, h, 0);
#endif

#if CRT_NOISE
    int x, y;
    nz_srand(1234);
    for(y = 0; y < screen->h; y++) {
        for(x = 0; x < screen->w; x++) {
            unsigned int c = bm_get(screen, x, y);
            c = noise(x, y, c);
            bm_set(screen, x, y, c);
        }
     }
#endif

#if CRT_BLUR
    memcpy(plotscreen.data, screen->data, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
#endif
}

void bm_blit_blend(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h);


void draw_hud() {
    int i;

    // Bitmap hud;
    // static unsigned char hud_buffer[128 * 24 * 4];
    // bm_bind_static(&hud, hud_buffer, 128, 24);

    uint16_t pc = c8_get_pc();
    uint16_t opcode = c8_opcode(pc);
    bm_set_color(hud, 0x202020);
    bm_clear(hud);
    bm_set_color(hud, 0xFFFFFF);
    bm_printf(hud, 1, 0, "%03X %04X", pc, opcode);
    for(i = 0; i < 16; i++) {
        bm_printf(hud, (i & 0x07) * 16, (i >> 3) * 8 + 8, "%02X", c8_get_reg(i));
    }

    bm_blit_blend(screen, 0, bm_height(screen) - 24, hud, 0, 0, bm_width(hud), bm_height(hud));
}

int render(double elapsedSeconds) {
    int i;
    static double timer = 0.0;

#ifdef __EMSCRIPTEN__
    if(!em_ready) return 1;
#endif

    int key_pressed = 0;
    for(i = 0; i < 16; i++) {
        int k = Key_Mapping[i];
        if(keys[k]) {
            key_pressed = 1;
            c8_key_down(i);
#if !defined(NDEBUG) && 0
            rlog("key pressed: %X 0x%02X", i, k);
#endif
        } else
            c8_key_up(i);
    }

    timer += elapsedSeconds;
    while(timer > 1.0/60.0) {
        c8_60hz_tick();
        timer -= 1.0/60.0;
    }

    if(running) {
        /* F5 breaks the program and enters debugging mode */
        if(keys[KCODE(F5)])
            running = 0;

        /* instructions per second * elapsed seconds = number of instructions to execute */
        int count = speed * elapsedSeconds;
        for(i = 0; i < count; i++) {
            if(c8_ended())
                return 0;
            else if(c8_waitkey() && !key_pressed)
                return 1;

            c8_step();

            if(c8_screen_updated())
                draw_screen();
        }
    } else {
        /* Debugging mode:
            F6 steps through the program
            F8 resumes
        */
        if(keys[KCODE(F8)]) {
            // bm_set_color(screen, 0x202020);
            // bm_fillrect(screen, 0, screen->h - 24, screen->w, screen->h);
            running = 1;
            return 1;
        }
        if(keys[KCODE(F6)]) {
            if(c8_ended())
                return 0;
            else if(c8_waitkey() && !key_pressed)
                return 1;
            c8_step();
            if(c8_screen_updated()) {
                draw_screen();
            }
            keys[KCODE(F6)] = 0;
        }

        draw_screen();
        draw_hud();
    }

    return 1;
}


void bm_blit_blend(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h) {
    int x,y, i, j;

    BmRect destClip = bm_get_clip(dst);

    if(sx < 0) {
        int delta = -sx;
        sx = 0;
        dx += delta;
        w -= delta;
    }

    if(dx < destClip.x0) {
        int delta = destClip.x0 - dx;
        sx += delta;
        w -= delta;
        dx = destClip.x0;
    }

    if(sx + w > bm_width(src)) {
        int delta = sx + w - bm_width(src);
        w -= delta;
    }

    if(dx + w > destClip.x1) {
        int delta = dx + w - destClip.x1;
        w -= delta;
    }

    if(sy < 0) {
        int delta = -sy;
        sy = 0;
        dy += delta;
        h -= delta;
    }

    if(dy < destClip.y0) {
        int delta = destClip.y0 - dy;
        sy += delta;
        h -= delta;
        dy = destClip.y0;
    }

    if(sy + h > bm_height(src)) {
        int delta = sy + h - bm_height(src);
        h -= delta;
    }

    if(dy + h > destClip.y1) {
        int delta = dy + h - destClip.y1;
        h -= delta;
    }

    if(w <= 0 || h <= 0)
        return;
    if(dx >= destClip.x1 || dx + w < destClip.x0)
        return;
    if(dy >= destClip.y1 || dy + h < destClip.y0)
        return;
    if(sx >= bm_width(src) || sx + w < 0)
        return;
    if(sy >= bm_height(src) || sy + h < 0)
        return;

    if(sx + w > bm_width(src)) {
        int delta = sx + w - bm_width(src);
        w -= delta;
    }

    if(sy + h > bm_height(src)) {
        int delta = sy + h - bm_height(src);
        h -= delta;
    }

    assert(dx >= 0 && dx + w <= destClip.x1);
    assert(dy >= 0 && dy + h <= destClip.y1);
    assert(sx >= 0 && sx + w <= bm_width(src));
    assert(sy >= 0 && sy + h <= bm_height(src));

    j = sy;
    for(y = dy; y < dy + h; y++) {
        i = sx;
        for(x = dx; x < dx + w; x++) {
            unsigned int c1 = (bm_get(src, i, j) >> 1) & 0x7F7F7F;
            unsigned int c2 = (bm_get(dst, x, y) >> 1) & 0x7F7F7F;
            bm_set(dst, x, y, c1 + c2);
            i++;
        }
        j++;
    }
}
