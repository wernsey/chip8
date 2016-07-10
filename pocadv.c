/*
Compiling with GCC (MinGW):
 $ gcc -I/local/include `sdl-config --cflags` test.c `sdl-config --libs`

Compiling with Emscripten:
 $ emsdk activate latest ; from the emsdk-xxx directory
 $ emcc -O2 test.c -o hello.html
 $ python -m SimpleHTTPServer 8080 &
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>

#include "pocadv.h"

int quit = 0;

#ifdef SDL2
#  if defined(ANDROID) || USE_OPENGL
#    define SCREEN_SCALE	1
#  else
#    define SCREEN_SCALE	2
#  endif
#else
#  define SCREEN_SCALE	1
#endif

#ifndef SDL2
static SDL_Surface *window;
#else
#  if USE_OPENGL
#else
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
#endif
static SDL_Window *window;
#endif

Bitmap *screen;

#ifndef SDL2
char keys[SDLK_LAST];
#else
char keys[SDL_NUM_SCANCODES];
#endif

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
    case KCODE(F12):  {
		/* F12 for screenshots. Doesn't make sense in the browser. */
		char filename[128];
		time_t t;
		struct tm *tmp;

		t = time(NULL);
		tmp = localtime(&t);
		strftime(filename, sizeof filename, "screen-%Y%m%d%H%M%S.bmp", tmp);
		bm_save(screen, filename);
	} return 1;
#endif
    default : return 0;
    }
}

char *read_text_file(const char *fname) {
	FILEOBJ *f;
	long len,r;
	char *str;
    
	if(!(f = FOPEN(fname, "rb")))
		return NULL;

	FSEEK(f, 0, SEEK_END);
	len = FTELL(f);
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
				key_press(event.key.keysym.scancode);
#else
                if(handleKeys(event.key.keysym.sym))
                    break;
                //rlog("Key down: %3u 0x%02X", event.key.keysym.sym, event.key.keysym.sym);
                keys[event.key.keysym.sym] = 1;
				key_press(event.key.keysym.sym);
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
				if(!pointer_click(event.button.x / SCREEN_SCALE, event.button.y / SCREEN_SCALE, finger_id)) {
					pointer_down(event.button.x / SCREEN_SCALE, event.button.y / SCREEN_SCALE, finger_id);
				}
            } break;
            case SDL_MOUSEBUTTONUP: { 
                lastEvent = "Mouse Up";finger_id = 0;
				if(event.button.button != SDL_BUTTON_LEFT) break;
                pointer_up(event.button.x / SCREEN_SCALE, event.button.y / SCREEN_SCALE, finger_id);
            } break;
            case SDL_MOUSEMOTION: { 
                lastEvent = "Mouse Move";finger_id = 0;
				if(!(event.motion.state & SDL_BUTTON(1))) break;
                pointer_move(event.motion.x / SCREEN_SCALE, event.motion.y / SCREEN_SCALE, finger_id);
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

/* Shucks, all my bitmap code expects the pixels to be in
BGRA, but the SDL surface wants it to be in RGBA.  */
static Bitmap *fix_bitmap_bgra(Bitmap *b) {
#if defined(__EMSCRIPTEN__) && 0
    int i;
    if(!b) 
        return NULL;
    for(i = 0; i < b->w * b->h * 4; i += 4) {
		int *p = (int*)(b->data + i);
		*p = (*p & 0xFF00FF00) | ((*p & 0x00FF0000) >> 16) | ((*p & 0x000000FF) << 16);
    }
#endif
    return b;
}

static void draw_frame() {    
    static Uint32 start = 0;
	static Uint32 elapsed = 0;
	
	elapsed = SDL_GetTicks() - start;
	
	if(elapsed < 1) return;
    
#if defined(SDL2) && USE_OPENGL
	SDL_GetWindowSize(window, &w, &h);
#endif

	double deltaTime = elapsed / 1000.0;		
    if(!render(deltaTime)) {
		quit = 1;
	}	
	//fix_bitmap_bgra(screen);
	
    start = SDL_GetTicks();
	
#if 0 /* If you need debug info on the screen : */    
    bm_set_color_s(screen, "white");
    bm_printf(screen, 10, 10, "%d / %.2f", elapsed, deltaTime);
    /* bm_printf(screen, 10, 20, "%s %d", lastEvent, finger_id); */
#else
	(void)elapsed;
#endif
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
    exit(1);
}

static void do_iteration() {
#ifndef SDL2
#  if !USE_OPENGL
    if(SDL_MUSTLOCK(window))
        SDL_LockSurface(window);
	bm_rebind(screen, window->pixels);
#  endif
   
    handle_events();

    draw_frame();

#  if USE_OPENGL
	SDL_GL_SwapWindow(window);
#  else
    if(SDL_MUSTLOCK(window))
        SDL_UnlockSurface(window);
    SDL_Flip(window);
#  endif
#else
    handle_events();
    
    draw_frame();
    
#  if USE_OPENGL
    SDL_GL_SwapWindow(window);
#  else
    SDL_UpdateTexture(texture, NULL, screen->data, screen->w*4);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
#  endif
#endif
}

#if USE_OPENGL
static int setup_view(int width, int height) {
	if(height == 0)
		height = 1;
	rlog("Setup view: %d x %d", width, height);
	/* float ratio = (float)width/(float)height; */
#if !defined(__EMSCRIPTEN__) && !defined(ANDROID)
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (err != GLEW_OK) {
		rerror("Couldn't init GLEW: %s", glewGetErrorString(err));
		return 0;
	}
#endif 
	return 1;
}
#endif

int main(int argc, char *argv[]) {
    
#if defined(SDL2) && USE_OPENGL
	SDL_GLContext glcontext;
	const char *error;
#endif

#ifdef __EMSCRIPTEN__
	logfile = stdout;
#else
	logfile = fopen("pocadv.log", "w");
#endif

    rlog("%s","Pocket Adventure: Application Running");
    
    srand(time(NULL));
    
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
   
#ifdef SDL2
#  if USE_OPENGL
	window = SDL_CreateWindow("Pocket Adventure (SDL2)",
						  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
						  SCREEN_WIDTH * SCREEN_SCALE, SCREEN_HEIGHT * SCREEN_SCALE,
						  SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);		
    if(!window) {
		rerror("%s","SDL_CreateWindow()");
		return 0;
	}
		
	glcontext = SDL_GL_CreateContext(window);
	error = SDL_GetError();
	if(error[0] != '\0') {
		rerror("SDL: %s", error);
		return 1;
	}
	SDL_GL_SetSwapInterval(1);
	
	if(!setup_view(SCREEN_WIDTH, SCREEN_HEIGHT)) return 1;
#  else
    window = SDL_CreateWindow("Pocket Adventure (SDL2)",
                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                          SCREEN_WIDTH * SCREEN_SCALE, SCREEN_HEIGHT * SCREEN_SCALE,
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
	
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
	if(!texture) {
		rerror("%s","SDL_CreateTexture()");
		return 1;
	}
#  endif    
    screen = bm_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    init_game(argc, argv);
#else
/* Using SDL 1.2 */
#  if USE_OPENGL
	SDL_WM_SetCaption("Pocket Adventure (SDL1.2)", "game");
	if(!(window = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_OPENGL))) {
		rerror("Set Video Mode Failed: %s\n", SDL_GetError());
		return 1;
	}
	setup_view(SCREEN_WIDTH, SCREEN_HEIGHT);
    screen = bm_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    init_game(argc, argv);
#else
    SDL_WM_SetCaption("Pocket Adventure (SDL1.2)", "game");
    window = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_SWSURFACE);
    
    if(SDL_MUSTLOCK(window)) {
        SDL_LockSurface(window);
		screen = bm_bind(SCREEN_WIDTH, SCREEN_HEIGHT, window->pixels);
		init_game(argc, argv);
		SDL_UnlockSurface(window);  
	} else {
		screen = bm_bind(SCREEN_WIDTH, SCREEN_HEIGHT, window->pixels);
		init_game(argc, argv);  
	}
#  endif
#endif

#ifdef TEST_SDL_LOCK_OPTS
    EM_ASM("SDL.defaults.copyOnLock = false; SDL.defaults.discardOnLock = true; SDL.defaults.opaqueFrontBuffer = false");
#endif

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(do_iteration, 0, 1);
#else
    rlog("%s","Pocket Adventure: Entering main loop");

    while(!quit) {
        do_iteration();
    }
    
    deinit_game();
    
#endif
    rlog("%s","Pocket Adventure: Main loop stopped");
#ifdef SDL2
#  if USE_OPENGL
	SDL_GL_DeleteContext(glcontext);
#  else	
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
#  endif
	SDL_DestroyWindow(window);
    bm_free(screen);
#else
#  if USE_OPENGL
	bm_free(screen);
#  else
    bm_unbind(screen);
#  endif
#endif

    SDL_Quit();
    
    rlog("%s","Application Done!\n");
    if(logfile != stdout && logfile != stderr)
		fclose(logfile);
    return 0;
}
