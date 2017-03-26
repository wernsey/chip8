/**
 * Bitmap API (bmp.h/bmp.c)
 * ========================
 * ![toc-]
 *
 * Low-level routines to manipulate bitmap graphic files.
 *
 * * It supports BMP, GIF, PCX and TGA files without any third party dependencies.
 * * PNG support is optional through [libpng][]. Use `-DUSEPNG` when compiling.
 * * JPG support is optional through [libjpeg][]. Use `-DUSEJPG` when compiling.
 *
 * [libpng]: http://www.libpng.org/pub/png/libpng.html
 * [libjpeg]: http://www.ijg.org/
 *
 * References
 * ----------
 *
 * * [BMP file format](http://en.wikipedia.org/wiki/BMP_file_format)
 * * [Bresenham's line algorithm](http://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm)
 * * <http://members.chello.at/~easyfilter/bresenham.html>
 * * [Flood fill](http://en.wikipedia.org/wiki/Flood_fill)
 * * <http://en.wikipedia.org/wiki/Midpoint_circle_algorithm>
 * * <http://web.archive.org/web/20110706093850/http://free.pages.at/easyfilter/bresenham.html>
 * * <http://damieng.com/blog/2011/02/20/typography-in-8-bits-system-fonts>
 * * <http://www.w3.org/Graphics/GIF/spec-gif89a.txt>
 * * Nelson, M.R. : "LZW Data Compression", Dr. Dobb's Journal, October 1989.
 * * <http://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art011>
 * * <http://www.matthewflickinger.com/lab/whatsinagif/bits_and_bytes.asp>
 * * <http://web.archive.org/web/20100206055706/http://www.qzx.com/pc-gpe/pcx.txt>
 * * <http://www.shikadi.net/moddingwiki/PCX_Format>
 * * <https://en.wikipedia.org/wiki/Truevision_TGA>
 * * <http://paulbourke.net/dataformats/tga/>
 * * <http://www.ludorg.net/amnesia/TGA_File_Format_Spec.html>
 * * [X PixMap](https://en.wikipedia.org/wiki/X_PixMap)
 * * <http://www.fileformat.info/format/xpm/egff.htm>
 *
 * License
 * -------
 *
 *    Author: Werner Stoop
 *    This is free and unencumbered software released into the public domain.
 *    http://unlicense.org/
 *
 * API
 * ---
 */

#ifndef BMP_H
#define BMP_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/**
 * ### Structures
 * `typedef struct bitmap Bitmap`  \
 * Structure containing a bitmap image.
 *
 * The internal format is 0xAARRGGBB little endian.
 * Meaning that p[0] contains B, p[1] contains G,
 * p[2] contains R and p[3] contains A
 * and the data buffer is an array of bytes BGRABGRABGRABGRABGRA...
 */
typedef struct bitmap {
    /* Dimesions of the bitmap */
    int w, h;

    /* The actual pixel data in RGBA format */
    unsigned char *data;

    /* Color for the pen of the canvas */
    unsigned int color;

    /* Font object for rendering text */
    struct bitmap_font *font;

    /* Clipping Rectangle
     * (x0,y0) is inclusive.
     * (x1,y1) is exclusive.
     */
    struct {
        int x0, y0;
        int x1, y1;
    } clip;
} Bitmap;

/**
 * ### Creating and Destroying bitmaps
 */

/**`Bitmap *bm_create(int w, int h)`  \
 * Creates a bitmap of the specified dimensions
 */
Bitmap *bm_create(int w, int h);

/** `void bm_free(Bitmap *b)`  \
 * Destroys a bitmap previously created with `bm_create()`
 */
void bm_free(Bitmap *b);

/** `Bitmap *bm_copy(Bitmap *b)`  \
 * Creates a duplicate of the bitmap structure `b`.
 */
Bitmap *bm_copy(Bitmap *b);

/** `Bitmap *bm_from_Xbm(int w, int h, unsigned char *data)`  \
 * Creates a `Bitmap` object from [XBM data](https://en.wikipedia.org/wiki/X_BitMap).  \
 * The XBM image is imported into a program through a `#include "include.xbm"` directive.  \
 * The width `w` and height `h` are the `_width` and `_height` variables at the top of the XBM file.
 * The `data` parameter is the `_bits` variable in the XBM file.
 */
Bitmap *bm_from_Xbm(int w, int h, unsigned char *data);

/** `Bitmap *bm_from_Xpm(char *xpm[])`  \
 * Creates a `Bitmap` object from [X PixMap](https://en.wikipedia.org/wiki/X_PixMap)
 * data in a source file.
 */
Bitmap *bm_from_Xpm(char *xpm[]);

/** `int bm_width(Bitmap *b)`  \
 * Retrieves the width of the bitmap `b`
 */
int bm_width(Bitmap *b);

/** `int bm_height(Bitmap *b)`  \
 * Retrieves the height of the bitmap `b`
 */
int bm_height(Bitmap *b);

/**
 * ### File I/O Functions
 * These functions are for reading graphics files into a `Bitmap` structure,
 * or for writing a `Bitmap` structure to a file.
 */

/** `Bitmap *bm_load(const char *filename)`  \
 * Loads a bitmap file `filename` into a bitmap structure.
 *
 * It tries to detect the file type from the first bytes in the file.
 *
 * BMP, GIF, PCX and TGA support is always enabled, while JPG and PNG support
 * depends on how the library was compiled.
 *
 * Returns `NULL` if the file could not be loaded.
 */
Bitmap *bm_load(const char *filename);

#ifdef EOF /* <stdio.h> included? http://stackoverflow.com/q/29117606/115589 */
/** `Bitmap *bm_load_fp(FILE *f)`  \
 * Loads a bitmap from a `FILE*` that's already open.
 *
 * BMP, GIF and PCX support is always enabled, while JPG and PNG support
 * depends on how the library was compiled.
 *
 * Returns `NULL` if the file could not be loaded.
 */
Bitmap *bm_load_fp(FILE *f);
#endif

#if defined(USESDL) && defined(_SDL_H)
/** `Bitmap *bm_load_rw(SDL_RWops *file)`  \
 * Loads a bitmap from a SDL `SDL_RWops*` structure,
 * for use with the [SDL library] (http://www.libsdl.org).
 *
 * This function is only available if the USESDL preprocessor macro
 * is defined, and `SDL.h` is included before `bmp.h`.
 *
 * BMP, GIF and PCX support is always enabled, while JPG and PNG support
 * depends on how the library was compiled.
 *
 * Returns `NULL` if the file could not be loaded.
 */
Bitmap *bm_load_rw(SDL_RWops *file);
#endif

/** `int bm_save(Bitmap *b, const char *fname)`  \
 * Saves the bitmap `b` to a bitmap file named `fname`.
 *
 * If the filename contains `".png"`, `".gif"`, `".pcx"`, `".tga"` or `".jpg"` the file is
 * saved as a PNG, GIF, PCX or JPG, respectively, otherwise the BMP format is the default.
 * It can only save to JPG or PNG if JPG or PNG support is enabled
 * at compile time, otherwise it saves to a BMP file.
 *
 * Returns 1 on success, 0 on failure.
 */
int bm_save(Bitmap *b, const char *fname);

/**
 * ### Binding Functions
 * These functions are used to bind a `Bitmap` structure to
 * an existing memory buffer such as an OpenGL texture, an
 * SDL Surface or a Win32 GDI context.
 */

/** `Bitmap *bm_bind(int w, int h, unsigned char *data)`  \
 * Creates a bitmap structure bound to an existing array
 * of pixel data (for example, an OpenGL texture or a SDL surface). The
 * `data` must be an array of `w` \* `h` \* 4 bytes of ARGB pixel data.
 * The returned `Bitmap*` must be destroyed with `bm_unbind()`
 * rather than `bm_free()`.
 */
Bitmap *bm_bind(int w, int h, unsigned char *data);

/** `void bm_rebind(Bitmap *b, unsigned char *data)`  \
 * Changes the data referred to by a bitmap structure previously
 * created with a call to `bm_bind()`.
 * The new data must be of the same dimensions as specified
 * in the original `bm_bind()` call.
 */
void bm_rebind(Bitmap *b, unsigned char *data);

/** `void bm_unbind(Bitmap *b)`  \
 * Deallocates the memory of a bitmap structure previously created
 * through `bm_bind()`.
 */
void bm_unbind(Bitmap *b);

/**
 * ### Clipping and Buffer Manipulation Functions
 */

/** `void bm_clip(Bitmap *b, int x0, int y0, int x1, int y1)`  \
 * Sets the clipping rectangle on the bitmap from `x0,y0` (inclusive)
 * to `x1,y1` exclusive.
 */
void bm_clip(Bitmap *b, int x0, int y0, int x1, int y1);

/** `void bm_unclip(Bitmap *b)`  \
 * Resets the bitmap `b`'s clipping rectangle.
 */
void bm_unclip(Bitmap *b);

/** `void bm_flip_vertical(Bitmap *b)`  \
 * Flips the bitmap vertically.
 */
void bm_flip_vertical(Bitmap *b);

/**
 * ### Pixel Functions
 */

/** `unsigned int bm_get(Bitmap *b, int x, int y)`  \
 * Retrieves the value of the pixel at `x,y` as an integer.
 *
 * The return value is in the form `0xAABBGGRR`
 */
unsigned int bm_get(Bitmap *b, int x, int y);

/** `void bm_set(Bitmap *b, int x, int y, unsigned int c)`  \
 * Sets the value of the pixel at `x,y` to the color `c`.
 *
 * `c` is in the form `0xAABBGGRR`
 */
void bm_set(Bitmap *b, int x, int y, unsigned int c);

/**
 * ### Color functions
 * Functions for manipulating colors in the image.
 */

/** `void bm_set_color(Bitmap *bm, unsigned int col)`  \
 * Sets the color of the pen to a color represented
 * by an integer, like `0xAARRGGBB`
 */
void bm_set_color(Bitmap *bm, unsigned int col);

/** `unsigned int bm_get_color(Bitmap *bc)`  \
 * Retrieves the pen color.
 */
unsigned int bm_get_color(Bitmap *bm);

/** `void bm_set_alpha(Bitmap *bm, int a)`  \
 * Sets the alpha value of the pen to `a`
 */
void bm_set_alpha(Bitmap *bm, int a);

/** `unsigned int bm_picker(Bitmap *bm, int x, int y)`  \
 * Sets the color of the pen to the color of the pixel at <x,y>
 * on the bitmap.
 *
 * The pen color can then be retrieved through `bm_get_color()`.
 *
 * It returns the integer representation of the color.
 */
unsigned int bm_picker(Bitmap *bm, int x, int y);

/** `unsigned int bm_atoi(const char *text)`  \
 * Converts a text string like "#FF00FF" or "white" to
 * an integer of the form `0xFF00FF`.
 * The `text` parameter is not case sensitive and spaces are
 * ignored, so for example "darkred" and "Dark Red" are equivalent.
 *
 * The shorthand "#RGB" format is also supported
 * (eg. "#0fb", which is the same as "#00FFBB").
 *
 * Additionally, it also supports the CSS syntax for "RGB(r,g,b)",
 * "RGBA(r,g,b,a)", "HSL(h,s,l)" and "HSLA(h,s,l,a)".
 *
 * The list of supported colors are based on the wikipedia's
 * list of HTML and X11 [Web colors](http://en.wikipedia.org/wiki/Web_colors).
 *
 * It returns 0 (black) if the string couldn't be parsed.
 */
unsigned int bm_atoi(const char *text);

/** `unsigned int bm_rgb(unsigned char R, unsigned char G, unsigned char B)`  \
 * Builds a color from the specified `(R,G,B)` values
 */
unsigned int bm_rgb(unsigned char R, unsigned char G, unsigned char B);

/** `unsigned int bm_rgba(unsigned char R, unsigned char G, unsigned char B, unsigned char A)`  \
 * Builds a color from the specified `(R,G,B,A)` values
 */
unsigned int bm_rgba(unsigned char R, unsigned char G, unsigned char B, unsigned char A);

/** `void bm_get_rgb(unsigned int col, unsigned char *R, unsigned char *G, unsigned char *B)`  \
 * Decomposes a color `col` into its `R,G,B` components.
 */
void bm_get_rgb(unsigned int col, unsigned char *R, unsigned char *G, unsigned char *B);

/** `unsigned int bm_hsl(double H, double S, double L)`  \
 * Creates a color from the given Hue/Saturation/Lightness values.
 * See <https://en.wikipedia.org/wiki/HSL_and_HSV> for more information.
 *
 * Hue (`H`) is given as an angle in degrees from 0&deg; to 360&deg;.
 * Saturation (`S`) and Lightness (`L`) are given as percentages from 0 to 100%.
 */
unsigned int bm_hsl(double H, double S, double L);

/** `unsigned int bm_hsla(double H, double S, double L, double A)`  \
 * Creates a color from the given Hue/Saturation/Lightness and alpha values.
 *
 * Hue (`H`) is given as an angle in degrees from 0&deg; to 360&deg;.
 * Saturation (`S`) and Lightness (`L`) and Alpha (`A`) are given as percentages from 0 to 100%.
 */
unsigned int bm_hsla(double H, double S, double L, double A);

/** `bm_get_hsl(unsigned int col, double *H, double *S, double *L)`  \
 * Decomposes a color `col` into its Hue/Saturation/Lightness components.
 *
 * Hue (`H`) is given as an angle in degrees from 0&deg; to 360&deg;.
 * Saturation (`S`) and Lightness (`L`) are given as percentages from 0 to 100%.
 */
void bm_get_hsl(unsigned int col, double *H, double *S, double *L);

/** `unsigned int bm_byte_order(unsigned int col)`  \
 * Fixes the input color to be in the proper byte order.
 *
 * The input color should be in the format `0xAARRGGBB`. The output
 * will be in either `0xAARRGGBB` or `0xAABBGGRR` depending on how the
 * library was compiled.
 */
unsigned int bm_byte_order(unsigned int col);

/** `unsigned int bm_lerp(unsigned int color1, unsigned int color2, double t)`  \
 * Computes the color that is a distance `t` along the line between
 * `color1` and `color2`.
 *
 * If `t` is 0 it returns `color1`. If `t` is 1.0 it returns `color2`.
 */
unsigned int bm_lerp(unsigned int color1, unsigned int color2, double t);

/**
 * ### Blitting Functions
 */

/** `void bm_blit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h)`  \
 * Blits an area of w*h pixels at sx,sy on the src bitmap to
 * dx,dy on the `dst` bitmap.
 */
void bm_blit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h);

/** `void bm_maskedblit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h)`  \
 * Blits an area of `w` \* `h` pixels at `sx,sy` on the `src` bitmap to
 * `dx,dy` on the `dst` bitmap.
 *
 * Pixels on the `src` bitmap that matches the `src` bitmap color are not blitted.
 * The alpha value of the pixels on the `src` bitmap is not taken into account.
 */
void bm_maskedblit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h);

/** `void bm_blit_ex(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, int mask)`  \
 * Extended blit function. Blits an area of `sw` \* `sh` pixels at `sx,sy` from the `src` bitmap to
 * `dx,dy` on the `dst` bitmap into an area of `dw` \* `dh` pixels, stretching or shrinking the blitted area as neccessary.
 *
 * If `mask` is non-zero, pixels on the `src` bitmap that matches the `src` bitmap color are not blitted.
 * Whether the alpha value of the pixels is taken into account depends on whether IGNORE_ALPHA is enabled.
 */
void bm_blit_ex(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, int mask);

/** `typedef int (*bm_blit_fun)(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, void *data);`  \
 * Prototype for the callback function to be passed to `bm_blit_ex_fun()`.
 *
 * `dst` is the destination bitmap and `dx,dy` is where the pixel is too be plotted.
 *
 * `src` is the source bitmap and `sx,sy` is where the color of the pixel is to be obtained from.
 *
 * `mask` is the mask color of the source bitmap. The function can decide whether or not to blit the pixel based on this.
 *
 * It should return 1 on success. If it returns 0 bm_blit_ex_fun will terminate immediately.
 */
typedef int (*bm_blit_fun)(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int mask, void *data);

/** `void bm_blit_ex_fun(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, bm_blit_fun fun, void *data);`  \
 * Blits a scaled bitmap from `src` to `dst`, but calls a callback function `fun`
 * for each pixel to handle the plotting instead of plotting the pixel directly.
 *
 * This can be used for some specific post-processing after the position of a pixel is determined
 * on the source and destination, but before the pixel is actually plotted.
 *
 * `data` is passed straight through to the callback function.
 *
 * The other parameters are the same as for `bm_blit_ex()`.
 */
void bm_blit_ex_fun(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, bm_blit_fun fun, void *data);

/**
 * ### Filter Functions
 */

/** `void bm_smooth(Bitmap *b)`  \
 * Smoothes the bitmap `b` by applying a 5x5 Gaussian filter.
 */
void bm_smooth(Bitmap *b);

/** `void bm_apply_kernel(Bitmap *b, int dim, float kernel[])`  \
 * Applies a `dim` * `dim` kernel to the image.
 */
void bm_apply_kernel(Bitmap *b, int dim, float kernel[]);

/** `Bitmap *bm_resample(const Bitmap *in, int nw, int nh)`  \
 * Creates a new bitmap of dimensions `nw` * `nh` that is a scaled
 * using the Nearest Neighbour method the input bitmap.
 *
 * The input bimap remains untouched.
 */
Bitmap *bm_resample(const Bitmap *in, int nw, int nh);

/** `Bitmap *bm_resample_blin(const Bitmap *in, int nw, int nh)`  \
 * Creates a new bitmap of dimensions `nw` * `nh` that is a scaled
 * using Bilinear Interpolation from the input bitmap.
 *
 * The input bimap remains untouched.
 *
 * _Bilinear Interpolation is better suited for making an image larger._
 */
Bitmap *bm_resample_blin(const Bitmap *in, int nw, int nh);

/** `Bitmap *bm_resample_bcub(const Bitmap *in, int nw, int nh)`  \
 * Creates a new bitmap of dimensions `nw` * `nh` that is a scaled
 * using Bicubic Interpolation from the input bitmap.
 *
 * The input bimap remains untouched.
 *
 * _Bicubic Interpolation is better suited for making an image smaller._
 */
Bitmap *bm_resample_bcub(const Bitmap *in, int nw, int nh);

/** `void bm_swap_color(Bitmap *b, unsigned int src, unsigned int dest)`  \
 * Replaces all pixels of color `src` in bitmap `b` with the color `dest`.
 */
void bm_swap_color(Bitmap *b, unsigned int src, unsigned int dest);

#ifdef NULL /* <stdlib.h> included? - required for size_t */
/** `void bm_reduce_palette(Bitmap *b, unsigned int palette[], size_t n)`  \
 * Reduces the colors in the bitmap `b` to the colors in `palette`
 * by applying Floyd-Steinberg dithering.
 *
 * `palette` is an array of integers containing the new palette and
 * `n` is the number of entries in the palette.
 */
void bm_reduce_palette(Bitmap *b, unsigned int palette[], size_t n);

/** `void bm_reduce_palette_OD4(Bitmap *b, unsigned int palette[], size_t n)`  \
 * Reduces the colors in the bitmap `b` to the colors in `palette`
 * by applying [ordered dithering](https://en.wikipedia.org/wiki/Ordered_dithering)
 * and a 4x4 Bayer matrix.
 *
 * `palette` is an array of integers containing the new palette and
 * `n` is the number of entries in the palette.
 */
void bm_reduce_palette_OD4(Bitmap *b, unsigned int palette[], size_t n);

/** `void bm_reduce_palette_OD8(Bitmap *b, unsigned int palette[], size_t n)`  \
 * Reduces the colors in the bitmap `b` to the colors in `palette`
 * by applying [ordered dithering](https://en.wikipedia.org/wiki/Ordered_dithering)
 * and a 8x8 Bayer matrix.
 *
 * `palette` is an array of integers containing the new palette and
 * `n` is the number of entries in the palette.
 */
void bm_reduce_palette_OD8(Bitmap *b, unsigned int palette[], size_t n);
#endif

/**
 * ### Drawing Primitives
 * `bmp.h` provides these methods for drawing graphics primitives.
 */

/** `void bm_clear(Bitmap *b)`  \
 * Clears the bitmap to the color of the pen.
 */
void bm_clear(Bitmap *b);

/** `void bm_putpixel(Bitmap *b, int x, int y)`  \
 * Draws a single pixel at <x,y> using the pen color.
 */
void bm_putpixel(Bitmap *b, int x, int y);

/** `void bm_line(Bitmap *b, int x0, int y0, int x1, int y1)`  \
 * Draws a line from <x0,y0> to <x1,y1> using the color of the pen.
 */
void bm_line(Bitmap *b, int x0, int y0, int x1, int y1);

/** `void bm_rect(Bitmap *b, int x0, int y0, int x1, int y1)`  \
 * Draws a rectangle from <x0,y0> to <x1,y1> using the pen color
 */
void bm_rect(Bitmap *b, int x0, int y0, int x1, int y1);

/** `void bm_fillrect(Bitmap *b, int x0, int y0, int x1, int y1)`  \
 * Draws a filled rectangle from <x0,y0> to <x1,y1> using the pen color
 */
void bm_fillrect(Bitmap *b, int x0, int y0, int x1, int y1);

/** `void bm_dithrect(Bitmap *b, int x0, int y0, int x1, int y1)`  \
 * Draws a dithered rectangle from <x0,y0> to <x1,y1> using the pen color
 */
void bm_dithrect(Bitmap *b, int x0, int y0, int x1, int y1);

/** `void bm_circle(Bitmap *b, int x0, int y0, int r)`  \
 * Draws a circle of radius `r` centered at <x,y> using the pen color
 */
void bm_circle(Bitmap *b, int x0, int y0, int r);

/** `void bm_fillcircle(Bitmap *b, int x0, int y0, int r)`  \
 * Draws a filled circle of radius `r` centered at <x,y> using the pen color
 */
void bm_fillcircle(Bitmap *b, int x0, int y0, int r);

/** `void bm_ellipse(Bitmap *b, int x0, int y0, int x1, int y1)`  \
 * Draws an ellipse that occupies the rectangle from <x0,y0> to
 * <x1,y1>, using the pen color
 */
void bm_ellipse(Bitmap *b, int x0, int y0, int x1, int y1);

/* MISSING: bm_fillellipse */

/** `void bm_round_rect(Bitmap *b, int x0, int y0, int x1, int y1, int r)`  \
 * Draws a rect from <x0,y0> to <x1,y1> using the pen color with rounded corners
 * of radius `r`
 */
void bm_roundrect(Bitmap *b, int x0, int y0, int x1, int y1, int r);

/** `void bm_fill_round_rect(Bitmap *b, int x0, int y0, int x1, int y1, int r)`  \
 * Draws a filled rect from <x0,y0> to <x1,y1> using the pen color with rounded corners
 * of radius `r`
 */
void bm_fillroundrect(Bitmap *b, int x0, int y0, int x1, int y1, int r);

/** `void bm_bezier3(Bitmap *b, int x0, int y0, int x1, int y1, int x2, int y2)`  \
 * Draws a Bezier curve with 3 control points `<x0,y0>`, `<x1,y1>` and `<x2,y2>`.
 */
void bm_bezier3(Bitmap *b, int x0, int y0, int x1, int y1, int x2, int y2);

/** `void bm_fill(Bitmap *b, int x, int y)`  \
 * Floodfills from <x,y> using the pen color.
 *
 * The color of the pixel at <x,y> is used as the source color.
 * The color of the pen is used as the target color.
 *
 * __NOTE__: The function does not take the clipping into account.
 */
void bm_fill(Bitmap *b, int x, int y);

/**
 * ### Font routines
 */

/** `typedef struct bitmap_font BmFont`  \
 * Structure that represents the details about a font.
 * * `const char *type` - a text description of the type of font
 * * `int (*puts)(Bitmap *b, int x, int y, const char *text)` -
 *   Pointer to the function that will actually render the text.
 * * `int (*width)(struct bitmap_font *font)` - Function that returns the
 *   width (in pixels) of a single character in the font.
 * * `int (*height)(struct bitmap_font *font)` - Function that returns the
 *   height (in pixels) of a single character in the font.
 * * `void *data` - Additonal data that may be required by the font.
 */
typedef struct bitmap_font {
    const char *type;
    int (*puts)(Bitmap *b, int x, int y, const char *text);
    int (*width)(struct bitmap_font *font);
    int (*height)(struct bitmap_font *font);
    void *data;
} BmFont;

/** `void bm_set_font(Bitmap *b, BmFont *font)`  \
 * Changes the font used to render text on the bitmap.
 */
void bm_set_font(Bitmap *b, BmFont *font);

/** `BmFont *bm_get_font(Bitmap *b)`  \
 * Retrieves the current font used to render text on the bitmap.
 */
BmFont *bm_get_font(Bitmap *b);

/** `void bm_reset_font(BmFont *b)` \
 * Resets the font used to draw on the `Bitmap` to the
 * default *Apple II*-inspired font
 */
void bm_reset_font(Bitmap *b);

/** `int bm_text_width(Bitmap *b, const char *s)`  \
 * Returns the width in pixels of a string of text.
 */
int bm_text_width(Bitmap *b, const char *s);

/** `int bm_text_height(Bitmap *b, const char *s)`  \
 * returns the height in pixels of a string of text.
 */
int bm_text_height(Bitmap *b, const char *s);

/** `void bm_putc(Bitmap *b, int x, int y, char c)`  \
 * Prints the character `c` at position `x,y` on the bitmap `b`.
 */
int bm_putc(Bitmap *b, int x, int y, char c);

/** `void bm_puts(Bitmap *b, int x, int y, const char *s)`  \
 * Prints the string `s` at position `x,y` on the bitmap `b`.
 */
int bm_puts(Bitmap *b, int x, int y, const char *text);

/** `void bm_printf(Bitmap *b, int x, int y, const char *fmt, ...)`  \
 * Prints a `printf()`-formatted style string to the bitmap `b`,
 * `fmt` is a `printf()`-style format string (It uses `vsnprintf()` internally).
 */
int bm_printf(Bitmap *b, int x, int y, const char *fmt, ...);

/**
 * ### Raster Font Functions
 * `bmp.h` has support for drawing text using raster fonts from any of the 
 * supported file types.
 *
 * The characters in the bitmap must be arranged like this:
 * ```
 *  !"#$%&'()*+,-./
 * 0123456789:;<=>?
 * @ABCDEFGHIJKLMNO
 * PQRSTUVWXYZ[\]^_
 * `abcdefghijklmno
 * pqrstuvwxyz{|}~ 
 * ```
 * The characters are in ASCII sequence, without the first 32 control characters. 
 * The pixel width and hight of the individual characters is calculated by dividing
 * the width and height of the bitmap by 16 and 6 respectively.
 *
 * Black (`#000000`) is used to indicate transparent pixels.
 *
 * `font.gif` is an example of an font that can be used in this way:  \
 * ![sample font image](fonts/font.gif)  \
 * The image is 128x48 pixels, so the individual characters are 8x8 pixels.
 * (128/16=8 and 48/6=8)
 */
 
/** `BmFont *bm_make_ras_font(const char *file, int spacing)`  \
 * Creates a raster font from a bitmap file named `file`.  \
 * The file can be in any of the supported file formats.
 * The `spacing` parameter determines. If it is zero, the width of the
 * characters is used.  \
 * A font created with this function must be freed by `bm_free_ras_font()`
 * afterwards.  \
 * The returned font's `type` will be set to `"RASTER_FONT"`
 */
BmFont *bm_make_ras_font(const char *file, int spacing);

/** `void bm_free_ras_font(BmFont *font)`  \
 * Frees a raster font previously created with `bm_make_ras_font()`.
 */
void bm_free_ras_font(BmFont *font);
 
/**
 * ### XBM Font Functions
 * `bmp.h` has support for drawing text using XBM fonts built
 * into the library. The XBM bitmaps can be compiled directly into 
 * a program's executable rather than being loaded at runtime, which
 * has the .
 */

/** `BmFont *bm_make_xbm_font(const unsigned char *bits, int spacing)`  \
 * Creates a font from a XBM bitmap. The XBM bitmap _must_ be modelled after the
 * fonts in the `fonts/` directory.
 */
BmFont *bm_make_xbm_font(const unsigned char *bits, int spacing);

/** `void bm_free_xbm_font(BmFont *font)`  \
 * Frees an XBM font previously created with `bm_make_xbm_font()`.
 */
void bm_free_xbm_font(BmFont *font);

#if defined(__cplusplus) || defined(c_plusplus)
} /* extern "C" */
#endif

/**
 * TODO
 * ----
 * - [] I should also go through the API and make the naming a bit more consistent.
 *   - Functions like `bm_rect()` should use `w,h` instead of `x1,y1` as parameters.
 * - [] How about replacing functions like `bm_brightness()` with a `bm_foreach()`
 *      function that takes a callback which iterates over all the pixels to simplify
 *      the API.  \
 *      The callback can look like `int (*)(Bitmap *b, int oldcolor, int x, int y)`
 * - [] I've added a precompiler definition `IGNORE_ALPHA` which causes all color
 *      operations to apply a `& 0x00FFFFFF` so that alpha values are ignored.  \
 *      It is not properly tested because I don't have any serious projects that
 *      depends on the alpha values at the moment.
 * - [x] `bm_fill()` should _perhaps_ stop using `bm_picker()`
 */

#endif /* BMP_H */
