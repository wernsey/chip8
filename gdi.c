/*
>tcc gdi.c bmp.c
Built from the HELLO_WIN.C example in tcc
References:
https://www.daniweb.com/software-development/cpp/code/241875/fast-animation-with-the-windows-gdi
https://www-user.tu-chemnitz.de/~heha/petzold/ch14e.htm
https://www-user.tu-chemnitz.de/~heha/petzold/ch15d.htm
http://forums.codeguru.com/showthread.php?487633-32-bit-DIB-from-24-bit-bitmap
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <windows.h>

#include "bmp.h"
#include "gdi.h"

char szAppName[] = APPNAME;
char szTitle[]   = APPNAME;

#define IDT_TIMER1  1001

/* Virtual-Key Codes here:
https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731(v=vs.85).aspx */
#define MAX_KEYS 256
char keys[MAX_KEYS];

Bitmap *screen = NULL;

void clear_keys() {
    int i;
    for(i = 0; i < MAX_KEYS; i++) {
        keys[i] = 0;
    }
}

void exit_error(const char *msg, ...) {
    char message_text[256];
    if(msg) {
        va_list arg;
        va_start (arg, msg);
        vsnprintf (message_text, (sizeof message_text) - 1, msg, arg);
        va_end (arg);
    }
    MessageBox(
        NULL,
        message_text,
        "Error",
        MB_ICONERROR | MB_OK
    );
    exit(1);
}

/** WIN32 and GDI routines below this line *****************************************/

static void CenterWindow(HWND hWnd);

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HDC hdc, hdcMem;
    static HBITMAP hbmOld, hbmp;
    static Bitmap *bmp;

    /* Todo: I didn't bother with higher resolution timers:
    https://msdn.microsoft.com/en-us/library/dn553408(v=vs.85).aspx */
    static clock_t startTime;

#define MAX_ARGS    16
    static int argc = 0;
    static char *argv[MAX_ARGS];
    static LPTSTR cmdl;

    switch (message) {

        case WM_CREATE: {
            unsigned char *pixels;

            CenterWindow(hwnd);

            BITMAPINFO bmi;
            ZeroMemory(&bmi, sizeof bmi);
            bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
            bmi.bmiHeader.biWidth = SCREEN_WIDTH;
            bmi.bmiHeader.biHeight =  -SCREEN_HEIGHT; // Order pixels from top to bottom
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

            hdcMem = CreateCompatibleDC( hdc );
            hbmOld = (HBITMAP)SelectObject( hdcMem, hbmp );

            bmp = bm_bind(SCREEN_WIDTH, SCREEN_HEIGHT, pixels);
            bm_set_color_s(bmp, "black");
            bm_clear(bmp);

            screen = bmp;

            clear_keys();

            /* This is very dirty. Should use CommandLineToArgvW(), but the
            header includes and the LPCWSTR proved problematic.
            The below is sufficient for what I want to do, but won't deal with
            quoted parameters or non-ascii etc.

            For an idea of how complex it can get, see:
            http://i1.blogs.msdn.com/b/oldnewthing/archive/2010/09/17/10063629.aspx
            http://www.windowsinspired.com/how-a-windows-programs-splits-its-command-line-into-individual-arguments/
            */
            cmdl = strdup(GetCommandLine());
            char *arg = strtok(cmdl, " \t");
            while(arg && argc < MAX_ARGS) {
                argv[argc++] = arg;
                arg = strtok(NULL, " ");
            }

            init_game(argc, argv);

            SetTimer(hwnd, IDT_TIMER1, 1000/FPS, (TIMERPROC) NULL);
            startTime = clock();

            } break;

        case WM_DESTROY:
            deinit_game();

            free(cmdl);

            KillTimer(hwnd, IDT_TIMER1);
            SelectObject( hdcMem, hbmOld );
            DeleteDC( hdc );
            bm_unbind(bmp);
            PostQuitMessage(0);
            break;

        case WM_TIMER:
            if(wParam == IDT_TIMER1) {
                clock_t endTime = clock();
                double elapsedSeconds = (double)(endTime - startTime) / CLOCKS_PER_SEC;
                if(!render(elapsedSeconds)) {
                    DestroyWindow(hwnd);
                }
                InvalidateRect(hwnd, 0, TRUE);
            }
            break;

        case WM_RBUTTONUP:
            DestroyWindow(hwnd);
            break;

        case WM_KEYDOWN:
            if (wParam < MAX_KEYS) {
                keys[wParam] = 1;
            }
            if (VK_ESCAPE == wParam)
                DestroyWindow(hwnd);
            break;

        case WM_KEYUP:
            if (wParam < MAX_KEYS) {
                keys[wParam] = 0;
            }
            break;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint( hwnd, &ps );
#if WINDOW_WIDTH == SCREEN_WIDTH && WINDOW_HEIGHT == SCREEN_HEIGHT
            BitBlt( hdc, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hdcMem, 0, 0, SRCCOPY );
#else
            StretchBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdcMem, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SRCCOPY);
#endif
            EndPaint( hwnd, &ps );
            startTime = clock();
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
        (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX) | WS_VISIBLE,
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

    /* Make sure the client area fits */
    RECT rcClient, rcWindow;
    POINT ptDiff;
    GetClientRect( hwnd, &rcClient );
    GetWindowRect( hwnd, &rcWindow );
    ptDiff.x = (rcWindow.right - rcWindow.left) - rcClient.right;
    ptDiff.y = (rcWindow.bottom - rcWindow.top) - rcClient.bottom;
    MoveWindow( hwnd, rcWindow.left, rcWindow.top, WINDOW_WIDTH + ptDiff.x, WINDOW_HEIGHT + ptDiff.y, 0);
    UpdateWindow( hwnd );

    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}

static void CenterWindow(HWND hwnd_self)
{
    HWND hwnd_parent;
    RECT rw_self, rc_parent, rw_parent;
    int xpos, ypos;

    hwnd_parent = GetParent(hwnd_self);
    if (NULL == hwnd_parent)
        hwnd_parent = GetDesktopWindow();

    GetWindowRect(hwnd_parent, &rw_parent);
    GetClientRect(hwnd_parent, &rc_parent);
    GetWindowRect(hwnd_self, &rw_self);

    xpos = rw_parent.left + (rc_parent.right + rw_self.left - rw_self.right) / 2;
    ypos = rw_parent.top + (rc_parent.bottom + rw_self.top - rw_self.bottom) / 2;

    SetWindowPos(
        hwnd_self, NULL,
        xpos, ypos, 0, 0,
        SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE
        );
}


char *read_text_file(const char *fname) {
    FILE *f;
    long len,r;
    char *str;

    if(!(f = fopen(fname, "rb")))
        return NULL;

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    rewind(f);

    if(!(str = malloc(len+2)))
        return NULL;
    r = fread(str, 1, len, f);

    if(r != len) {
        free(str);
        return NULL;
    }

    fclose(f);
    str[len] = '\0';
    return str;
}
