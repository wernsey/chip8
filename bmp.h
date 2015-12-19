/*1 bmp.h
 *# Low-level routines to manipulate bitmap graphic files.\n
 *{
 ** It supports BMP, GIF and PCX files without any third party dependencies.
 ** PNG support is optional through libpng (http://www.libpng.org/pub/png/libpng.html). Use {{-DUSEPNG}} when compiling.
 ** JPG support is optional through libjpeg (http://www.ijg.org/). Use {{-DUSEJPG}} when compiling.
 *} 
 *2 References
 *{
 ** http://en.wikipedia.org/wiki/BMP_file_format
 ** http://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
 ** http://members.chello.at/~easyfilter/bresenham.html
 ** http://en.wikipedia.org/wiki/Flood_fill
 ** http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
 ** http://web.archive.org/web/20110706093850/http://free.pages.at/easyfilter/bresenham.html
 ** http://damieng.com/blog/2011/02/20/typography-in-8-bits-system-fonts
 ** http://www.w3.org/Graphics/GIF/spec-gif89a.txt
 ** Nelson, M.R. : "LZW Data Compression", Dr. Dobb's Journal, October 1989.
 ** http://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art011
 ** http://www.matthewflickinger.com/lab/whatsinagif/bits_and_bytes.asp
 ** http://web.archive.org/web/20100206055706/http://www.qzx.com/pc-gpe/pcx.txt
 ** http://www.shikadi.net/moddingwiki/PCX_Format
 *} 
 *2 License
 *[
 *# Author: Werner Stoop
 *# This is free and unencumbered software released into the public domain.
 *# http://unlicense.org/
 *]
 *2 API
 */
 
#ifndef BMP_H
#define BMP_H
 
#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/*@ typedef struct bitmap Bitmap
 *# Structure containing a bitmap image.
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

/*@ struct bitmap *bm_create(int w, int h)
 *# Creates a bitmap of the specified dimensions
 */
Bitmap *bm_create(int w, int h);

/*@ void bm_free(struct bitmap *b)
 *# Destroys a bitmap previously created with bm_create()
 */
void bm_free(Bitmap *b);

/*@ Bitmap *bm_load(const char *filename)
 *# Loads a bitmap file {{filename}} into a bitmap structure.\n
 *# It tries to detect the file type from the first bytes in the file.\n
 *# BMP, GIF and PCX support is always enabled, while JPG and PNG support 
 *# depends on how the library was compiled.\n
 *# Returns {{NULL}} if the file could not be loaded.
 */
Bitmap *bm_load(const char *filename);

#ifdef EOF /* <stdio.h> included? http://stackoverflow.com/q/29117606/115589 */
/*@ Bitmap *bm_load_fp(FILE *f)
 *# Loads a bitmap from a {{FILE*}} that's already open.\n
 *# BMP, GIF and PCX support is always enabled, while JPG and PNG support 
 *# depends on how the library was compiled.\n
 *# Returns {{NULL}} if the file could not be loaded.
 */
Bitmap *bm_load_fp(FILE *f);
#endif

#if defined(USESDL) && defined(_SDL_H)
/*@ Bitmap *bm_load_rw(SDL_RWops *file)
 *# Loads a bitmap from a SDL {{SDL_RWops*}} structure,
 *# for use with the SDL library (http://www.libsdl.org).\n
 *# This function is only available if the USESDL preprocessor macro
 *# is defined, and {{SDL.h}} is included before {{bmp.h}}.\n
 *# BMP, GIF and PCX support is always enabled, while JPG and PNG support 
 *# depends on how the library was compiled.\n
 *# Returns {{NULL}} if the file could not be loaded.\n
 */
Bitmap *bm_load_rw(SDL_RWops *file);
#endif

/*@ int bm_save(Bitmap *b, const char *fname)
 *# Saves the bitmap {{b}} to a BMP, JPG or PNG file named {{fname}}.\n
 *# If the filename contains {{".bmp"}}, {{".gif"}}, {{".pcx"}} or {{".jpg"}} the file is 
 *# saved as a BMP, GIF, PCX or JPG, respectively, otherwise the PNG format is the default.
 *# It can only save to JPG or PNG if JPG or PNG support is enabled
 *# at compile time, otherwise it saves to a BMP file.\n
 *# Returns 1 on success, 0 on failure.
 */
int bm_save(Bitmap *b, const char *fname);

/*@ Bitmap *bm_bind(int w, int h, unsigned char *data)
 *# Creates a bitmap structure bound to an existing array
 *# of pixel data (for example, an OpenGL texture or a SDL surface). The 
 *# {{data}} must be an array of {{w}} * {{h}} * 4 bytes of ARGB pixel data.
 *# The returned {{bitmap*}} must be deallocated with {{bm_unbind()}}
 *# rather than {{bm_free()}}.
 */
Bitmap *bm_bind(int w, int h, unsigned char *data);

/*@ void bm_rebind(Bitmap *b, unsigned char *data)
 *# Changes the data referred to by a bitmap structure previously
 *# created with a call to {{bm_bind()}}.
 *# The new data must be of the same dimensions as specified
 *# in the original {{bm_bind()}} call.
 */
void bm_rebind(Bitmap *b, unsigned char *data);

/*@ void bm_unbind(Bitmap *b)
 *# Deallocates the memory of a bitmap structure previously created 
 *# through {{bm_bind()}}.
 */
void bm_unbind(Bitmap *b);

/*@ Bitmap *bm_copy(Bitmap *b)
 *# Creates a duplicate of the bitmap structure.\n
 *# Caveat: Font information is not copied and must be set on the copy before
 *# text can be drawn on it.
 */
Bitmap *bm_copy(Bitmap *b);

/*@ void bm_flip_vertical(Bitmap *b)
 *# Flips the bitmap vertically.
 */
void bm_flip_vertical(Bitmap *b);

/*@ void bm_clip(Bitmap *b, int x0, int y0, int x1, int y1)
 *# Sets the clipping rectangle on the bitmap from {{x0,y0}} (inclusive)
 *# to {{x1,y1}} exclusive.
 */
void bm_clip(Bitmap *b, int x0, int y0, int x1, int y1);

/*@ void bm_unclip(Bitmap *b)
 *# Resets the bitmap {{b}}'s clipping rectangle.
 */
void bm_unclip(Bitmap *b);

/*@ unsigned int bm_get(Bitmap *b, int x, int y)
 *# Retrieves the value of the pixel at x,y as an integer.\n
 *# The return value is in the form 0xAABBGGRR
 */
unsigned int bm_get(Bitmap *b, int x, int y);

/*@ void bm_set(Bitmap *b, int x, int y, int c)
 *# Sets the value of the pixel at x,y to the color c.\n
 *# {{c}} is in the form 0xAABBGGRR
 */
void bm_set(Bitmap *b, int x, int y, unsigned int c);

/*@ void bm_set_rgb(Bitmap *b, int x, int y, unsigned char R, unsigned char G, unsigned char B)
 *# Sets a pixel at x,y in the bitmap {{b}} to the specified R,G,B color
 */ 
void bm_set_rgb(Bitmap *b, int x, int y, unsigned char R, unsigned char G, unsigned char B);

/*@ void bm_set_rgb_a(Bitmap *b, int x, int y, unsigned char R, unsigned char G, unsigned char B, unsigned char A)
 *# Sets a pixel at x,y in the bitmap {{b}} to the specified R,G,B,A color
 */ 
void bm_set_rgb_a(Bitmap *b, int x, int y, unsigned char R, unsigned char G, unsigned char B, unsigned char A);

/*@ unsigned char bm_getr(Bitmap *b, int x, int y)
 *# Retrieves the R value of the pixel at x,y in bitmap {{b}} 
 */
unsigned char bm_getr(Bitmap *b, int x, int y);

/*@ unsigned char bm_getg(Bitmap *b, int x, int y)
 *# Retrieves the G value of the pixel at x,y in bitmap {{b}} 
 */
unsigned char bm_getg(Bitmap *b, int x, int y);

/*@ unsigned char bm_getb(Bitmap *b, int x, int y)
 *# Retrieves the B value of the pixel at x,y in bitmap {{b}} 
 */
unsigned char bm_getb(Bitmap *b, int x, int y);

/*@ unsigned char bm_geta(Bitmap *b, int x, int y)
 *# Retrieves the A (alpha) value of the pixel at x,y in bitmap {{b}} 
 */
unsigned char bm_geta(Bitmap *b, int x, int y);

/*@ int bm_count_colors(Bitmap *b, int use_mask)
 *# Counts the number of distinct colours in the bitmap {{b}}.\n
 *# If {{use_mask}} is set, a mask is used internally to ignore the
 *# alpha value of each pixel.
 */
int bm_count_colors(Bitmap *b, int use_mask);

/*@ void bm_set_color_rgb(Bitmap *bm, unsigned char r, unsigned char g, unsigned char b)
 *# Sets the colour of the pen to (r,g,b)
 */
void bm_set_color_rgb(Bitmap *bm, unsigned char r, unsigned char g, unsigned char b);

/*@ void bm_set_alpha(Bitmap *bm, int a)
 *# Sets the alpha value of the pen to {{a}}
 */
void bm_set_alpha(Bitmap *bm, int a);

/*@ void bm_adjust_rgba(Bitmap *bm, float rf, float gf, float bf, float af)
 *# Multiplies the RGBA values of each pixel in the bitmap with {{rf,gf,bf,af}}
 */
void bm_adjust_rgba(Bitmap *bm, float rf, float gf, float bf, float af);

/*@ void bm_set_color_s(Bitmap *bm, const char *text)
 *# Sets the colour of the pen to a colour represented by text.
 *# The text can be in the HTML format, like #RRGGBB or one of
 *# a variety of colour names, eg.: "white", "black", "red".
 *# See {{bm_color_atoi()}} for more details on the format of
 *# the string.
 */
void bm_set_color_s(Bitmap *bm, const char *text);

/*@ unsigned int bm_color_atoi(const char *text)
 *# Converts a text string like "#FF00FF" or "white" to
 *# an integer of the form 0xFF00FF.
 *# The {{text}} parameter is not case sensitive and spaces are 
 *# ignored, so for example "darkred" and "Dark Red" are equivalent.
 *# The shorthand #RGB format is also supported
 *# (eg. #0fb, which is the same as #00FFBB)
 *# Additionally, it also supports the syntax "RGB(x,y,z)".
 *# The list of supported colors are based on the wikipedia's
 *# list of HTML and X11 Web colors:
 *# http://en.wikipedia.org/wiki/Web_colors
 */
unsigned int bm_color_atoi(const char *text);

/*@ void bm_set_color(Bitmap *bm, unsigned int col)
 *# Sets the colour of the pen to a colour represented 
 *# by an integer, like 0xAARRGGBB
 */
void bm_set_color(Bitmap *bm, unsigned int col);

/*@ void bm_get_color_rgb(Bitmap *bc, int *r, int *g, int *b)
 *# Retrieves the pen colour into &r, &g and &b.
 */
void bm_get_color_rgb(Bitmap *bm, int *r, int *g, int *b);

/*@ unsigned int bm_get_color(Bitmap *bc)
 *# Retrieves the pen colour.
 */
unsigned int bm_get_color(Bitmap *bm);

/*@ unsigned int bm_picker(Bitmap *bm, int x, int y)
 *# Sets the colour of the pen to the colour of the pixel at <x,y>
 *# on the bitmap.
 *# The pen colour can then be retrieved through {{bm_get_color()}}.\n
 *# It returns the integer representation of the colour.
 */
unsigned int bm_picker(Bitmap *bm, int x, int y);

/*@ int bm_color_is(Bitmap *bm, int x, int y, int r, int g, int b) 
 *# Returns 1 if the colour at <x,y> on the bitmap is (r,g,b), 0 otherwise
 */
int bm_color_is(Bitmap *bm, int x, int y, int r, int g, int b); 

/*@ double bm_cdist(int color1, int color2)
 # Returns the distance between two colors in the RGB color space.
 */
double bm_cdist(int color1, int color2);

/*@ int bm_lerp(int color1, int color2, double t)
 *# Computes the color that is a distance {{t}} along the line between 
 *# color1 and color2.\n
 *# If {{t}} is 0 it returns color1. If t is 1.0 it returns color2.
 */
int bm_lerp(int color1, int color2, double t);

/*@ int bm_brightness(int color, double adj)
 *# Adjusts the brightness of a {{color}} by a factor of {{adj}}.\n
 *# That is {{adj > 1.0}} makes the color brighter, while
 *# {{adj < 1.0}} makes the color darker.
 */
int bm_brightness(int color, double adj);

/*@ Bitmap *bm_fromXbm(int w, int h, unsigned char *data)
 *# Creates a Bitmap object from XBM data. 
 */
Bitmap *bm_fromXbm(int w, int h, unsigned char *data);

/*@ void bm_blit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h)
 *# Blits an area of w*h pixels at sx,sy on the src bitmap to 
 *# dx,dy on the {{dst}} bitmap.
 */
void bm_blit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h);

/*@ void bm_maskedblit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h)
 *# Blits an area of w*h pixels at sx,sy on the src bitmap to 
 *# dx,dy on the {{dst}} bitmap.\n
 *# Pixels on the {{src}} bitmap that matches the src bitmap colour are not blitted.
 *# The alpha value of the pixels on the {{src}} bitmap is not taken into account.
 */
void bm_maskedblit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h);

/*@ void bm_blit_ex(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, int mask)
 *# Extended blit function. Blits an area of sw*sh pixels at sx,sy from the {{src}} bitmap to 
 *# dx,dy on the {{dst}} bitmap into an area of dw*dh pixels, stretching or shrinking the blitted area as neccessary.\n
 *# If {{mask}} is non-zero, pixels on the src bitmap that matches the src bitmap colour are not blitted.
 *# The alpha value of the pixels on the {{src}} bitmap is not taken into account.
 */
void bm_blit_ex(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, int mask);

/*@ typedef int (*bm_blit_fun)(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, void *data);
 *# Prototype for the callback function to be passed to {{bm_blit_ex_fun()}}.\n
 *# {{dst}} is the destination bitmap and {{dx,dy}} is where the pixel is too be plotted.\n
 *# {{src}} is the source bitmap and {{sx,sy}} is where the colour of the pixel is to be obtained from.\n
 *# {{mask}} is the mask color of the source bitmap. The function can decide whether or not to blit the pixel based on this.\n
 *# It should return 1 on success. If it returns 0 bm_blit_ex_fun will terminate immediately.
 */
typedef int (*bm_blit_fun)(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int mask, void *data);

/*@ void bm_blit_ex_fun(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, bm_blit_fun fun, void *data);
 *# Blits a scaled bitmap from {{src}} to {{dst}}, but calls a callback function {{fun}}
 *# for each pixel to handle the plotting instead of plotting the pixel directly.\n
 *# This can be used for some specific post-processing after the position of a pixel is determined
 *# on the source and destination, but before the pixel is actually plotted.\n
 *# {{data}} is passed straight through to the callback function.\n
 *# The other parameters are the same as for {{bm_blit_ex()}}.
 */
void bm_blit_ex_fun(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, bm_blit_fun fun, void *data);

/*@ void bm_smooth(Bitmap *b)
 *# Smoothes the bitmap by essentially applying a 5x5 Gaussian filter.
 */
void bm_smooth(Bitmap *b);

/*@ void bm_apply_kernel(Bitmap *b, int dim, float kernel)
 *# Applies a {{dim}} x {{dim}} kernel to the image.
 */
void bm_apply_kernel(Bitmap *b, int dim, float kernel[]);

/*@ Bitmap *bm_resample(const Bitmap *in, int nw, int nh)
 *# Creates a new bitmap of dimensions nw*nh that is a scaled
 *# using the Nearest Neighbour method the input bitmap.\n
 *# The input bimap remains untouched.
 */
Bitmap *bm_resample(const Bitmap *in, int nw, int nh);

/*@ Bitmap *bm_resample_blin(const Bitmap *in, int nw, int nh)
 *# Creates a new bitmap of dimensions nw*nh that is a scaled
 *# using Bilinear Interpolation from the input bitmap. \n
 *# The input bimap remains untouched.\n
 *# Bilinear Interpolation is better suited for making an image larger.
 */
Bitmap *bm_resample_blin(const Bitmap *in, int nw, int nh);

/*@ Bitmap *bm_resample_bcub(const Bitmap *in, int nw, int nh)
 *# Creates a new bitmap of dimensions nw*nh that is a scaled
 *# using Bicubic Interpolation from the input bitmap.\n
 *# The input bimap remains untouched.\n
 *# Bicubic Interpolation is better suited for making an image smaller.
 */
Bitmap *bm_resample_bcub(const Bitmap *in, int nw, int nh);

/*@ void bm_swap_colour(Bitmap *b, unsigned char sR, unsigned char sG, unsigned char sB, unsigned char dR, unsigned char dG, unsigned char dB)
 *# Replaces all pixels of colour [sR,sG,sB] in bitmap {{b}} with the colour [dR,dG,dB]
 */
void bm_swap_colour(Bitmap *b, unsigned char sR, unsigned char sG, unsigned char sB, unsigned char dR, unsigned char dG, unsigned char dB);

#ifdef NULL /* <stdlib.h> included? - required for size_t */
/*@ void bm_reduce_palette(Bitmap *b, int palette[], size_t n)
 *# Reduces the colours in the bitmap {{b}} to the colors in {{palette}}
 *# by applying Floyd-Steinberg dithering.\n
 *# {{palette}} is an array of integers containing the new palette and
 *# {{n}} is the number of entries in the palette.
 */
void bm_reduce_palette(Bitmap *b, int palette[], size_t n);
#endif

/*2 Drawing Primitives
 *# {{bmp.h}} provides these methods for drawing graphics primitives.
 */

/*@ int bm_width(Bitmap *b)
 *# Retrieves the width of the bitmap {{b}}
 */
int bm_width(Bitmap *b);

/*@ int bm_height(Bitmap *b)
 *# Retrieves the height of the bitmap {{b}}
 */
int bm_height(Bitmap *b);

/*@ void bm_clear(Bitmap *b)
 *# Clears the bitmap to the colour of the pen.
 */
void bm_clear(Bitmap *b);

/*@ void bm_putpixel(Bitmap *b, int x, int y)
 *# Draws a single pixel at <x,y> using the pen colour.
 */
void bm_putpixel(Bitmap *b, int x, int y);

/*@ void bm_line(Bitmap *b, int x0, int y0, int x1, int y1)
 *# Draws a line from <x0,y0> to <x1,y1> using the colour of the pen.
 */
void bm_line(Bitmap *b, int x0, int y0, int x1, int y1);

/*@ void bm_rect(Bitmap *b, int x0, int y0, int x1, int y1)
 *# Draws a rectangle from <x0,y0> to <x1,y1> using the pen colour
 */
void bm_rect(Bitmap *b, int x0, int y0, int x1, int y1);

/*@ void bm_fillrect(Bitmap *b, int x0, int y0, int x1, int y1)
 *# Draws a filled rectangle from <x0,y0> to <x1,y1> using the pen colour
 */
void bm_fillrect(Bitmap *b, int x0, int y0, int x1, int y1);

/*@ void bm_dithrect(Bitmap *b, int x0, int y0, int x1, int y1)
 *# Draws a dithered rectangle from <x0,y0> to <x1,y1> using the pen colour
 */
void bm_dithrect(Bitmap *b, int x0, int y0, int x1, int y1);

/*@ void bm_circle(Bitmap *b, int x0, int y0, int r)
 *# Draws a circle of radius {{r}} centered at <x,y> using the pen colour
 */
void bm_circle(Bitmap *b, int x0, int y0, int r);

/*@ void bm_fillcircle(Bitmap *b, int x0, int y0, int r)
 *# Draws a filled circle of radius {{r}} centered at <x,y> using the pen colour
 */
void bm_fillcircle(Bitmap *b, int x0, int y0, int r);

/*@ void bm_ellipse(Bitmap *b, int x0, int y0, int x1, int y1)
 *# Draws an ellipse that occupies the rectangle from <x0,y0> to 
 *# <x1,y1>, using the pen colour
 */
void bm_ellipse(Bitmap *b, int x0, int y0, int x1, int y1);

/* MISSING: bm_fillellipse */

/*@ void bm_round_rect(Bitmap *b, int x0, int y0, int x1, int y1, int r)
 *# Draws a rect from <x0,y0> to <x1,y1> using the pen colour with rounded corners 
 *# of radius {{r}}
 */
void bm_roundrect(Bitmap *b, int x0, int y0, int x1, int y1, int r);

/*@ void bm_fill_round_rect(Bitmap *b, int x0, int y0, int x1, int y1, int r)
 *# Draws a filled rect from <x0,y0> to <x1,y1> using the pen colour with rounded corners 
 *# of radius {{r}}
 */
void bm_fillroundrect(Bitmap *b, int x0, int y0, int x1, int y1, int r);

/* FIXME: The bezier works kinda-sorta for specific values of the control points, but
 * it is unreliable, so I'd replace it with something more robust at a later stage.
 */
void bm_bezier3(Bitmap *b, int x0, int y0, int x1, int y1, int x2, int y2);

/*@ void bm_fill(Bitmap *b, int x, int y)
 *# Floodfills from <x,y> using the pen colour.\n
 *# The colour of the pixel at <x,y> is used as the source colour.
 *# The colour of the pen is used as the target colour.
 *N The function does not take the clipping into account.
 */
void bm_fill(Bitmap *b, int x, int y);

/*2 Font routines
 */
 
/*@ typedef struct bitmap_font BmFont
 *# Structure that represents the details about a font.
 *{
 ** {{const char *type}} - a text description of the type of font
 ** {{int (*puts)(Bitmap *b, int x, int y, const char *text)}} -
 *# Pointer to the structure that will actually render the text.
 ** {{void (*dtor)(struct bitmap_font *font)}} -
 *# Destructor that will be used to delete the font if the bitmap is deleted
 *# or another font chosen.
 ** {{int (*width)(struct bitmap_font *font)}} - Function that returns the
 *# width (in pixels) of a single character in the font.
 ** {{int (*height)(struct bitmap_font *font)}} - Function that returns the
 *# height (in pixels) of a single character in the font.
 ** {{void *data}} - Additonal data that may be required by the font.
 *}
 */
typedef struct bitmap_font {	
	const char *type;
	int (*puts)(Bitmap *b, int x, int y, const char *text);
	void (*dtor)(struct bitmap_font *font);
	int (*width)(struct bitmap_font *font);
	int (*height)(struct bitmap_font *font);	
	void *data;
} BmFont; 
 
/*@ bm_set_font(Bitmap *b, , BmFont *font)
 *# Changes the font used to render text on the bitmap.
 */
void bm_set_font(Bitmap *b, BmFont *font);

/*@ int bm_text_width(Bitmap *b, const char *s)
 *# Returns the width in pixels of a string of text.
 */
int bm_text_width(Bitmap *b, const char *s);

/*@ int bm_text_height(Bitmap *b, const char *s)
 *# returns the height in pixels of a string of text.
 */
int bm_text_height(Bitmap *b, const char *s);

/*@ void bm_putc(Bitmap *b, int x, int y, char c)
 *# Prints the character {{c}} at position {{x,y}} on the bitmap {{b}}.
 */
int bm_putc(Bitmap *b, int x, int y, char c);

/*@ void bm_puts(Bitmap *b, int x, int y, const char *s)
 *# Prints the string {{s}} at position {{x,y}} on the bitmap {{b}}.
 */
int bm_puts(Bitmap *b, int x, int y, const char *text);

/*@ void bm_printf(Bitmap *b, int x, int y, const char *fmt, ...)
 *# Prints a {{printf()}}-formatted style string to the bitmap {{b}},
 *# {{fmt}} is a {{printf()}}-style format string (It uses {{vsnprintf()}} internally).
 */
int bm_printf(Bitmap *b, int x, int y, const char *fmt, ...);

/*3 XBM Font Functions
 *# {{bmp.h}} has rudimetary support for drawing text using fonts in 
 *# XBM bitmaps. The XBM bitmaps are compiled into your program's executable
 *# rather than being loaded from a file.
 */
 
/*@ BmFont *bm_make_xbm_font(const unsigned char *bits, int spacing)
 *# Creates a font from a XBM bitmap. The XBM bitmap {/must/} be modeled after the
 *# fonts in the {{fonts/}} directory.
 */
BmFont *bm_make_xbm_font(const unsigned char *bits, int spacing);

#ifndef NO_FONTS
/*3 Built-in XBM fonts.
 *# There are a number of built-in fonts available which is placed in the
 *# {{fonts/}} directory.\n
 *# If you don't wish to use these, compile with the {{-DNO_FONTS}} flag.
 *@ enum bm_fonts
 *# Built-in XBM fonts that can be set with {{bm_std_font()}}
 *{
 ** {{BM_FONT_NORMAL}} - A default font.
 ** {{BM_FONT_BOLD}} - A bold font.
 ** {{BM_FONT_CIRCUIT}} - A "computer" font that looks like a circuit board.
 ** {{BM_FONT_HAND}} - A font that looks like hand writing.
 ** {{BM_FONT_SMALL}} - A small font.
 ** {{BM_FONT_SMALL_I}} - A variant of the small font with the foreground and background inverted.
 ** {{BM_FONT_THICK}} - A thicker variant of the normal font.
 *}
 */
enum bm_fonts {
	BM_FONT_NORMAL,
	BM_FONT_BOLD,
	BM_FONT_CIRCUIT,
	BM_FONT_HAND,
	BM_FONT_SMALL,
	BM_FONT_SMALL_I,
	BM_FONT_THICK
};

/*@ void bm_std_font(Bitmap *b, enum bm_fonts font)
 *# Sets the font to one of the standard (built-in) ones.
 */
void bm_std_font(Bitmap *b, enum bm_fonts font);

/*@ int bm_font_index(const char *name)
 *# Returns the index of one of the standard (built-in) XBM fonts identified
 *# by the {{name}}.
 */
int bm_font_index(const char *name);

/*@ const char *bm_font_name(int index);
 *# Returns the name of the standard (built-in) XBM font identified
 *# by {{index}}, which should fall in the range of the {{enum bm_fonts}}.
 */
const char *bm_font_name(int index);
#endif /* NO_FONTS */

#if defined(__cplusplus) || defined(c_plusplus)
} /* extern "C" */
#endif

#endif /* BMP_H */
