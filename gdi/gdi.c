/**
 * Win32 application that allows rendering directly to a GDI contexts
 * through my bitmap module.
 * (for simple games without any third party dependencies)
 *
 * ## References:
 *
 * * The bitmap module: https://github.com/wernsey/bitmap
 * * https://www.daniweb.com/software-development/cpp/code/241875/fast-animation-with-the-windows-gdi
 * * https://www-user.tu-chemnitz.de/~heha/petzold/ch14e.htm
 * * https://www-user.tu-chemnitz.de/~heha/petzold/ch15d.htm
 * * http://forums.codeguru.com/showthread.php?487633-32-bit-DIB-from-24-bit-bitmap
 * * HELLO_WIN.C example of the tcc C compiler
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define __STDC_WANT_LIB_EXT1__ 1

#include <windows.h>

#include "../bmp.h"
#include "gdi.h"

#define VSCREEN_WIDTH    (SCREEN_WIDTH * (EPX_SCALE?2:1))
#define VSCREEN_HEIGHT   (SCREEN_HEIGHT * (EPX_SCALE?2:1))
#define WINDOW_WIDTH    (VSCREEN_WIDTH * SCREEN_SCALE)
#define WINDOW_HEIGHT   (VSCREEN_HEIGHT * SCREEN_SCALE)

/* fflush() the log file after each call to rlog()?
I only use it for those hard to debug crashes */
#ifndef FLUSH
#  define FLUSH 0
#endif

#ifndef LOG_FILE_NAME
#  define LOG_FILE_NAME "gdi-game.log"
#endif

#ifndef USE_LOG_STREAM
#  define USE_LOG_STREAM 0
#else
#  ifndef LOG_STREAM
#    define LOG_STREAM   stderr
#  endif
#endif

static char szAppName[] = WINDOW_CAPTION;
static char szTitle[]   = WINDOW_CAPTION " - GDI";

Bitmap *screen = NULL;

static Bitmap *cursor = NULL;
static int cursor_hsx, cursor_hsy;
static Bitmap *cursor_back = NULL;

/* Virtual-Key Codes here:
https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731(v=vs.85).aspx */
#define MAX_KEYS 256
char keys[MAX_KEYS];
static int pressed_key = 0;

int mouse_x, mouse_y;
static int mclick = 0, mdown = 0, mrelease = 0, mmove = 0;

int quit = 0;

static int show_fps = 0;
static double frameTimes[256];
static unsigned int n_elapsed = 0;

int show_debug() {
    return show_fps;
}

#if EPX_SCALE
static Bitmap *scale_epx_i(Bitmap *in, Bitmap *out);
#endif

void clear_keys() {
    int i;
    for(i = 0; i < MAX_KEYS; i++) {
        keys[i] = 0;
    }
}
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

static FILE *logfile = NULL;

void rlog(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    fputs("INFO: ", logfile);
    vfprintf(logfile, fmt, arg);
    fputc('\n', logfile);
    va_end(arg);
#if FLUSH
    fflush(logfile);
#endif
}

void rerror(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    fputs("ERROR: ", logfile);
    vfprintf(logfile, fmt, arg);
    fputc('\n', logfile);
    va_end(arg);
    fflush(logfile);
}

void exit_error(const char *msg, ...) {
    char message_text[256];
    if(msg) {
        va_list arg;
        va_start (arg, msg);
        vsnprintf (message_text, (sizeof message_text) - 1, msg, arg);
        message_text[(sizeof message_text) - 1] = '\0';
        va_end (arg);
        fputc('\n', logfile);
    } else {
        message_text[0] = '\0';
    }
    if (logfile) {
        fputs(message_text, logfile);
    }
    MessageBox(
        NULL,
        message_text,
        "Error",
        MB_ICONERROR | MB_OK
    );
    exit(1);
}

void set_cursor(Bitmap *b, int hsx, int hsy) {
    cursor_hsx = hsx;
    cursor_hsy = hsy;
    cursor = b;
    if(b) {
        int w = bm_width(b), h = bm_height(b);
        if(!cursor_back)
            cursor_back = bm_create(w, h);
        else if(bm_width(cursor_back) != w || bm_height(cursor_back) != h) {
            bm_free(cursor_back);
            cursor_back = bm_create(w, h);
        }
        ShowCursor(0);
    } else {
        bm_free(cursor_back);
        cursor_back = NULL;
        ShowCursor(1);
    }
}

/** WIN32 and GDI routines below this line *****************************************/

static int split_cmd_line(char *cmdl, char *argv[], int max);
static void FitWindow(HWND hWnd);

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HDC hdc, hdcMem;
    static HBITMAP hbmOld, hbmp;

#if EPX_SCALE
    static Bitmap *epx = NULL;
#endif

#define MAX_ARGS    16
    static int argc = 0;
    static char *argv[MAX_ARGS];
    static LPTSTR cmdl;

    switch (message) {

        case WM_CREATE: {
            unsigned char *pixels;

            BITMAPINFO bmi;
            ZeroMemory(&bmi, sizeof bmi);
            bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
            bmi.bmiHeader.biWidth = VSCREEN_WIDTH;
            bmi.bmiHeader.biHeight =  -VSCREEN_HEIGHT; // Order pixels from top to bottom
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32; // last byte not used, 32 bit for alignment
            bmi.bmiHeader.biCompression = BI_RGB;
            bmi.bmiHeader.biSizeImage = 0;
            bmi.bmiHeader.biXPelsPerMeter = 0;
            bmi.bmiHeader.biYPelsPerMeter = 0;
            bmi.bmiHeader.biClrUsed = 0;
            bmi.bmiHeader.biClrImportant = 0;
            bmi.bmiColors[0].rgbBlue = 0;
            bmi.bmiColors[0].rgbGreen = 0;
            bmi.bmiColors[0].rgbRed = 0;
            bmi.bmiColors[0].rgbReserved = 0;

            hdc = GetDC( hwnd );
            hbmp = CreateDIBSection( hdc, &bmi, DIB_RGB_COLORS, (void**)&pixels, NULL, 0 );
            if (!hbmp) {
                exit_error("CreateDIBSection");
                return 0;
            }

            hdcMem = CreateCompatibleDC( hdc );
            hbmOld = (HBITMAP)SelectObject( hdcMem, hbmp );

#if !EPX_SCALE
            screen = bm_bind(VSCREEN_WIDTH, VSCREEN_HEIGHT, pixels);
#else
            screen = bm_create(SCREEN_WIDTH, SCREEN_HEIGHT);
            epx = bm_bind(VSCREEN_WIDTH, VSCREEN_HEIGHT, pixels);
#endif
            bm_set_color(screen, bm_atoi("black"));
            bm_clear(screen);

            clear_keys();

            cmdl = _strdup(GetCommandLine());
            argc = split_cmd_line(cmdl, argv, MAX_ARGS);

            init_game(argc, argv);

            } break;

        case WM_DESTROY:
            quit = 1;
            deinit_game();

            free(cmdl);

#if !EPX_SCALE
            bm_unbind(screen);
#else
            bm_free(screen);
            bm_unbind(epx);
#endif
            SelectObject( hdcMem, hbmOld );
            DeleteDC( hdc );
            screen = NULL;
            PostQuitMessage(0);
            break;

        case WM_RBUTTONUP:
#if 0
            DestroyWindow(hwnd);
#endif
            break;

        /* If you want text input, WM_CHAR is what you're looking for */
        case WM_CHAR:
            if (wParam < 128) {
                pressed_key = wParam;
            }
            break;
        case WM_SYSKEYDOWN:
            // TIL the F10 key doesn't go through the WM_KEYDOWN:
            // https://msdn.microsoft.com/en-us/library/windows/desktop/gg153546(v=vs.85).aspx
            if (wParam == VK_F10) {
                show_fps = !show_fps;
            } else return DefWindowProc(hwnd, message, wParam, lParam);
            break;
        case WM_KEYDOWN:
            if (wParam == VK_F12) {

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

            } else if (ESCAPE_QUITS && VK_ESCAPE == wParam) {
                DestroyWindow(hwnd);
            } else if (wParam < MAX_KEYS) {
                keys[wParam] = 1;
                pressed_key = wParam | 0xFFFF0000;
            }
            break;
        case WM_KEYUP:
            if (wParam < MAX_KEYS) {
                keys[wParam] = 0;
            }
            break;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK: /* ...all clicks treated equally */
        {
            mouse_x = LOWORD(lParam) * SCREEN_WIDTH / WINDOW_WIDTH;
            mouse_y = HIWORD(lParam) * SCREEN_HEIGHT / WINDOW_HEIGHT;
            mdown = 1;
            mrelease = 0;
            mclick = 1;
        } break;

        case WM_LBUTTONUP:
        {
            mdown = 0;
            mrelease = 1;
        } break;
        case WM_MOUSEMOVE:
        {
            mouse_x = LOWORD(lParam) * SCREEN_WIDTH / WINDOW_WIDTH;
            mouse_y = HIWORD(lParam) * SCREEN_HEIGHT / WINDOW_HEIGHT;
            mmove = 1;
        } break;
        case WM_PAINT:
        {
            if(!screen) break;
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint( hwnd, &ps );

            int cx = 0, cy = 0;
            if(cursor) {
                cx = mouse_x - cursor_hsx;
                cy = mouse_y - cursor_hsy;
                int cw = bm_width(cursor);
                int ch = bm_height(cursor);

                bm_blit(cursor_back, 0, 0, screen, cx, cy, cw, ch);
                bm_maskedblit(screen, cx, cy, cursor, 0, 0, cw, ch);
            }

#if EPX_SCALE
            scale_epx_i(screen, epx);
#endif

#if WINDOW_WIDTH == VSCREEN_WIDTH && WINDOW_HEIGHT == VSCREEN_HEIGHT
            BitBlt( hdc, 0, 0, VSCREEN_WIDTH, VSCREEN_HEIGHT, hdcMem, 0, 0, SRCCOPY );
#else
            StretchBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdcMem, 0, 0, VSCREEN_WIDTH, VSCREEN_HEIGHT, SRCCOPY);
#endif
            EndPaint( hwnd, &ps );

            if(cursor) {
                bm_maskedblit(screen, cx, cy, cursor_back, 0, 0, bm_width(cursor_back), bm_height(cursor_back));
            }

            break;
        }
        /* Don't erase the background - it causes flickering
            http://stackoverflow.com/a/14153470/115589 */
        case WM_ERASEBKGND:
            return 1;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

int APIENTRY WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow
        )
{
    MSG msg;
    WNDCLASS wc;
    HWND hwnd;
    double elapsedSeconds = 0.0;

#ifdef _MSC_VER
    errno_t err = fopen_s(&logfile, LOG_FILE_NAME, "w");
    if (err != 0) {
        exit_error("Unable to open log file `%s`");
    }
#elif defined USE_LOG_STREAM
	logfile = LOG_STREAM;
#else
    logfile = fopen(LOG_FILE_NAME, "w");
    if(!logfile) {
        exit_error("Unable to open log file `%s`");
    }
#endif

    rlog("%s","GDI Framework: Application Running");

    ZeroMemory(&wc, sizeof wc);
    wc.hInstance     = hInstance;
    wc.lpszClassName = szAppName;
    wc.lpfnWndProc   = (WNDPROC)WndProc;
    wc.style         = CS_DBLCLKS|CS_VREDRAW|CS_HREDRAW;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

    if (FALSE == RegisterClass(&wc))
        return 0;

    hwnd = CreateWindow(
        szAppName,
        szTitle,
        //WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        //WS_POPUP, // For no border/titlebar etc
        WS_CAPTION,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        0,
        0,
        hInstance,
        0);

    if (NULL == hwnd)
        return 0;

    FitWindow(hwnd);

    ShowWindow(hwnd , SW_SHOW);

    /* Todo: I didn't bother with higher resolution timers:
    https://msdn.microsoft.com/en-us/library/dn553408(v=vs.85).aspx */

    rlog("%s","GDI Framework: Entering main loop");
    quit = 0;
    for(;;) {
        clock_t startTime, endTime;
        startTime = clock();
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if(quit) break;

        Sleep(1);
        if(elapsedSeconds > 1.0/FPS) {

            if(!render(elapsedSeconds)) {
                DestroyWindow(hwnd);
            } else {
                if(show_fps && n_elapsed > 0) {
                    double sum = 0;
                    int i, n = n_elapsed > 0xFF ? 0xFF : n_elapsed;
                    for(i = 0; i < n; i++) sum += frameTimes[i];
                    double avg = sum / n;
                    double fps = 1.0 / avg;
                    BmFont * f = bm_get_font(screen);
                    bm_reset_font(screen);
                    bm_set_color(screen, bm_atoi("red"));
                    bm_fillrect(screen, 0, 0, 50, 10);
                    bm_set_color(screen, bm_atoi("yellow"));
                    bm_printf(screen, 1, 1, "%3.2f", fps);
                    bm_set_font(screen, f);
                }
                frameTimes[(n_elapsed++) & 0xFF] = elapsedSeconds;
            }

            InvalidateRect(hwnd, 0, TRUE);
            elapsedSeconds = 0.0;
            pressed_key = 0;
            mrelease = 0;
            mmove = 0;
            mclick = 0;
        }
        endTime = clock();
        elapsedSeconds += (double)(endTime - startTime) / CLOCKS_PER_SEC;
    }
    rlog("%s","GDI Framework: Main loop stopped");
    rlog("%s","Application Done!\n");

#if !USE_LOG_STREAM
    fclose(logfile);
#endif

    return msg.wParam;
}

/* Make sure the client area fits; center the window in the process */
static void FitWindow(HWND hwnd) {
    RECT rcClient, rwClient;
    POINT ptDiff;
    HWND hwndParent;
    RECT rcParent, rwParent;
    POINT ptPos;

    GetClientRect(hwnd, &rcClient);
    GetWindowRect(hwnd, &rwClient);
    ptDiff.x = (rwClient.right - rwClient.left) - rcClient.right;
    ptDiff.y = (rwClient.bottom - rwClient.top) - rcClient.bottom;

    hwndParent = GetParent(hwnd);
    if (NULL == hwndParent)
        hwndParent = GetDesktopWindow();

    GetWindowRect(hwndParent, &rwParent);
    GetClientRect(hwndParent, &rcParent);

    ptPos.x = rwParent.left + (rcParent.right - WINDOW_WIDTH) / 2;
    ptPos.y = rwParent.top + (rcParent.bottom - WINDOW_HEIGHT) / 2;

    MoveWindow(hwnd, ptPos.x, ptPos.y, WINDOW_WIDTH + ptDiff.x, WINDOW_HEIGHT + ptDiff.y, 0);
}

/*
Alternative to CommandLineToArgvW().
I used a compiler where shellapi.h was not available,
so this function breaks it down according to the last set of rules in
http://i1.blogs.msdn.com/b/oldnewthing/archive/2010/09/17/10063629.aspx

Only a long time after I wrote this did I discover that you can actually
use __argc, __argv to access the commandline parameters...

extern "C" int __stdcall WinMain( struct HINSTANCE__*, struct HINSTANCE__*, char*, int ) {
    return main( __argc, __argv );
}

http://www.testdeveloper.com/2010/03/16/a-few-ways-to-access-argc-and-argv-in-c/

*/
static int split_cmd_line(char *cmdl, char *argv[], int max) {

    int argc = 0;
    char *p = cmdl, *q = p, *arg = p;
    int state = 1;
    while(state) {
        switch(state) {
            case 1:
                if(argc == max) return argc;
                if(!*p) {
                    state = 0;
                } else if(isspace(*p)) {
                    *q++ = *p++;
                } else if(*p == '\"') {
                    state = 2;
                    *q++ = *p++;
                    arg = q;
                } else {
                    state = 3;
                    arg = q;
                    *q++ = *p++;
                }
            break;
            case 2:
                if(!*p) {
                    argv[argc++] = arg;
                    *q++ = '\0';
                    state = 0;
                } else if(*p == '\"') {
                    if(p[1] == '\"') {
                        state = 2;
                        *q++ = *p;
                        p+=2;
                    } else {
                        state = 1;
                        argv[argc++] = arg;
                        *q++ = '\0';
                        p++;
                    }
                } else {
                    *q++ = *p++;
                }
            break;
            case 3:
                if(!*p) {
                    state = 0;
                    argv[argc++] = arg;
                    *q++ = '\0';
                } else if(isspace(*p)) {
                    state = 1;
                    argv[argc++] = arg;
                    *q++ = '\0';
                    p++;
                } else {
                    *q++ = *p++;
                }
            break;
        }
    }
    return argc;
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

char *readfile(const char *fname) {
    FILE *f;
    size_t len, r;
    char *bytes;

#ifdef _MSC_VER
    errno_t err = fopen_s(&f, fname, "rb");
    if(err != 0)
        return NULL;
#else
    f = fopen(fname, "rb");
    if (!f)
        return NULL;
#endif

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    rewind(f);

    if(!(bytes = malloc(len+2)))
        return NULL;
    r = fread(bytes, 1, len, f);

    if(r != len) {
        free(bytes);
        return NULL;
    }

    fclose(f);
    bytes[len] = '\0';

    return bytes;
}

Bitmap *get_bmp(const char *filename) {
    return bm_load(filename);
}
