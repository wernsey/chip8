#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include <float.h>
#include <assert.h>

#ifdef USESDL
#  ifdef ANDROID
#    include <SDL.h>
#  else
#    include <SDL2/SDL.h>
#  endif
#endif

/*
Use the -DUSEPNG compiler option to enable PNG support via libpng.
If you use it, you need to link against the libpng (-lpng)
and zlib (-lz) libraries.
Use the -DUSEJPG compiler option to enable JPG support via libjpg.
I've decided to keep both optional, for situations where
you may not want to import a bunch of third party libraries.
*/
#ifdef USEPNG
#   include <png.h>
#endif
#ifdef USEJPG
#   include <jpeglib.h>
#   include <setjmp.h>
#endif

#include "bmp.h"

/*
TODO:
    The alpha support is a recent addition, so it is still a bit
    sporadic and not well tested.
    I may also decide to change the API around it (especially wrt.
    blitting) in the future.

    Also, functions like bm_color_atoi() and bm_set_color() does
    not take the alpha value into account. The integers they return and
    accept is still 0xRRGGBB instead of 0xRRGGBBAA - It probably implies
    that I should change the type to unsigned int where it currently is
    just int.
*/

#ifndef NO_FONTS
/* I basically drew font.xbm from the fonts at
 * http://damieng.com/blog/2011/02/20/typography-in-8-bits-system-fonts
 * The Apple ][ font turned out to be the nicest normal font.
 * The bold font was inspired by Commodore 64.
 * I later added some others for a bit of variety.
 */
#include "fonts/bold.xbm"
#include "fonts/circuit.xbm"
#include "fonts/hand.xbm"
#include "fonts/normal.xbm"
#include "fonts/small.xbm"
#include "fonts/smallinv.xbm"
#include "fonts/thick.xbm"
#endif

#define FONT_WIDTH 96

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#pragma pack(push, 1) /* Don't use any padding (Windows compilers) */

/* Data structures for the header of BMP files. */
struct bmpfile_magic {
  unsigned char magic[2];
};

struct bmpfile_header {
  uint32_t filesz;
  uint16_t creator1;
  uint16_t creator2;
  uint32_t bmp_offset;
};

struct bmpfile_dibinfo {
  uint32_t header_sz;
  int32_t width;
  int32_t height;
  uint16_t nplanes;
  uint16_t bitspp;
  uint32_t compress_type;
  uint32_t bmp_bytesz;
  int32_t hres;
  int32_t vres;
  uint32_t ncolors;
  uint32_t nimpcolors;
};

struct bmpfile_colinfo {
    uint8_t b, g, r, a;
};

/* RGB triplet used for palettes in PCX and GIF support */
struct rgb_triplet {
    unsigned char r, g, b;
};
#pragma pack(pop)

#define BM_BPP          4 /* Bytes per Pixel */
#define BM_BLOB_SIZE(B) (B->w * B->h * BM_BPP)
#define BM_ROW_SIZE(B)  (B->w * BM_BPP)

#define BM_GET(b, x, y) (*((unsigned int*)(b->data + y * BM_ROW_SIZE(b) + x * BM_BPP)))
#define BM_SET(b, x, y, c) *((unsigned int*)(b->data + y * BM_ROW_SIZE(b) + x * BM_BPP)) = c

#define BM_SET_RGBA(BMP, X, Y, R, G, B, A) do { \
        int _p = ((Y) * BM_ROW_SIZE(BMP) + (X)*BM_BPP); \
        BMP->data[_p++] = B;\
        BMP->data[_p++] = G;\
        BMP->data[_p++] = R;\
        BMP->data[_p++] = A;\
    } while(0)

#define BM_GETB(B,X,Y) (B->data[((Y) * BM_ROW_SIZE(B) + (X) * BM_BPP) + 0])
#define BM_GETG(B,X,Y) (B->data[((Y) * BM_ROW_SIZE(B) + (X) * BM_BPP) + 1])
#define BM_GETR(B,X,Y) (B->data[((Y) * BM_ROW_SIZE(B) + (X) * BM_BPP) + 2])
#define BM_GETA(B,X,Y) (B->data[((Y) * BM_ROW_SIZE(B) + (X) * BM_BPP) + 3])

/* N=0 -> B, N=1 -> G, N=2 -> R, N=3 -> A */
#define BM_GETN(B,N,X,Y) (B->data[((Y) * BM_ROW_SIZE(B) + (X) * BM_BPP) + (N)])

Bitmap *bm_create(int w, int h) {
    Bitmap *b = malloc(sizeof *b);

    b->w = w;
    b->h = h;

    b->clip.x0 = 0;
    b->clip.y0 = 0;
    b->clip.x1 = w;
    b->clip.y1 = h;

    b->data = malloc(BM_BLOB_SIZE(b));
    memset(b->data, 0x00, BM_BLOB_SIZE(b));

    b->font = NULL;
#ifndef NO_FONTS
    bm_std_font(b, BM_FONT_NORMAL);
#endif

    bm_set_color(b, 0xFFFFFFFF);

    return b;
}

/* Wraps around the stdio functions, so I don't have to duplicate my code
    for SDL2's RWops support.
    It is unfortunately an abstraction over an abstraction in the case of
    SDL_RWops, but such is life. */
typedef struct {
    void *data;
    size_t (*fread)(void* ptr, size_t size, size_t nobj, void* stream);
    long (*ftell)(void* stream);
    int (*fseek)(void* stream, long offset, int origin);
} BmReader;

static BmReader make_file_reader(FILE *fp) {
    BmReader rd;
    rd.data = fp;
    rd.fread = (size_t(*)(void*,size_t,size_t,void*))fread;
    rd.ftell = (long(*)(void* ))ftell;
    rd.fseek = (int(*)(void*,long,int))fseek;
    return rd;
}

#ifdef USESDL
static size_t rw_fread(void *ptr, size_t size, size_t nobj, SDL_RWops *stream) {
    return SDL_RWread(stream, ptr, size, nobj);
}
static long rw_ftell(SDL_RWops *stream) {
    return SDL_RWtell(stream);
}
static int rw_fseek(SDL_RWops *stream, long offset, int origin) {
    switch (origin) {
        case SEEK_SET: origin = RW_SEEK_SET; break;
        case SEEK_CUR: origin = RW_SEEK_CUR; break;
        case SEEK_END: origin = RW_SEEK_END; break;
    }
    if(SDL_RWseek(stream, offset, origin) < 0)
        return 1;
    return 0;
}
static BmReader make_rwops_reader(SDL_RWops *rw) {
    BmReader rd;
    rd.data = rw;
    rd.fread = (size_t(*)(void*,size_t,size_t,void*))rw_fread;
    rd.ftell = (long(*)(void* ))rw_ftell;
    rd.fseek = (int(*)(void*,long,int))rw_fseek;
    return rd;
}
#endif

Bitmap *bm_load(const char *filename) {
    Bitmap *bmp;
    FILE *f = fopen(filename, "rb");
    if(!f)
        return NULL;
    bmp = bm_load_fp(f);
    fclose(f);
    return bmp;
}

static Bitmap *bm_load_bmp_rd(BmReader rd);
static Bitmap *bm_load_gif_rd(BmReader rd);
static Bitmap *bm_load_pcx_rd(BmReader rd);
#ifdef USEPNG
static Bitmap *bm_load_png_fp(FILE *f);
#endif
#ifdef USEJPG
static Bitmap *bm_load_jpg_fp(FILE *f);
#endif

Bitmap *bm_load_fp(FILE *f) {
    unsigned char magic[3];

    long start = ftell(f),
        isbmp = 0, ispng = 0, isjpg = 0, ispcx = 0, isgif = 0;
    /* Tries to detect the type of file by looking at the first bytes. */
    if(fread(magic, sizeof magic, 1, f) == 1) {
        if(!memcmp(magic, "BM", 2))
            isbmp = 1;
        else if(!memcmp(magic, "GIF", 3))
            isgif = 1;
        else if(magic[0] == 0xFF && magic[1] == 0xD8)
            isjpg = 1;
        else if(magic[0] == 0x0A)
            ispcx = 1;
        else
            ispng = 1; /* Assume PNG by default; the other magic numbers are simpler */
    } else {
        return NULL;
    }
    fseek(f, start, SEEK_SET);

#ifdef USEJPG
    if(isjpg)
        return bm_load_jpg_fp(f);
#else
    (void)isjpg;
#endif
#ifdef USEPNG
    if(ispng)
        return bm_load_png_fp(f);
#else
    (void)ispng;
#endif
    if(isgif) {
        BmReader rd = make_file_reader(f);
        return bm_load_gif_rd(rd);
    }
    if(ispcx) {
        BmReader rd = make_file_reader(f);
        return bm_load_pcx_rd(rd);
    }
    if(isbmp) {
        BmReader rd = make_file_reader(f);
        return bm_load_bmp_rd(rd);
    }
    return NULL;
}

static Bitmap *bm_load_bmp_rd(BmReader rd) {
    struct bmpfile_magic magic;
    struct bmpfile_header hdr;
    struct bmpfile_dibinfo dib;
    struct bmpfile_colinfo *palette = NULL;

    Bitmap *b = NULL;

    int rs, i, j;
    char *data = NULL;

    long start_offset = rd.ftell(rd.data);

    if(rd.fread(&magic, sizeof magic, 1, rd.data) != 1) {
        return NULL;
    }

    if(memcmp(magic.magic, "BM", 2)) {
        return NULL;
    }

    if(rd.fread(&hdr, sizeof hdr, 1, rd.data) != 1 ||
        rd.fread(&dib, sizeof dib, 1, rd.data) != 1) {
        return NULL;
    }

    if((dib.bitspp != 8 && dib.bitspp != 24) || dib.compress_type != 0) {
        /* Unsupported BMP type. TODO (maybe): support more types? */
        return NULL;
    }

    b = bm_create(dib.width, dib.height);
    if(!b) {
        return NULL;
    }

    if(dib.bitspp <= 8) {
        if(!dib.ncolors) {
            dib.ncolors = 1 << dib.bitspp;
        }
        palette = calloc(dib.ncolors, sizeof *palette);
        if(!palette) {
            goto error;
        }
        if(rd.fread(palette, sizeof *palette, dib.ncolors, rd.data) != dib.ncolors) {
            goto error;
        }
    }

    if(rd.fseek(rd.data, hdr.bmp_offset + start_offset, SEEK_SET) != 0) {
        goto error;
    }

    rs = ((dib.width * dib.bitspp / 8) + 3) & ~3;
    assert(rs % 4 == 0);

    data = malloc(rs * b->h);
    if(!data) {
        goto error;
    }

    if(dib.bmp_bytesz == 0) {
        if(rd.fread(data, 1, rs * b->h, rd.data) != rs * b->h) {
            goto error;
        }
    } else {
        if(rd.fread(data, 1, dib.bmp_bytesz, rd.data) != dib.bmp_bytesz) {
            goto error;
        }
    }

    if(dib.bitspp == 8) {
        for(j = 0; j < b->h; j++) {
            for(i = 0; i < b->w; i++) {
                uint8_t p = data[(b->h - (j) - 1) * rs + i];
                assert(p < dib.ncolors);
                BM_SET_RGBA(b, i, j, palette[p].r, palette[p].g, palette[p].b, palette[p].a);
            }
        }
    } else {
        for(j = 0; j < b->h; j++) {
            for(i = 0; i < b->w; i++) {
                int p = ((b->h - (j) - 1) * rs + (i)*3);
                BM_SET_RGBA(b, i, j, data[p+2], data[p+1], data[p+0], 0xFF);
            }
        }
    }

end:
    if(data) free(data);
    if(palette != NULL) free(palette);

    return b;
error:
    if(b)
        bm_free(b);
    b = NULL;
    goto end;
}

static int bm_save_bmp(Bitmap *b, const char *fname);
static int bm_save_gif(Bitmap *b, const char *fname);
static int bm_save_pcx(Bitmap *b, const char *fname);
#ifdef USEPNG
static int bm_save_png(Bitmap *b, const char *fname);
#endif
#ifdef USEJPG
static int bm_save_jpg(Bitmap *b, const char *fname);
#endif

int bm_save(Bitmap *b, const char *fname) {
    /* If the filename contains ".bmp" save as BMP,
       if the filename contains ".jpg" save as JPG,
        otherwise save as PNG */
    char *lname = strdup(fname), *c,
        bmp = 0, jpg = 0, png = 0, pcx = 0, gif = 0;
    for(c = lname; *c; c++)
        *c = tolower(*c);
    bmp = !!strstr(lname, ".bmp");
    pcx = !!strstr(lname, ".pcx");
    gif = !!strstr(lname, ".gif");
    jpg = !!strstr(lname, ".jpg") || !!strstr(lname, ".jpeg");
    png = !bmp && !jpg && !pcx;
    free(lname);

#ifdef USEPNG
    if(png)
        return bm_save_png(b, fname);
#else
    (void)png;
#endif
#ifdef USEJPG
    if(jpg)
        return bm_save_jpg(b, fname);
#else
    (void)jpg;
#endif
    if(gif)
        return bm_save_gif(b, fname);
    if(pcx)
        return bm_save_pcx(b, fname);
    return bm_save_bmp(b, fname);
}

static int bm_save_bmp(Bitmap *b, const char *fname) {

    /* TODO: Now that I have a function to count colors, maybe
        I should choose to save a bitmap as 8-bit if there
        are <= 256 colors in the image? */

    struct bmpfile_magic magic = {{'B','M'}};
    struct bmpfile_header hdr;
    struct bmpfile_dibinfo dib;
    FILE *f;

    int rs, padding, i, j;
    char *data;

    padding = 4 - ((b->w * 3) % 4);
    if(padding > 3) padding = 0;
    rs = b->w * 3 + padding;
    assert(rs % 4 == 0);

    f = fopen(fname, "wb");
    if(!f) return 0;

    hdr.creator1 = 0;
    hdr.creator2 = 0;
    hdr.bmp_offset = sizeof magic + sizeof hdr + sizeof dib;

    dib.header_sz = sizeof dib;
    dib.width = b->w;
    dib.height = b->h;
    dib.nplanes = 1;
    dib.bitspp = 24;
    dib.compress_type = 0;
    dib.hres = 2835;
    dib.vres = 2835;
    dib.ncolors = 0;
    dib.nimpcolors = 0;

    dib.bmp_bytesz = rs * b->h;
    hdr.filesz = hdr.bmp_offset + dib.bmp_bytesz;

    fwrite(&magic, sizeof magic, 1, f);
    fwrite(&hdr, sizeof hdr, 1, f);
    fwrite(&dib, sizeof dib, 1, f);

    data = malloc(dib.bmp_bytesz);
    if(!data) {
        fclose(f);
        return 0;
    }
    memset(data, 0x00, dib.bmp_bytesz);

    for(j = 0; j < b->h; j++) {
        for(i = 0; i < b->w; i++) {
            int p = ((b->h - (j) - 1) * rs + (i)*3);
            data[p+2] = BM_GETR(b, i, j);
            data[p+1] = BM_GETG(b, i, j);
            data[p+0] = BM_GETB(b, i, j);
        }
    }

    fwrite(data, 1, dib.bmp_bytesz, f);

    free(data);

    fclose(f);
    return 1;
}

#ifdef USEPNG
/*
http://zarb.org/~gc/html/libpng.html
http://www.labbookpages.co.uk/software/imgProc/libPNG.html
*/
static Bitmap *bm_load_png_fp(FILE *f) {
    Bitmap *bmp = NULL;

    unsigned char header[8];
    png_structp png = NULL;
    png_infop info = NULL;
    int number_of_passes;
    png_bytep * rows = NULL;

    int w, h, ct, bpp, x, y;

    if((fread(header, 1, 8, f) != 8) || png_sig_cmp(header, 0, 8)) {
        goto error;
    }

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png) {
        goto error;
    }
    info = png_create_info_struct(png);
    if(!info) {
        goto error;
    }
    if(setjmp(png_jmpbuf(png))) {
        goto error;
    }

    png_init_io(png, f);
    png_set_sig_bytes(png, 8);

    png_read_info(png, info);

    w = png_get_image_width(png, info);
    h = png_get_image_height(png, info);
    ct = png_get_color_type(png, info);

    bpp = png_get_bit_depth(png, info);
    assert(bpp == 8);(void)bpp;

    if(ct != PNG_COLOR_TYPE_RGB && ct != PNG_COLOR_TYPE_RGBA) {
        goto error;
    }

    number_of_passes = png_set_interlace_handling(png);
    (void)number_of_passes;

    bmp = bm_create(w,h);

    if(setjmp(png_jmpbuf(png))) {
        goto error;
    }

    rows = malloc(h * sizeof *rows);
    for(y = 0; y < h; y++)
        rows[y] = malloc(png_get_rowbytes(png,info));

    png_read_image(png, rows);

    /* Convert to my internal representation */
    if(ct == PNG_COLOR_TYPE_RGBA) {
        for(y = 0; y < h; y++) {
            png_byte *row = rows[y];
            for(x = 0; x < w; x++) {
                png_byte *ptr = &(row[x * 4]);
                BM_SET_RGBA(bmp, x, y, ptr[0], ptr[1], ptr[2], ptr[3]);
            }
        }
    } else if(ct == PNG_COLOR_TYPE_RGB) {
        for(y = 0; y < h; y++) {
            png_byte *row = rows[y];
            for(x = 0; x < w; x++) {
                png_byte *ptr = &(row[x * 3]);
                BM_SET_RGBA(bmp, x, y, ptr[0], ptr[1], ptr[2], 0xFF);
            }
        }
    }

    goto done;
error:
    if(bmp) bm_free(bmp);
    bmp = NULL;
done:
    if (info != NULL) png_free_data(png, info, PNG_FREE_ALL, -1);
    if (png != NULL) png_destroy_read_struct(&png, NULL, NULL);
    if(rows) {
        for(y = 0; y < h; y++) {
            free(rows[y]);
        }
        free(rows);
    }
    return bmp;
}

static int bm_save_png(Bitmap *b, const char *fname) {

    png_structp png = NULL;
    png_infop info = NULL;
    int y, rv = 1;

    FILE *f = fopen(fname, "wb");
    if(!f) {
        return 0;
    }

    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png) {
        goto error;
    }

    info = png_create_info_struct(png);
    if(!info) {
        goto error;
    }

    if(setjmp(png_jmpbuf(png))) {
        goto error;
    }

    png_init_io(png, f);

    if(setjmp(png_jmpbuf(png))) {
        goto error;
    }

    png_set_IHDR(png, info, b->w, b->h, 8, PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
        PNG_FILTER_TYPE_BASE);

    png_write_info(png, info);

    png_bytep row = malloc(4 * b->w * sizeof *row);
    int x;
    for(y = 0; y < b->h; y++) {
        png_bytep r = row;
        for(x = 0; x < b->w; x++) {
            *r++ = BM_GETR(b,x,y);
            *r++ = BM_GETG(b,x,y);
            *r++ = BM_GETB(b,x,y);
            *r++ = BM_GETA(b,x,y);
        }
        png_write_row(png, row);
    }

    if(setjmp(png_jmpbuf(png))) {
        goto error;
    }

    goto done;
error:
    rv = 0;
done:
    if(info) png_free_data(png, info, PNG_FREE_ALL, -1);
    if(png) png_destroy_write_struct(&png, NULL);
    fclose(f);
    return rv;
}
#endif

#ifdef USEJPG
struct jpg_err_handler {
    struct jpeg_error_mgr pub;
    jmp_buf jbuf;
};

static void jpg_on_error(j_common_ptr cinfo) {
    struct jpg_err_handler *err = (struct jpg_err_handler *) cinfo->err;
    longjmp(err->jbuf, 1);
}

static Bitmap *bm_load_jpg_fp(FILE *f) {
    struct jpeg_decompress_struct cinfo;
    struct jpg_err_handler jerr;
    Bitmap *bmp = NULL;
    int i, j, row_stride;
    unsigned char *data;
    JSAMPROW row_pointer[1];

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpg_on_error;
    if(setjmp(jerr.jbuf)) {
        jpeg_destroy_decompress(&cinfo);
        return NULL;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, f);

    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;

    bmp = bm_create(cinfo.image_width, cinfo.image_height);
    if(!bmp) {
        return NULL;
    }
    row_stride = bmp->w * 3;

    data = malloc(row_stride);
    if(!data) {
        return NULL;
    }
    memset(data, 0x00, row_stride);
    row_pointer[0] = data;

    jpeg_start_decompress(&cinfo);

    for(j = 0; j < cinfo.output_height; j++) {
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
        for(i = 0; i < bmp->w; i++) {
            unsigned char *ptr = &(data[i * 3]);
            BM_SET_RGBA(bmp, i, j, ptr[0], ptr[1], ptr[2], 0xFF);
        }
    }
    free(data);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return bmp;
}
static int bm_save_jpg(Bitmap *b, const char *fname) {
    struct jpeg_compress_struct cinfo;
    struct jpg_err_handler jerr;
    FILE *f;
    int i, j;
    JSAMPROW row_pointer[1];
    int row_stride;
    unsigned char *data;

    if(!(f = fopen(fname, "wb"))) {
        return 0;
    }

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpg_on_error;
    if(setjmp(jerr.jbuf)) {
        jpeg_destroy_compress(&cinfo);
        return 0;
    }
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, f);

    cinfo.image_width = b->w;
    cinfo.image_height = b->h;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    /*jpeg_set_quality(&cinfo, 100, TRUE);*/

    row_stride = b->w * 3;

    data = malloc(row_stride);
    if(!data) {
        fclose(f);
        return 0;
    }
    memset(data, 0x00, row_stride);

    jpeg_start_compress(&cinfo, TRUE);
    for(j = 0; j < b->h; j++) {
        for(i = 0; i < b->w; i++) {
            data[i*3+0] = BM_GETR(b, i, j);
            data[i*3+1] = BM_GETG(b, i, j);
            data[i*3+2] = BM_GETB(b, i, j);
        }
        row_pointer[0] = data;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    free(data);

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    fclose(f);
    return 1;
}
#endif

#ifdef USESDL
/* Some functions to load graphics through the SDL_RWops
    related functions.
*/
#  ifdef USEPNG
static Bitmap *bm_load_png_rw(SDL_RWops *rw);
#  endif
#  ifdef USEJPG
static Bitmap *bm_load_jpg_rw(SDL_RWops *rw);
#  endif

Bitmap *bm_load_rw(SDL_RWops *rw) {
    unsigned char magic[3];
    long start = SDL_RWtell(rw);
    long isbmp = 0, ispng = 0, isjpg = 0, ispcx = 0, isgif = 0;
    if(SDL_RWread(rw, magic, sizeof magic, 1) == 1) {
        if(!memcmp(magic, "BM", 2))
            isbmp = 1;
        else if(!memcmp(magic, "GIF", 3))
            isgif = 1;
        else if(magic[0] == 0xFF && magic[1] == 0xD8)
            isjpg = 1;
        else if(magic[0] == 0x0A)
            ispcx = 1;
        else
            ispng = 1; /* Assume PNG by default.
                    JPG and BMP magic numbers are simpler */
    }
    SDL_RWseek(rw, start, RW_SEEK_SET);

#  ifdef USEJPG
    if(isjpg)
        return bm_load_jpg_rw(rw);
#  else
    (void)isjpg;
#  endif
#  ifdef USEPNG
    if(ispng)
        return bm_load_png_rw(rw);
#  else
    (void)ispng;
#  endif
    if(isgif) {
        BmReader rd = make_rwops_reader(rw);
        return bm_load_gif_rd(rd);
    }
    if(ispcx) {
        BmReader rd = make_rwops_reader(rw);
        return bm_load_pcx_rd(rd);
    }
    if(isbmp) {
        BmReader rd = make_rwops_reader(rw);
        return bm_load_bmp_rd(rd);
    }
    return NULL;
}

#  ifdef USEPNG
/*
Code to read a PNG from a SDL_RWops
http://www.libpng.org/pub/png/libpng-1.2.5-manual.html
http://blog.hammerian.net/2009/reading-png-images-from-memory/
*/
static void read_rwo_data(png_structp png_ptr, png_bytep data, png_size_t length) {
    SDL_RWops *rw = png_get_io_ptr(png_ptr);
    SDL_RWread(rw, data, 1, length);
}

static Bitmap *bm_load_png_rw(SDL_RWops *rw) {
        Bitmap *bmp = NULL;

    unsigned char header[8];
    png_structp png = NULL;
    png_infop info = NULL;
    int number_of_passes;
    png_bytep * rows = NULL;

    int w, h, ct, bpp, x, y;

    if((SDL_RWread(rw, header, 1, 8) != 8) || png_sig_cmp(header, 0, 8)) {
        goto error;
    }

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png) {
        goto error;
    }
    info = png_create_info_struct(png);
    if(!info) {
        goto error;
    }
    if(setjmp(png_jmpbuf(png))) {
        goto error;
    }

    png_init_io(png, NULL);
    png_set_read_fn(png, rw, read_rwo_data);

    png_set_sig_bytes(png, 8);

    png_read_info(png, info);

    w = png_get_image_width(png, info);
    h = png_get_image_height(png, info);
    ct = png_get_color_type(png, info);

    bpp = png_get_bit_depth(png, info);
    assert(bpp == 8);(void)bpp;

    if(ct != PNG_COLOR_TYPE_RGB && ct != PNG_COLOR_TYPE_RGBA) {
        goto error;
    }

    number_of_passes = png_set_interlace_handling(png);
    (void)number_of_passes;

    bmp = bm_create(w,h);

    if(setjmp(png_jmpbuf(png))) {
        goto error;
    }

    rows = malloc(h * sizeof *rows);
    for(y = 0; y < h; y++)
        rows[y] = malloc(png_get_rowbytes(png,info));

    png_read_image(png, rows);

    /* Convert to my internal representation */
    if(ct == PNG_COLOR_TYPE_RGBA) {
        for(y = 0; y < h; y++) {
            png_byte *row = rows[y];
            for(x = 0; x < w; x++) {
                png_byte *ptr = &(row[x * 4]);
                BM_SET_RGBA(bmp, x, y, ptr[0], ptr[1], ptr[2], ptr[3]);
            }
        }
    } else if(ct == PNG_COLOR_TYPE_RGB) {
        for(y = 0; y < h; y++) {
            png_byte *row = rows[y];
            for(x = 0; x < w; x++) {
                png_byte *ptr = &(row[x * 3]);
                BM_SET_RGBA(bmp, x, y, ptr[0], ptr[1], ptr[2], 0xFF);
            }
        }
    }

    goto done;
error:
    if(bmp) bm_free(bmp);
    bmp = NULL;
done:
    if (info != NULL) png_free_data(png, info, PNG_FREE_ALL, -1);
    if (png != NULL) png_destroy_read_struct(&png, NULL, NULL);
    if(rows) {
        for(y = 0; y < h; y++) {
            free(rows[y]);
        }
        free(rows);
    }
    return bmp;
}
#  endif /* USEPNG */
#  ifdef USEJPG

/*
Code to read a JPEG from an SDL_RWops.
Refer to jdatasrc.c in libjpeg's code.
See also
http://www.cs.stanford.edu/~acoates/decompressJpegFromMemory.txt
*/
#define JPEG_INPUT_BUFFER_SIZE  4096
struct rw_jpeg_src_mgr {
    struct jpeg_source_mgr pub;
    SDL_RWops *rw;
    JOCTET *buffer;
    boolean start_of_file;
};

static void rw_init_source(j_decompress_ptr cinfo) {
    struct rw_jpeg_src_mgr *src = (struct rw_jpeg_src_mgr *)cinfo->src;
    src->start_of_file = TRUE;
}

static boolean rw_fill_input_buffer(j_decompress_ptr cinfo) {
    struct rw_jpeg_src_mgr *src = (struct rw_jpeg_src_mgr *)cinfo->src;
    size_t nbytes = SDL_RWread(src->rw, src->buffer, 1, JPEG_INPUT_BUFFER_SIZE);

    if(nbytes <= 0) {
        /*if(src->start_of_file)
            ERREXIT(cinfo, JERR_INPUT_EMPTY);
        WARNMS(cinfo, JWRN_JPEG_EOF);*/
        src->buffer[0] = (JOCTET)0xFF;
        src->buffer[1] = (JOCTET)JPEG_EOI;
        nbytes = 2;
    }

    src->pub.next_input_byte = src->buffer;
    src->pub.bytes_in_buffer = nbytes;

    src->start_of_file = TRUE;
    return TRUE;
}

static void rw_skip_input_data(j_decompress_ptr cinfo, long nbytes) {
    struct rw_jpeg_src_mgr *src = (struct rw_jpeg_src_mgr *)cinfo->src;
    if(nbytes > 0) {
        while(nbytes > src->pub.bytes_in_buffer) {
            nbytes -= src->pub.bytes_in_buffer;
            (void)(*src->pub.fill_input_buffer)(cinfo);
        }
        src->pub.next_input_byte += nbytes;
        src->pub.bytes_in_buffer -= nbytes;
    }
}

static void rw_term_source(j_decompress_ptr cinfo) {
    /* Apparently nothing to do here */
}

static void rw_set_source_mgr(j_decompress_ptr cinfo, SDL_RWops *rw) {
    struct rw_jpeg_src_mgr *src;
    if(!cinfo->src) {
        cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof *src);
        src = (struct rw_jpeg_src_mgr *)cinfo->src;
        src->buffer = (JOCTET *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT, JPEG_INPUT_BUFFER_SIZE * sizeof(JOCTET));
    }

    src = (struct rw_jpeg_src_mgr *)cinfo->src;

    src->pub.init_source = rw_init_source;
    src->pub.fill_input_buffer = rw_fill_input_buffer;
    src->pub.skip_input_data = rw_skip_input_data;
    src->pub.term_source = rw_term_source;
    src->pub.resync_to_restart = jpeg_resync_to_restart;

    src->pub.bytes_in_buffer = 0;
    src->pub.next_input_byte = NULL;

    src->rw = rw;
}

static Bitmap *bm_load_jpg_rw(SDL_RWops *rw) {
    struct jpeg_decompress_struct cinfo;
    struct jpg_err_handler jerr;
    Bitmap *bmp = NULL;
    int i, j, row_stride;
    unsigned char *data;
    JSAMPROW row_pointer[1];

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpg_on_error;
    if(setjmp(jerr.jbuf)) {
        jpeg_destroy_decompress(&cinfo);
        return NULL;
    }
    jpeg_create_decompress(&cinfo);

    /* jpeg_stdio_src(&cinfo, f); */
    rw_set_source_mgr(&cinfo, rw);

    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;

    bmp = bm_create(cinfo.image_width, cinfo.image_height);
    if(!bmp) {
        return NULL;
    }
    row_stride = bmp->w * 3;

    data = malloc(row_stride);
    if(!data) {
        return NULL;
    }
    memset(data, 0x00, row_stride);
    row_pointer[0] = data;

    jpeg_start_decompress(&cinfo);

    for(j = 0; j < cinfo.output_height; j++) {
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
        for(i = 0; i < bmp->w; i++) {
            unsigned char *ptr = &(data[i * 3]);
            BM_SET_RGBA(bmp, i, j, ptr[0], ptr[1], ptr[2], 0xFF);
        }
    }
    free(data);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return bmp;
}
#  endif /* USEJPG */

#endif /* USESDL */

/* These functions are used for the palettes in my GIF and PCX support: */

static int cnt_comp_mask(const void*ap, const void*bp);

/* Variation on bm_count_colors() that builds an 8-bit palette while it is counting.
 * It returns -1 in case there are more than 256 colours in the palette, meaning the
 * image will have to be quantized first.
 * It also ignores the alpha values of the pixels.
 * It also has the side effect that the returned palette contains sorted colors, which
 * is useful for bsrch_palette_lookup().
 */
static int count_colors_build_palette(Bitmap *b, struct rgb_triplet rgb[256]) {
    int count = 1, i, c;
    int npx = b->w * b->h;
    int *sort = malloc(npx * sizeof *sort);
    memcpy(sort, b->data, npx * sizeof *sort);
    qsort(sort, npx, sizeof(int), cnt_comp_mask);
    c = sort[0] & 0x00FFFFFF;
    rgb[0].r = (c >> 16) & 0xFF;
    rgb[0].g = (c >> 8) & 0xFF;
    rgb[0].b = (c >> 0) & 0xFF;
    for(i = 1; i < npx; i++){
        c = sort[i] & 0x00FFFFFF;
        if(c != (sort[i-1]& 0x00FFFFFF)) {
            if(count == 256) {
                return -1;
            }
            rgb[count].r = (c >> 16) & 0xFF;
            rgb[count].g = (c >> 8) & 0xFF;
            rgb[count].b = (c >> 0) & 0xFF;
            count++;
        }
    }
    free(sort);
    return count;
}

/* Uses a binary search to find the index of a colour in a palette.
It (almost) goes without saying that the palette must be sorted. */
static int bsrch_palette_lookup(struct rgb_triplet rgb[], int c, int imin, int imax) {
    c &= 0x00FFFFFF; /* Ignore the alpha value */
    while(imax >= imin) {
        int imid = (imin + imax) >> 1, c2;
        assert(imid <= 255);
        c2 = (rgb[imid].r << 16) | (rgb[imid].g << 8) | rgb[imid].b;
        if(c == c2)
            return imid;
        else if(c2 < c)
            imin = imid + 1;
        else
            imax = imid - 1;
    }
    return -1;
}

/* Comparison function for sorting an array of rgb_triplets with qsort() */
static int comp_rgb(const void *ap, const void *bp) {
    const struct rgb_triplet *ta = ap, *tb = bp;
    int a = (ta->r << 16) | (ta->g << 8) | ta->b;
    int b = (tb->r << 16) | (tb->g << 8) | tb->b;
    return a - b;
}

/* GIF support
http://www.w3.org/Graphics/GIF/spec-gif89a.txt
Nelson, M.R. : "LZW Data Compression", Dr. Dobb's Journal, October 1989.
http://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art011
http://www.matthewflickinger.com/lab/whatsinagif/bits_and_bytes.asp
*/

#pragma pack(push, 1) /* Don't use any padding */
typedef struct {

    /* header */
    struct {
        char signature[3];
        char version[3];
    } header;

    enum {gif_87a, gif_89a} version;

    /* logical screen descriptor */
    struct {
        unsigned short width;
        unsigned short height;
        unsigned char fields;
        unsigned char background;
        unsigned char par; /* pixel aspect ratio */
    } lsd;

    Bitmap *bmp;

} GIF;

/* GIF Graphic Control Extension */
typedef struct {
    unsigned char block_size;
    unsigned char fields;
    unsigned short delay;
    unsigned char trans_index;
    unsigned char terminator;
} GIF_GCE;

/* GIF Image Descriptor */
typedef struct {
    unsigned char separator;
    unsigned short left;
    unsigned short top;
    unsigned short width;
    unsigned short height;
    unsigned char fields;
} GIF_ID;

/* GIF Application Extension */
typedef struct {
    unsigned char block_size;
    char app_id[8];
    char auth_code[3];
} GIF_APP_EXT;

/* GIF text extension */
typedef struct {
    unsigned char block_size;
    unsigned short grid_left;
    unsigned short grid_top;
    unsigned short grid_width;
    unsigned short grid_height;
    unsigned char text_fg;
    unsigned char text_bg;
} GIF_TXT_EXT;
#pragma pack(pop)

static int gif_read_image(BmReader rd, GIF *gif, struct rgb_triplet *ct, int sct);
static int gif_read_tbid(BmReader rd, GIF *gif, GIF_ID *gif_id, GIF_GCE *gce, struct rgb_triplet *ct, int sct);
static unsigned char *gif_data_sub_blocks(BmReader rd, int *r_tsize);
static unsigned char *lzw_decode_bytes(unsigned char *bytes, int data_len, int code_size, int *out_len);

static Bitmap *bm_load_gif_rd(BmReader rd) {
    GIF gif;

    /* From the packed fields in the logical screen descriptor */
    int gct, sgct;

    struct rgb_triplet *palette = NULL;

    unsigned char trailer;

    gif.bmp = NULL;

    /* Section 17. Header. */
    if(rd.fread(&gif.header, sizeof gif.header, 1, rd.data) != 1) {
        return NULL;
    }
    if(memcmp(gif.header.signature, "GIF", 3)){
        return NULL;
    }
    if(!memcmp(gif.header.version, "87a", 3)){
        gif.version = gif_87a;
    } else if(!memcmp(gif.header.version, "89a", 3)){
        gif.version = gif_89a;
    } else {
        return NULL;
    }

    /* Section 18. Logical Screen Descriptor. */

    /* Ugh, I once used a compiler that added a padding byte */
    assert(sizeof gif.lsd == 7);
    assert(sizeof *palette == 3);

    if(rd.fread(&gif.lsd, sizeof gif.lsd, 1, rd.data) != 1) {
        return NULL;
    }

    gct = !!(gif.lsd.fields & 0x80);
    sgct = gif.lsd.fields & 0x07;

    if(gct) {
        /* raise 2 to the power of [sgct+1] */
        sgct = 1 << (sgct + 1);
    }

    gif.bmp = bm_create(gif.lsd.width, gif.lsd.height);

    if(gct) {
        /* Section 19. Global Color Table. */
        struct rgb_triplet *bg;
        palette = calloc(sgct, sizeof *palette);

        if(rd.fread(palette, sizeof *palette, sgct, rd.data) != sgct) {
            free(palette);
            return NULL;
        }

        /* Set the Bitmap's color to the background color.*/
        bg = &palette[gif.lsd.background];
        bm_set_color_rgb(gif.bmp, bg->r, bg->g, bg->b);
        bm_clear(gif.bmp);
        bm_set_color_rgb(gif.bmp, 0, 0, 0);
        bm_set_alpha(gif.bmp, 0);

    } else {
        /* what? */
        palette = NULL;
    }

    for(;;) {
        long pos = rd.ftell(rd.data);
        if(!gif_read_image(rd, &gif, palette, sgct)) {
            rd.fseek(rd.data, pos, SEEK_SET);
            break;
        }
    }

    if(palette)
        free(palette);

    /* Section 27. Trailer. */
    if((rd.fread(&trailer, 1, 1, rd.data) != 1) || trailer != 0x3B) {
        bm_free(gif.bmp);
        return NULL;
    }

    return gif.bmp;
}

static int gif_read_extension(BmReader rd, GIF_GCE *gce) {
    unsigned char introducer, label;

    if((rd.fread(&introducer, 1, 1, rd.data) != 1) || introducer != 0x21) {
        return 0;
    }
    if(rd.fread(&label, 1, 1, rd.data) != 1) {
        return 0;
    }

    if(label == 0xF9) {
        /* 23. Graphic Control Extension. */
        if(rd.fread(gce, sizeof *gce, 1, rd.data) != 1) {
            return 0;
        }
    } else if(label == 0xFE) {
        /* Section 24. Comment Extension. */
        int len;
        gif_data_sub_blocks(rd, &len);
    } else if(label == 0x01) {
        /* Section 25. Plain Text Extension. */
        GIF_TXT_EXT te;
        int len;
        if(rd.fread(&te, sizeof te, 1, rd.data) != 1) {
            return 0;
        }
        gif_data_sub_blocks(rd, &len);
    } else if(label == 0xFF) {
        /* Section 26. Application Extension. */
        GIF_APP_EXT ae;
        int len;
        if(rd.fread(&ae, sizeof ae, 1, rd.data) != 1) {
            return 0;
        }
        gif_data_sub_blocks(rd, &len); /* Skip it */
    } else {
        return 0;
    }
    return 1;
}

/* Section 20. Image Descriptor. */
static int gif_read_image(BmReader rd, GIF *gif, struct rgb_triplet *ct, int sct) {
    GIF_GCE gce;
    GIF_ID gif_id;
    int rv = 1;

    /* Packed fields in the Image Descriptor */
    int lct, slct;

    memset(&gce, 0, sizeof gce);

    if(gif->version >= gif_89a) {
        for(;;) {
            long pos = rd.ftell(rd.data);
            if(!gif_read_extension(rd, &gce)) {
                rd.fseek(rd.data, pos, SEEK_SET);
                break;
            }
        }
    }

    if(rd.fread(&gif_id, sizeof gif_id, 1, rd.data) != 1) {
        return 0; /* no more blocks to read */
    }

    if(gif_id.separator != 0x2C) {
        return 0;
    }

    lct = !!(gif_id.fields & 0x80);
    slct = gif_id.fields & 0x07;
    if(lct) {
        /* Section 21. Local Color Table. */
        /* raise 2 to the power of [slct+1] */
        slct = 1 << (slct + 1);

        ct = calloc(slct, sizeof *ct);

        if(rd.fread(ct, sizeof *ct, slct, rd.data) != slct) {
            free(ct);
            return 0;
        }
        sct = slct;
    }

    if(!gif_read_tbid(rd, gif, &gif_id, &gce, ct, sct)) {
        rv = 0; /* what? */
    }

    if(lct) {
        free(ct);
    }

    return rv;
}

/* Section 15. Data Sub-blocks. */
static unsigned char *gif_data_sub_blocks(BmReader rd, int *r_tsize) {
    unsigned char *buffer = NULL, *pos, size;
    int tsize = 0;

    if(r_tsize)
        *r_tsize = 0;

    if(rd.fread(&size, 1, 1, rd.data) != 1) {
        return NULL;
    }
    buffer = realloc(buffer, 1);

    while(size > 0) {
        buffer = realloc(buffer, tsize + size + 1);
        pos = buffer + tsize;

        if(rd.fread(pos, sizeof *pos, size, rd.data) != size) {
            free(buffer);
            return NULL;
        }

        tsize += size;
        if(rd.fread(&size, 1, 1, rd.data) != 1) {
            free(buffer);
            return NULL;
        }
    }

    if(r_tsize)
        *r_tsize = tsize;
    buffer[tsize] = '\0';
    return buffer;
}

/* Section 22. Table Based Image Data. */
static int gif_read_tbid(BmReader rd, GIF *gif, GIF_ID *gif_id, GIF_GCE *gce, struct rgb_triplet *ct, int sct) {
    int len, rv = 1;
    unsigned char *bytes, min_code_size;

    if(rd.fread(&min_code_size, 1, 1, rd.data) != 1) {
        return 0;
    }

    bytes = gif_data_sub_blocks(rd, &len);
    if(bytes && len > 0) {
        int i, outlen, x, y;

        /* Packed fields in the Graphic Control Extension */
        int intl, dispose = 0, trans_flag = 0;

        intl = !!(gif_id->fields & 0x40); /* Interlaced? */

        if(gce->block_size) {
            /* gce->block_size will be 4 if the GCE is present, 0 otherwise */
            dispose = (gce->fields >> 2) & 0x07;
            trans_flag = gce->fields & 0x01;
            if(trans_flag) {
                /* Mmmm, my bitmap module won't be able to handle
                    situations where different image blocks in the
                    GIF has different transparent colors */
                struct rgb_triplet *bg = &ct[gce->trans_index];
                bm_set_color_rgb(gif->bmp, bg->r, bg->g, bg->b);
            }
        }

        if(gif_id->top + gif_id->height > gif->bmp->h ||
            gif_id->left + gif_id->width > gif->bmp->w) {
            /* This image descriptor doesn't fall within the bounds of the image */
            return 0;
        }

        if(dispose == 2) {
            /* Restore the background color */
            for(y = 0; y < gif_id->height; y++) {
                for(x = 0; x < gif_id->width; x++) {
                    bm_set(gif->bmp, x + gif_id->left, y + gif_id->top, gif->bmp->color);
                }
            }
        } else if(dispose != 3) {
            /* dispose = 0 or 1; if dispose is 3, we leave ignore the new image */
            unsigned char *decoded = lzw_decode_bytes(bytes, len, min_code_size, &outlen);
            if(decoded) {
                if(outlen != gif_id->width * gif_id->height) {
                    /* Shouldn't happen unless the file is corrupt */
                    rv = 0;
                } else {
                    /* Vars for interlacing: */
                    int grp = 1, /* Group we're in */
                        inty = 0, /* Y we're currently at */
                        inti = 8, /* amount by which we should increment inty */
                        truey; /* True Y, taking interlacing and the image descriptor into account */
                    for(i = 0, y = 0; y < gif_id->height && rv; y++) {
                        /* Appendix E. Interlaced Images. */
                        if(intl) {
                            truey = inty + gif_id->top;
                            inty += inti;
                            if(inty >= gif_id->height) {
                                switch(++grp) {
                                    case 2: inti = 8; inty = 4; break;
                                    case 3: inti = 4; inty = 2; break;
                                    case 4: inti = 2; inty = 1;break;
                                }
                            }
                        } else {
                            truey = y + gif_id->top;
                        }
                        assert(truey >= 0 && truey < gif->bmp->h);
                        for(x = 0; x < gif_id->width && rv; x++, i++) {
                            int c = decoded[i];
                            if(c < sct) {
                                struct rgb_triplet *rgb = &ct[c];
                                assert(x + gif_id->left >= 0 && x + gif_id->left < gif->bmp->w);
                                if(trans_flag && c == gce->trans_index) {
                                    bm_set_rgb_a(gif->bmp, x + gif_id->left, truey, rgb->r, rgb->g, rgb->b, 0x00);
                                } else {
                                    bm_set_rgb(gif->bmp, x + gif_id->left, truey, rgb->r, rgb->g, rgb->b);
                                }
                            } else {
                                /* Decode error */
                                rv = 0;
                            }
                        }
                    }
                }
                free(decoded);
            }
        }
        free(bytes);
    }
    return rv;
}

typedef struct {
    int prev;
    int code;
} gif_dict;

static int lzw_read_code(unsigned char bytes[], int bits, int *pos) {
    int i, bi, code = 0;
    assert(pos);
    for(i = *pos, bi=1; i < *pos + bits; i++, bi <<=1) {
        int byte = i >> 3;
        int bit = i & 0x07;
        if(bytes[byte] & (1 << bit))
            code |= bi;
    }
    *pos = i;
    return code;
}

static unsigned char *lzw_decode_bytes(unsigned char *bytes, int data_len, int code_size, int *out_len) {
    unsigned char *out = NULL;
    int out_size = 32;
    int outp = 0;

    int base_size = code_size;

    int pos = 0, code, old = -1;

    /* Clear and end of stream codes */
    int clr = 1 << code_size;
    int end = clr + 1;

    /* Dictionary */
    int di, dict_size = 1 << (code_size + 1);
    gif_dict *dict = realloc(NULL, dict_size * sizeof *dict);

    /* Stack so we don't need to recurse down the dictionary */
    int stack_size = 2;
    unsigned char *stack = realloc(NULL, stack_size);
    int sp = 0;
    int sym = -1, ptr;

    *out_len = 0;
    out = realloc(NULL, out_size);

    /* Initialize the dictionary */
    for(di = 0; di < dict_size; di++) {
        dict[di].prev = -1;
        dict[di].code = di;
    }
    di = end + 1;

    code = lzw_read_code(bytes, code_size + 1, &pos);
    while((pos >> 3) <= data_len + 1) {
        if(code == clr) {
            code_size = base_size;
            dict_size = 1 << (code_size + 1);
            di = end + 1;
            code = lzw_read_code(bytes, code_size + 1, &pos);
            old = -1;
            continue;
        } else if(code == end) {
            break;
        }

        if(code > di) {
            /* Shouldn't happen, unless file corrupted */
            free(out);
            return NULL;
        }

        if(code == di) {
            /* Code is not in the table */
            ptr = old;
            stack[sp++] = sym;
        } else {
            /* Code is in the table */
            ptr = code;
        }

        /* Walk down the dictionary and push the codes onto a stack */
        while(ptr >= 0) {
            stack[sp++] = dict[ptr].code;
            if(sp == stack_size) {
                stack_size <<= 1;
                stack = realloc(stack, stack_size);
            }
            ptr = dict[ptr].prev;
        }
        sym = stack[sp-1];

        /* Output the decoded bytes */
        while(sp > 0) {
            out[outp++] = stack[--sp];
            if(outp == out_size) {
                out_size <<= 1;
                out = realloc(out, out_size);
            }
        }

        /* update the dictionary */
        if(old >= 0) {
            if(di < dict_size) {
                dict[di].prev = old;
                dict[di].code = sym;
                di++;
            }
            /* Resize the dictionary? */
            if(di == dict_size && code_size < 11) {
                code_size++;
                dict_size = 1 << (code_size + 1);
                dict = realloc(dict, dict_size * sizeof *dict);
            }
        }

        old = code;
        code = lzw_read_code(bytes, code_size + 1, &pos);
    }
    free(stack);
    free(dict);

    *out_len = outp;
    return out;
}

static void lzw_emit_code(unsigned char **buffer, int *buf_size, int *pos, int c, int bits) {
    int i, m;
    for(i = *pos, m = 1; i < *pos + bits; i++, m <<= 1) {
        int byte = i >> 3;
        int bit = i & 0x07;
        if(!bit) {
            if(byte == *buf_size) {
                *buf_size <<= 1;
                *buffer = realloc(*buffer, *buf_size);
            }
            (*buffer)[byte] = 0x00;
        }
        if(c & m)
            (*buffer)[byte] |= (1 << bit);
    }
    *pos += bits;
}

static unsigned char *lzw_encode_bytes(unsigned char *bytes, int data_len, int code_size, int *out_len) {
    int base_size = code_size;

    /* Clear and end of stream codes */
    int clr = 1 << code_size;
    int end = clr + 1;

    /* dictionary */
    int i, di, dict_size = 1 << (code_size + 1);
    gif_dict *dict = realloc(NULL, dict_size * sizeof *dict);

    int buf_size = 4;
    int pos = 0;
    unsigned char *buffer = realloc(NULL, buf_size);

    int ii, string, prev, tlen;

    *out_len = 0;

    /* Initialize the dictionary */
    for(di = 0; di < dict_size; di++) {
        dict[di].prev = -1;
        dict[di].code = di;
    }
    di = end+1;

    dict[clr].prev = -1;
    dict[clr].code = -1;
    dict[end].prev = -1;
    dict[end].code = -1;

    string = -1;
    prev = clr;

    lzw_emit_code(&buffer, &buf_size, &pos, clr, code_size + 1);

    for(ii = 0; ii < data_len; ii++) {
        int character, res;
reread:
        character = bytes[ii];

        /* Find it in the dictionary; If the entry is in the dict, it can't be
        before dict[string], therefore we can eliminate the first couple of entries. */
        for(res = -1, i = string>0?string:0; i < di; i++) {
            if(dict[i].prev == string && dict[i].code == character) {
                res = i;
                break;
            }
        }

        if(res >= 0) {
            /* Found */
            string = res;
            prev = res;
        } else {
            /* Not found */
            lzw_emit_code(&buffer, &buf_size, &pos, prev, code_size + 1);

            /* update the dictionary */
            if(di == dict_size) {
                /* Resize the dictionary */
                if(code_size < 11) {
                    code_size++;
                    dict_size = 1 << (code_size + 1);
                    dict = realloc(dict, dict_size * sizeof *dict);
                } else {
                    /* lzw_emit_code a clear code */
                    lzw_emit_code(&buffer, &buf_size, &pos, clr,code_size + 1);
                    code_size = base_size;
                    dict_size = 1 << (code_size + 1);
                    di = end + 1;
                    string = -1;
                    prev = clr;
                    goto reread;
                }
            }

            dict[di].prev = string;
            dict[di].code = character;
            di++;

            string = character;
            prev = character;
        }
    }

    lzw_emit_code(&buffer, &buf_size, &pos, prev,code_size + 1);
    lzw_emit_code(&buffer, &buf_size, &pos, end,code_size + 1);

    /* Total length */
    tlen = (pos >> 3);
    if(pos & 0x07) tlen++;
    *out_len = tlen;

    return buffer;
}

static int bm_save_gif(Bitmap *b, const char *fname) {
    GIF gif;
    GIF_GCE gce;
    GIF_ID gif_id;
    int nc, sgct, bg;
    struct rgb_triplet gct[256];
    Bitmap *bo = b;
    unsigned char code_size = 0x08;

    /* For encoding */
    int len, x, y, p;
    unsigned char *bytes, *pixels;

    FILE *f = fopen(fname, "wb");
    if(!f) {
        return 0;
    }

    memcpy(gif.header.signature, "GIF", 3);
    memcpy(gif.header.version, "89a", 3);
    gif.version = gif_89a;
    gif.lsd.width = b->w;
    gif.lsd.height = b->h;
    gif.lsd.background = 0;
    gif.lsd.par = 0;

    /* Using global color table, color resolution = 8-bits */
    gif.lsd.fields = 0xF0;

    nc = count_colors_build_palette(b, gct);
    if(nc < 0) {
        int palette[256], q;

        /* Too many colors */
        sgct = 256;
        gif.lsd.fields |= 0x07;

        /* color quantization - see bm_save_pcx() */
        nc = 0;
        for(nc = 0; nc < 256; nc++) {
            int c = bm_get(b, rand()%b->w, rand()%b->h);
            gct[nc].r = (c >> 16) & 0xFF;
            gct[nc].g = (c >> 8) & 0xFF;
            gct[nc].b = (c >> 0) & 0xFF;
        }
        qsort(gct, nc, sizeof gct[0], comp_rgb);
        for(q = 0; q < nc; q++) {
            palette[q] = (gct[q].r << 16) | (gct[q].g << 8) | gct[q].b;
        }
        /* Copy the image and dither it to match the palette */
        b = bm_copy(b);
        bm_reduce_palette(b, palette, nc);
    } else {
        if(nc > 128) {
            sgct = 256;
            gif.lsd.fields |= 0x07;
        } else if(nc > 64) {
            sgct = 128;
            gif.lsd.fields |= 0x06;
            code_size = 7;
        } else if(nc > 32) {
            sgct = 64;
            gif.lsd.fields |= 0x05;
            code_size = 6;
        } else if(nc > 16) {
            sgct = 32;
            gif.lsd.fields |= 0x04;
            code_size = 5;
        } else if(nc > 8) {
            sgct = 16;
            gif.lsd.fields |= 0x03;
            code_size = 4;
        } else {
            sgct = 8;
            gif.lsd.fields |= 0x02;
            code_size = 3;
        }
    }

    /* See if we can find the background color in the palette */
    bg = b->color & 0x00FFFFFF;
    bg = bsrch_palette_lookup(gct, bg, 0, nc - 1);
    if(bg >= 0) {
        gif.lsd.background = bg;
    }

    /* Map the pixels in the image to their palette indices */
    pixels = malloc(b->w * b->h);
    for(y = 0, p = 0; y < b->h; y++) {
        for(x = 0; x < b->w; x++) {
            int i, c = bm_get(b, x, y);
            i = bsrch_palette_lookup(gct, c, 0, nc - 1);
            /* At this point in time, the color MUST be in the palette */
            assert(i >= 0);
            assert(i < sgct);
            pixels[p++] = i;
        }
    }
    assert(p == b->w * b->h);

    if(fwrite(&gif.header, sizeof gif.header, 1, f) != 1 ||
        fwrite(&gif.lsd, sizeof gif.lsd, 1, f) != 1 ||
        fwrite(gct, sizeof *gct, sgct, f) != sgct) {
        fclose(f);
        return 0;
    }

    /* Nothing of use here */
    gce.block_size = 4;
    gce.fields = 0;
    gce.delay = 0;
    if(bg >= 0) {
        gce.fields |= 0x01;
        gce.trans_index = bg;
    } else {
        gce.trans_index = 0;
    }
    gce.terminator = 0x00;

    fputc(0x21, f);
    fputc(0xF9, f);
    if(fwrite(&gce, sizeof gce, 1, f) != 1) {
        fclose(f);
        return 0;
    }

    gif_id.separator = 0x2C;
    gif_id.left = 0x00;
    gif_id.top = 0x00;
    gif_id.width = b->w;
    gif_id.height = b->h;
    /* Not using local color table or interlacing */
    gif_id.fields = 0;
    if(fwrite(&gif_id, sizeof gif_id, 1, f) != 1) {
        fclose(f);
        return 0;
    }

    fputc(code_size, f);

    /* Perform the LZW compression */
    bytes = lzw_encode_bytes(pixels, b->w * b->h, code_size, &len);
    free(pixels);

    /* Write out the data sub-blocks */
    for(p = 0; p < len; p++) {
        if(p % 0xFF == 0) {
            /* beginning of a new block; lzw_emit_code the length byte */
            if(len - p >= 0xFF) {
                fputc(0xFF, f);
            } else {
                fputc(len - p, f);
            }
        }
        fputc(bytes[p], f);
    }
    free(bytes);

    fputc(0x00, f); /* terminating block */

    fputc(0x3B, f); /* trailer byte */

    if(bo != b)
        bm_free(b);

    fclose(f);
    return 1;
}

/* PCX support
http://web.archive.org/web/20100206055706/http://www.qzx.com/pc-gpe/pcx.txt
http://www.shikadi.net/moddingwiki/PCX_Format
*/
#pragma pack(push, 1)
struct pcx_header {
    char manuf;
    char version;
    char encoding;
    char bpp;
    unsigned short xmin, ymin, xmax, ymax;
    unsigned short vert_dpi, hori_dpi;

    union {
        unsigned char bytes[48];
        struct rgb_triplet rgb[16];
    } palette;

    char reserved;
    char planes;
    unsigned short bytes_per_line;
    unsigned short paltype;
    unsigned short hscrsize, vscrsize;
    char pad[54];
};
#pragma pack(pop)

static Bitmap *bm_load_pcx_rd(BmReader rd) {
    struct pcx_header hdr;
    Bitmap *b = NULL;
    int y;

    struct rgb_triplet rgb[256];

    if(rd.fread(&hdr, sizeof hdr, 1, rd.data) != 1) {
        return NULL;
    }
    if(hdr.manuf != 0x0A) {
        return NULL;
    }

    if(hdr.version != 5 || hdr.encoding != 1 || hdr.bpp != 8 || (hdr.planes != 1 && hdr.planes != 3)) {
        /* We might want to support these PCX types at a later stage... */
        return NULL;
    }

    if(hdr.planes == 1) {
        long pos = rd.ftell(rd.data);
        char pbyte;

        rd.fseek(rd.data, -769, SEEK_END);
        if(rd.fread(&pbyte, sizeof pbyte, 1, rd.data) != 1) {
            return NULL;
        }
        if(pbyte != 12) {
            return NULL;
        }
        if(rd.fread(&rgb, sizeof rgb[0], 256, rd.data) != 256) {
            return NULL;
        }

        rd.fseek(rd.data, pos, SEEK_SET);
    }

    b = bm_create(hdr.xmax - hdr.xmin + 1, hdr.ymax - hdr.ymin + 1);

    for(y = 0; y < b->h; y++) {
        int p;
        for(p = 0; p < hdr.planes; p++) {
            int x = 0;
            while(x < b->w) {
                int cnt = 1;
                unsigned char i;
                if(rd.fread(&i, sizeof i, 1, rd.data) != 1)
                    goto read_error;

                if((i & 0xC0) == 0xC0) {
                    cnt = i & 0x3F;
                    if(rd.fread(&i, sizeof i, 1, rd.data) != 1)
                        goto read_error;
                }
                if(hdr.planes == 1) {
                    int c = (rgb[i].r << 16) | (rgb[i].g << 8) | rgb[i].b;
                    while(cnt--) {
                        bm_set(b, x++, y, c);
                    }
                } else {
                    while(cnt--) {
                        int c = bm_get(b, x, y);
                        switch(p) {
                        case 0: c |= (i << 16); break;
                        case 1: c |= (i << 8); break;
                        case 2: c |= (i << 0); break;
                        }
                        bm_set(b, x++, y, c);
                    }
                }
            }
        }
    }

    return b;
read_error:
    bm_free(b);
    return NULL;
}

static int bm_save_pcx(Bitmap *b, const char *fname) {
    FILE *f;
    struct rgb_triplet rgb[256];
    int ncolors, x, y, rv = 1;
    struct pcx_header hdr;
    Bitmap *bo = b;

    if(!b)
        return 0;

    f = fopen(fname, "wb");
    if(!f)
        return 0;

    memset(&hdr, 0, sizeof hdr);

    hdr.manuf = 0x0A;
    hdr.version = 5;
    hdr.encoding = 1;
    hdr.bpp = 8;

    hdr.xmin = 0;
    hdr.ymin = 0;
    hdr.xmax = b->w - 1;
    hdr.ymax = b->h - 1;

    hdr.vert_dpi = b->h;
    hdr.hori_dpi = b->w;

    hdr.reserved = 0;
    hdr.planes = 1;
    hdr.bytes_per_line = hdr.xmax - hdr.xmin + 1;
    hdr.paltype = 1;
    hdr.hscrsize = 0;
    hdr.vscrsize = 0;

    memset(&rgb, 0, sizeof rgb);

    if(fwrite(&hdr, sizeof hdr, 1, f) != 1) {
        fclose(f);
        return 0;
    }

    ncolors = count_colors_build_palette(b, rgb);
    if(ncolors < 0) {
        /* This is my poor man's color quantization hack:
            Sample random pixels and generate a palette from them.
            A better solution would be to use some clustering, but
            I don't have the stomach for that now. */
        int palette[256], q;
        ncolors = 0;
        for(ncolors = 0; ncolors < 256; ncolors++) {
            unsigned int c = bm_get(b, rand()%b->w, rand()%b->h);
            rgb[ncolors].r = (c >> 16) & 0xFF;
            rgb[ncolors].g = (c >> 8) & 0xFF;
            rgb[ncolors].b = (c >> 0) & 0xFF;
        }
        qsort(rgb, ncolors, sizeof rgb[0], comp_rgb);
        for(q = 0; q < ncolors; q++) {
            palette[q] = (rgb[q].r << 16) | (rgb[q].g << 8) | rgb[q].b;
        }
        b = bm_copy(b);
        /* Copy the image and dither it to match the palette */
        bm_reduce_palette(b, palette, ncolors);
    }

    for(y = 0; y < b->h; y++) {
        x = 0;
        while(x < b->w) {
            int i, cnt = 1;
            unsigned int c = bm_get(b, x++, y);
            while(x < b->w && cnt < 63) {
                unsigned int n = bm_get(b, x, y);
                if(c != n)
                    break;
                x++;
                cnt++;
            }
            i = bsrch_palette_lookup(rgb, c, 0, ncolors - 1);
            assert(i >= 0); /* At this point in time, the color MUST be in the palette */
            if(cnt == 1 && i < 192) {
                fputc(i, f);
            } else {
                fputc(0xC0 | cnt, f);
                fputc(i, f);
            }
        }
    }

    fputc(12, f);
    if(fwrite(rgb, sizeof rgb[0], 256, f) != 256) {
        rv = 0;
    }

    if(b != bo) {
        bm_free(b);
    }

    fclose(f);
    return rv;
}

Bitmap *bm_copy(Bitmap *b) {
    Bitmap *out = bm_create(b->w, b->h);
    memcpy(out->data, b->data, BM_BLOB_SIZE(b));

    out->color = b->color;

    /* Caveat: The input bitmap is technically the owner
    of its own font, so we can't just copy the pointer
    */
    /* out->font = b->font */
    out->font = NULL;

    memcpy(&out->clip, &b->clip, sizeof b->clip);

    return out;
}

void bm_free(Bitmap *b) {
    if(!b) return;
    if(b->data) free(b->data);
    if(b->font && b->font->dtor)
        b->font->dtor(b->font);
    free(b);
}

Bitmap *bm_bind(int w, int h, unsigned char *data) {
    Bitmap *b = malloc(sizeof *b);

    b->w = w;
    b->h = h;

    b->clip.x0 = 0;
    b->clip.y0 = 0;
    b->clip.x1 = w;
    b->clip.y1 = h;

    b->data = data;

    b->font = NULL;
#ifndef NO_FONTS
    bm_std_font(b, BM_FONT_NORMAL);
#endif

    bm_set_color(b, 0xFFFFFFFF);

    return b;
}

void bm_rebind(Bitmap *b, unsigned char *data) {
    b->data = data;
}

void bm_unbind(Bitmap *b) {
    if(!b) return;
    if(b->font && b->font->dtor)
        b->font->dtor(b->font);
    free(b);
}

void bm_flip_vertical(Bitmap *b) {
    int y;
    size_t s = BM_ROW_SIZE(b);
    unsigned char *trow = malloc(s);
    for(y = 0; y < b->h/2; y++) {
        unsigned char *row1 = &b->data[y * s];
        unsigned char *row2 = &b->data[(b->h - y - 1) * s];
        memcpy(trow, row1, s);
        memcpy(row1, row2, s);
        memcpy(row2, trow, s);
    }
    free(trow);
}

unsigned int bm_get(Bitmap *b, int x, int y) {
    unsigned int *p;
    assert(x >= 0 && x < b->w && y >= 0 && y < b->h);
    p = (unsigned int*)(b->data + y * BM_ROW_SIZE(b) + x * BM_BPP);
    return *p;
}

void bm_set(Bitmap *b, int x, int y, unsigned int c) {
    unsigned int *p;
    assert(x >= 0 && x < b->w && y >= 0 && y < b->h);
    p = (unsigned int*)(b->data + y * BM_ROW_SIZE(b) + x * BM_BPP);
    *p = c;
}

void bm_set_rgb(Bitmap *b, int x, int y, unsigned char R, unsigned char G, unsigned char B) {
    assert(x >= 0 && x < b->w && y >= 0 && y < b->h);
    BM_SET_RGBA(b, x, y, R, G, B, (b->color >> 24));
}

void bm_set_rgb_a(Bitmap *b, int x, int y, unsigned char R, unsigned char G, unsigned char B, unsigned char A) {
    assert(x >= 0 && x < b->w && y >= 0 && y < b->h);
    BM_SET_RGBA(b, x, y, R, G, B, A);
}

unsigned char bm_getr(Bitmap *b, int x, int y) {
    return BM_GETR(b,x,y);
}

unsigned char bm_getg(Bitmap *b, int x, int y) {
    return BM_GETG(b,x,y);
}

unsigned char bm_getb(Bitmap *b, int x, int y) {
    return BM_GETB(b,x,y);
}

unsigned char bm_geta(Bitmap *b, int x, int y) {
    return BM_GETA(b,x,y);
}

Bitmap *bm_fromXbm(int w, int h, unsigned char *data) {
    int x,y;

    Bitmap *bmp = bm_create(w, h);

    int byte = 0;
    for(y = 0; y < h; y++)
        for(x = 0; x < w;) {
            int i, b;
            b = data[byte++];
            for(i = 0; i < 8 && x < w; i++) {
                unsigned char c = (b & (1 << i))?0x00:0xFF;
                BM_SET_RGBA(bmp, x++, y, c, c, c, c);
            }
        }
    return bmp;
}

void bm_clip(Bitmap *b, int x0, int y0, int x1, int y1) {
    if(x0 > x1) {
        int t = x1;
        x1 = x0;
        x0 = t;
    }
    if(y0 > y1) {
        int t = y1;
        y1 = y0;
        y0 = t;
    }
    if(x0 < 0) x0 = 0;
    if(x1 > b->w) x1 = b->w;
    if(y0 < 0) y0 = 0;
    if(y1 > b->h) y1 = b->h;

    b->clip.x0 = x0;
    b->clip.y0 = y0;
    b->clip.x1 = x1;
    b->clip.y1 = y1;
}

void bm_unclip(Bitmap *b) {
    b->clip.x0 = 0;
    b->clip.y0 = 0;
    b->clip.x1 = b->w;
    b->clip.y1 = b->h;
}

void bm_blit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h) {
    int x,y, i, j;

    if(sx < 0) {
        int delta = -sx;
        sx = 0;
        dx += delta;
        w -= delta;
    }

    if(dx < dst->clip.x0) {
        int delta = dst->clip.x0 - dx;
        sx += delta;
        w -= delta;
        dx = dst->clip.x0;
    }

    if(sx + w > src->w) {
        int delta = sx + w - src->w;
        w -= delta;
    }

    if(dx + w > dst->clip.x1) {
        int delta = dx + w - dst->clip.x1;
        w -= delta;
    }

    if(sy < 0) {
        int delta = -sy;
        sy = 0;
        dy += delta;
        h -= delta;
    }

    if(dy < dst->clip.y0) {
        int delta = dst->clip.y0 - dy;
        sy += delta;
        h -= delta;
        dy = dst->clip.y0;
    }

    if(sy + h > src->h) {
        int delta = sy + h - src->h;
        h -= delta;
    }

    if(dy + h > dst->clip.y1) {
        int delta = dy + h - dst->clip.y1;
        h -= delta;
    }

    if(w <= 0 || h <= 0)
        return;
    if(dx >= dst->clip.x1 || dx + w < dst->clip.x0)
        return;
    if(dy >= dst->clip.y1 || dy + h < dst->clip.y0)
        return;
    if(sx >= src->w || sx + w < 0)
        return;
    if(sy >= src->h || sy + h < 0)
        return;

    if(sx + w > src->w) {
        int delta = sx + w - src->w;
        w -= delta;
    }

    if(sy + h > src->h) {
        int delta = sy + h - src->h;
        h -= delta;
    }

    assert(dx >= 0 && dx + w <= dst->clip.x1);
    assert(dy >= 0 && dy + h <= dst->clip.y1);
    assert(sx >= 0 && sx + w <= src->w);
    assert(sy >= 0 && sy + h <= src->h);

    j = sy;
    for(y = dy; y < dy + h; y++) {
        i = sx;
        for(x = dx; x < dx + w; x++) {
            int c = BM_GET(src, i, j);
            BM_SET(dst, x, y, c);
            i++;
        }
        j++;
    }
}

void bm_maskedblit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h) {
    int x,y, i, j;

    if(sx < 0) {
        int delta = -sx;
        sx = 0;
        dx += delta;
        w -= delta;
    }

    if(dx < dst->clip.x0) {
        int delta = dst->clip.x0 - dx;
        sx += delta;
        w -= delta;
        dx = dst->clip.x0;
    }

    if(sx + w > src->w) {
        int delta = sx + w - src->w;
        w -= delta;
    }

    if(dx + w > dst->clip.x1) {
        int delta = dx + w - dst->clip.x1;
        w -= delta;
    }

    if(sy < 0) {
        int delta = -sy;
        sy = 0;
        dy += delta;
        h -= delta;
    }

    if(dy < dst->clip.y0) {
        int delta = dst->clip.y0 - dy;
        sy += delta;
        h -= delta;
        dy = dst->clip.y0;
    }

    if(sy + h > src->h) {
        int delta = sy + h - src->h;
        h -= delta;
    }

    if(dy + h > dst->clip.y1) {
        int delta = dy + h - dst->clip.y1;
        h -= delta;
    }

    if(w <= 0 || h <= 0)
        return;
    if(dx >= dst->clip.x1 || dx + w < dst->clip.x0)
        return;
    if(dy >= dst->clip.y1 || dy + h < dst->clip.y0)
        return;
    if(sx >= src->w || sx + w < 0)
        return;
    if(sy >= src->h || sy + h < 0)
        return;

    if(sx + w > src->w) {
        int delta = sx + w - src->w;
        w -= delta;
    }

    if(sy + h > src->h) {
        int delta = sy + h - src->h;
        h -= delta;
    }

    assert(dx >= 0 && dx + w <= dst->clip.x1);
    assert(dy >= 0 && dy + h <= dst->clip.y1);
    assert(sx >= 0 && sx + w <= src->w);
    assert(sy >= 0 && sy + h <= src->h);

    j = sy;
    for(y = dy; y < dy + h; y++) {
        i = sx;
        for(x = dx; x < dx + w; x++) {
            int c = BM_GET(src, i, j) & 0xFFFFFF;
            if(c != (src->color & 0xFFFFFF))
                BM_SET(dst, x, y, c);
            i++;
        }
        j++;
    }
}

void bm_blit_ex(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, int mask) {
    int x, y, ssx;
    int ynum = 0;
    int xnum = 0;
    unsigned int maskc = bm_get_color(src) & 0xFFFFFF;
    /*
    Uses Bresenham's algoritm to implement a simple scaling while blitting.
    See the article "Scaling Bitmaps with Bresenham" by Tim Kientzle in the
    October 1995 issue of C/C++ Users Journal

    Or see these links:
        http://www.drdobbs.com/image-scaling-with-bresenham/184405045
        http://www.compuphase.com/graphic/scale.htm
    */

    if(sw == dw && sh == dh) {
        /* Special cases, no scaling */
        if(mask) {
            bm_maskedblit(dst, dx, dy, src, sx, sy, dw, dh);
        } else {
            bm_blit(dst, dx, dy, src, sx, sy, dw, dh);
        }
        return;
    }

    if(sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
        return;

    /* Clip on the Y */
    y = dy;
    while(y < dst->clip.y0 || sy < 0) {
        ynum += sh;
        while(ynum > dh) {
            ynum -= dh;
            sy++;
        }
        y++;
    }

    if(dy >= dst->clip.y1 || dy + dh < dst->clip.y0)
        return;

    /* Clip on the X */
    x = dx;
    while(x < dst->clip.x0 || sx < 0) {
        xnum += sw;
        while(xnum > dw) {
            xnum -= dw;
            sx++;
            sw--;
        }
        x++;
    }
    dw -= (x - dx);
    dx = x;

    if(dx >= dst->clip.x1 || dx + dw < dst->clip.x0)
        return;

    ssx = sx; /* Save sx for the next row */
    for(; y < dy + dh; y++){
        if(sy >= src->h || y >= dst->clip.y1)
            break;
        xnum = 0;
        sx = ssx;

        assert(y >= dst->clip.y0 && sy >= 0);
        for(x = dx; x < dx + dw; x++) {
            int c;
            if(sx >= src->w || x >= dst->clip.x1)
                break;
            assert(x >= dst->clip.x0 && sx >= 0);

            c = BM_GET(src, sx, sy) & 0xFFFFFF;
            if(!mask || c != maskc)
                BM_SET(dst, x, y, c);

            xnum += sw;
            while(xnum > dw) {
                xnum -= dw;
                sx++;
            }
        }
        ynum += sh;
        while(ynum > dh) {
            ynum -= dh;
            sy++;
        }
    }
}

/*
Works the same as bm_blit_ex(), but calls the callback for each pixel
typedef int (*bm_blit_fun)(Bitmap *dst, int dx, int dy, int sx, int sy, void *data);
*/
void bm_blit_ex_fun(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, bm_blit_fun fun, void *data){
    int x, y, ssx;
    int ynum = 0;
    int xnum = 0;
    unsigned int maskc = bm_get_color(src) & 0xFFFFFF;

    if(!fun || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
        return;

    /* Clip on the Y */
    y = dy;
    while(y < dst->clip.y0 || sy < 0) {
        ynum += sh;
        while(ynum > dh) {
            ynum -= dh;
            sy++;
        }
        y++;
    }

    if(dy >= dst->clip.y1 || dy + dh < dst->clip.y0)
        return;

    /* Clip on the X */
    x = dx;
    while(x < dst->clip.x0 || sx < 0) {
        xnum += sw;
        while(xnum > dw) {
            xnum -= dw;
            sx++;
            sw--;
        }
        x++;
    }
    dw -= (x - dx);
    dx = x;

    if(dx >= dst->clip.x1 || dx + dw < dst->clip.x0)
        return;

    ssx = sx; /* Save sx for the next row */
    for(; y < dy + dh; y++){
        if(sy >= src->h || y >= dst->clip.y1)
            break;
        xnum = 0;
        sx = ssx;

        assert(y >= dst->clip.y0 && sy >= 0);
        for(x = dx; x < dx + dw; x++) {
            if(sx >= src->w || x >= dst->clip.x1)
                break;
            if(!fun(dst, x, y, src, sx, sy, maskc, data))
                return;
            xnum += sw;
            while(xnum > dw) {
                xnum -= dw;
                sx++;
            }
        }
        ynum += sh;
        while(ynum > dh) {
            ynum -= dh;
            sy++;
        }
    }
}

void bm_smooth(Bitmap *b) {
    Bitmap *tmp = bm_create(b->w, b->h);
    unsigned char *t = b->data;
    int x, y;

    /* http://prideout.net/archive/bloom/ */
    int kernel[] = {1,4,6,4,1};

    assert(b->clip.y0 < b->clip.y1);
    assert(b->clip.x0 < b->clip.x1);

    for(y = 0; y < b->h; y++)
        for(x = 0; x < b->w; x++) {
            int p, k, c = 0;
            float R = 0, G = 0, B = 0, A = 0;
            for(p = x-2, k = 0; p < x+2; p++, k++) {
                if(p < 0 || p >= b->w)
                    continue;
                R += kernel[k] * BM_GETR(b,p,y);
                G += kernel[k] * BM_GETG(b,p,y);
                B += kernel[k] * BM_GETB(b,p,y);
                A += kernel[k] * BM_GETA(b,p,y);
                c += kernel[k];
            }
            BM_SET_RGBA(tmp, x, y, R/c, G/c, B/c, A/c);
        }

    for(y = 0; y < b->h; y++)
        for(x = 0; x < b->w; x++) {
            int p, k, c = 0;
            float R = 0, G = 0, B = 0, A = 0;
            for(p = y-2, k = 0; p < y+2; p++, k++) {
                if(p < 0 || p >= b->h)
                    continue;
                R += kernel[k] * BM_GETR(tmp,x,p);
                G += kernel[k] * BM_GETG(tmp,x,p);
                B += kernel[k] * BM_GETB(tmp,x,p);
                A += kernel[k] * BM_GETA(tmp,x,p);
                c += kernel[k];
            }
            BM_SET_RGBA(tmp, x, y, R/c, G/c, B/c, A/c);
        }

    b->data = tmp->data;
    tmp->data = t;
    bm_free(tmp);
}

void bm_apply_kernel(Bitmap *b, int dim, float kernel[]) {
    Bitmap *tmp = bm_create(b->w, b->h);
    unsigned char *t = b->data;
    int x, y;
    int kf = dim >> 1;

    assert(b->clip.y0 < b->clip.y1);
    assert(b->clip.x0 < b->clip.x1);

    for(y = 0; y < b->h; y++) {
        for(x = 0; x < b->w; x++) {
            int p, q, u, v;
            float R = 0, G = 0, B = 0, A = 0, c = 0;
            for(p = x-kf, u = 0; p <= x+kf; p++, u++) {
                if(p < 0 || p >= b->w)
                    continue;
                for(q = y-kf, v = 0; q <= y+kf; q++, v++) {
                    if(q < 0 || q >= b->h)
                        continue;
                    R += kernel[u + v * dim] * BM_GETR(b,p,q);
                    G += kernel[u + v * dim] * BM_GETG(b,p,q);
                    B += kernel[u + v * dim] * BM_GETB(b,p,q);
                    A += kernel[u + v * dim] * BM_GETA(b,p,q);
                    c += kernel[u + v * dim];
                }
            }
            R /= c; if(R > 255) R = 255;if(R < 0) R = 0;
            G /= c; if(G > 255) G = 255;if(G < 0) G = 0;
            B /= c; if(B > 255) B = 255;if(B < 0) B = 0;
            A /= c; if(A > 255) A = 255;if(A < 0) A = 0;
            BM_SET_RGBA(tmp, x, y, R, G, B, A);
        }
    }

    b->data = tmp->data;
    tmp->data = t;
    bm_free(tmp);
}

void bm_swap_colour(Bitmap *b, unsigned char sR, unsigned char sG, unsigned char sB, unsigned char dR, unsigned char dG, unsigned char dB) {
    /* Why does this function exist again? */
    int x,y;
    for(y = 0; y < b->h; y++)
        for(x = 0; x < b->w; x++) {
            if(BM_GETR(b,x,y) == sR && BM_GETG(b,x,y) == sG && BM_GETB(b,x,y) == sB) {
                int a = BM_GETA(b, x, y);
                BM_SET_RGBA(b, x, y, dR, dG, dB, a);
            }
        }
}

/*
Image scaling functions:
 - bm_resample() : Uses the nearest neighbour
 - bm_resample_blin() : Uses bilinear interpolation.
 - bm_resample_bcub() : Uses bicubic interpolation.
Bilinear Interpolation is better suited for making an image larger.
Bicubic Interpolation is better suited for making an image smaller.
http://blog.codinghorror.com/better-image-resizing/
*/
Bitmap *bm_resample(const Bitmap *in, int nw, int nh) {
    Bitmap *out = bm_create(nw, nh);
    int x, y;
    for(y = 0; y < nh; y++)
        for(x = 0; x < nw; x++) {
            int sx = x * in->w/nw;
            int sy = y * in->h/nh;
            assert(sx < in->w && sy < in->h);
            BM_SET(out, x, y, BM_GET(in,sx,sy));
        }
    return out;
}

/* http://rosettacode.org/wiki/Bilinear_interpolation */
static double lerp(double s, double e, double t) {
    return s + (e-s)*t;
}
static double blerp(double c00, double c10, double c01, double c11, double tx, double ty) {
    return lerp(
        lerp(c00, c10, tx),
        lerp(c01, c11, tx),
        ty);
}

Bitmap *bm_resample_blin(const Bitmap *in, int nw, int nh) {
    Bitmap *out = bm_create(nw, nh);
    int x, y;
    for(y = 0; y < nh; y++)
        for(x = 0; x < nw; x++) {
            int C[4], c;
            double gx = (double)x * in->w/(double)nw;
            int sx = (int)gx;
            double gy = (double)y * in->h/(double)nh;
            int sy = (int)gy;
            int dx = 1, dy = 1;
            assert(sx < in->w && sy < in->h);
            if(sx + 1 >= in->w){ sx=in->w-1; dx = 0; }
            if(sy + 1 >= in->h){ sy=in->h-1; dy = 0; }
            for(c = 0; c < 4; c++) {
                int p00 = BM_GETN(in,c,sx,sy);
                int p10 = BM_GETN(in,c,sx+dx,sy);
                int p01 = BM_GETN(in,c,sx,sy+dy);
                int p11 = BM_GETN(in,c,sx+dx,sy+dy);
                C[c] = (int)blerp(p00, p10, p01, p11, gx-sx, gy-sy);
            }
            BM_SET_RGBA(out, x, y, C[0], C[1], C[2], C[3]);
        }
    return out;
}

/*
http://www.codeproject.com/Articles/236394/Bi-Cubic-and-Bi-Linear-Interpolation-with-GLSL
except I ported the GLSL code to straight C
*/
static double triangular_fun(double b) {
    b = b * 1.5 / 2.0;
    if( -1.0 < b && b <= 0.0) {
        return b + 1.0;
    } else if(0.0 < b && b <= 1.0) {
        return 1.0 - b;
    }
    return 0;
}

Bitmap *bm_resample_bcub(const Bitmap *in, int nw, int nh) {
    Bitmap *out = bm_create(nw, nh);
    int x, y;

    for(y = 0; y < nh; y++)
    for(x = 0; x < nw; x++) {

        double sum[4] = {0.0, 0.0, 0.0, 0.0};
        double denom[4] = {0.0, 0.0, 0.0, 0.0};

        double a = (double)x * in->w/(double)nw;
        int sx = (int)a;
        double b = (double)y * in->h/(double)nh;
        int sy = (int)b;

        int m, n, c, C;
        for(m = -1; m < 3; m++ )
        for(n = -1; n < 3; n++) {
            double f = triangular_fun((double)sx - a);
            double f1 = triangular_fun(-((double)sy - b));
            for(c = 0; c < 4; c++) {
                int i = sx+m;
                int j = sy+n;
                if(i < 0) i = 0;
                if(i >= in->w) i = in->w - 1;
                if(j < 0) j = 0;
                if(j >= in->h) j = in->h - 1;
                C = BM_GETN(in, c, i, j);
                sum[c] = sum[c] + C * f1 * f;
                denom[c] = denom[c] + f1 * f;
            }
        }

        BM_SET_RGBA(out, x, y, sum[0]/denom[0], sum[1]/denom[1], sum[2]/denom[2], sum[3]/denom[3]);
    }
    return out;
}

/* Sort functions for bm_count_colors() */
static int cnt_comp(const void *ap, const void *bp) {
    int a = *(int*)ap, b = *(int*)bp;
    return a - b;
}
static int cnt_comp_mask(const void*ap, const void*bp) {
    int a = *(int*)ap, b = *(int*)bp;
    return (a & 0x00FFFFFF) - (b & 0x00FFFFFF);
}

int bm_count_colors(Bitmap *b, int use_mask) {
    /* Counts the number of colours in an image by
    treating the pixels in the image as an array
    of integers, sorting them and then counting the
    number of times the value of the array changes.
    Based on this suggestion:
    http://stackoverflow.com/a/128055/115589
    (according to the comments, certain qsort()
    implementations may have problems with large
    images if they are recursive)
    */
    int count = 1, i;
    int npx = b->w * b->h;
    int *sort = malloc(npx * sizeof *sort);
    memcpy(sort, b->data, npx * sizeof *sort);
    if(use_mask) {
        qsort(sort, npx, sizeof(int), cnt_comp_mask);
    } else {
        qsort(sort, npx, sizeof(int), cnt_comp);
    }
    if(use_mask) {
        for(i = 1; i < npx; i++){
            if((sort[i] & 0x00FFFFFF) != (sort[i-1]& 0x00FFFFFF))
                count++;
        }
    } else {
        for(i = 1; i < npx; i++){
            if(sort[i] != sort[i-1])
                count++;
        }
    }
    free(sort);
    return count;
}

void bm_set_color_rgb(Bitmap *bm, unsigned char r, unsigned char g, unsigned char b) {
    bm->color = 0xFF000000 | (r << 16) | (g << 8) | b;
}

void bm_set_alpha(Bitmap *bm, int a) {
    if(a < 0) a = 0;
    if(a > 255) a = 255;
    bm->color = (bm->color & 0x00FFFFFF) | (a << 24);
}

void bm_adjust_rgba(Bitmap *bm, float rf, float gf, float bf, float af) {
    int x, y;
    for(y = 0; y < bm->h; y++)
        for(x = 0; x < bm->w; x++) {
            float R = BM_GETR(bm,x,y);
            float G = BM_GETG(bm,x,y);
            float B = BM_GETB(bm,x,y);
            float A = BM_GETA(bm,x,y);
            BM_SET_RGBA(bm, x, y, rf * R, gf * G, bf * B, af * A);
        }
}

/* Lookup table for bm_color_atoi()
 * This list is based on the HTML and X11 colors on the
 * Wikipedia's list of web colors:
 * http://en.wikipedia.org/wiki/Web_colors
 * I also felt a bit nostalgic for the EGA graphics from my earliest
 * computer memories, so I added the EGA colors (prefixed with "EGA") from here:
 * http://en.wikipedia.org/wiki/Enhanced_Graphics_Adapter
 *
 * Keep the list sorted because a binary search is used.
 *
 * bm_color_atoi()'s text parameter is not case sensitive and spaces are
 * ignored, so for example "darkred" and "Dark Red" are equivalent.
 */
static const struct color_map_entry {
    const char *name;
    unsigned int color;
} color_map[] = {
    {"ALICEBLUE", 0xF0F8FF},
    {"ANTIQUEWHITE", 0xFAEBD7},
    {"AQUA", 0x00FFFF},
    {"AQUAMARINE", 0x7FFFD4},
    {"AZURE", 0xF0FFFF},
    {"BEIGE", 0xF5F5DC},
    {"BISQUE", 0xFFE4C4},
    {"BLACK", 0x000000},
    {"BLANCHEDALMOND", 0xFFEBCD},
    {"BLUE", 0x0000FF},
    {"BLUEVIOLET", 0x8A2BE2},
    {"BROWN", 0xA52A2A},
    {"BURLYWOOD", 0xDEB887},
    {"CADETBLUE", 0x5F9EA0},
    {"CHARTREUSE", 0x7FFF00},
    {"CHOCOLATE", 0xD2691E},
    {"CORAL", 0xFF7F50},
    {"CORNFLOWERBLUE", 0x6495ED},
    {"CORNSILK", 0xFFF8DC},
    {"CRIMSON", 0xDC143C},
    {"CYAN", 0x00FFFF},
    {"DARKBLUE", 0x00008B},
    {"DARKCYAN", 0x008B8B},
    {"DARKGOLDENROD", 0xB8860B},
    {"DARKGRAY", 0xA9A9A9},
    {"DARKGREEN", 0x006400},
    {"DARKKHAKI", 0xBDB76B},
    {"DARKMAGENTA", 0x8B008B},
    {"DARKOLIVEGREEN", 0x556B2F},
    {"DARKORANGE", 0xFF8C00},
    {"DARKORCHID", 0x9932CC},
    {"DARKRED", 0x8B0000},
    {"DARKSALMON", 0xE9967A},
    {"DARKSEAGREEN", 0x8FBC8F},
    {"DARKSLATEBLUE", 0x483D8B},
    {"DARKSLATEGRAY", 0x2F4F4F},
    {"DARKTURQUOISE", 0x00CED1},
    {"DARKVIOLET", 0x9400D3},
    {"DEEPPINK", 0xFF1493},
    {"DEEPSKYBLUE", 0x00BFFF},
    {"DIMGRAY", 0x696969},
    {"DODGERBLUE", 0x1E90FF},
    {"EGABLACK", 0x000000},
    {"EGABLUE", 0x0000AA},
    {"EGABRIGHTBLACK", 0x555555},
    {"EGABRIGHTBLUE", 0x5555FF},
    {"EGABRIGHTCYAN", 0x55FFFF},
    {"EGABRIGHTGREEN", 0x55FF55},
    {"EGABRIGHTMAGENTA", 0xFF55FF},
    {"EGABRIGHTRED", 0xFF5555},
    {"EGABRIGHTWHITE", 0xFFFFFF},
    {"EGABRIGHTYELLOW", 0xFFFF55},
    {"EGABROWN", 0xAA5500},
    {"EGACYAN", 0x00AAAA},
    {"EGADARKGRAY", 0x555555},
    {"EGAGREEN", 0x00AA00},
    {"EGALIGHTGRAY", 0xAAAAAA},
    {"EGAMAGENTA", 0xAA00AA},
    {"EGARED", 0xAA0000},
    {"EGAWHITE", 0xAAAAAA},
    {"FIREBRICK", 0xB22222},
    {"FLORALWHITE", 0xFFFAF0},
    {"FORESTGREEN", 0x228B22},
    {"FUCHSIA", 0xFF00FF},
    {"GAINSBORO", 0xDCDCDC},
    {"GHOSTWHITE", 0xF8F8FF},
    {"GOLD", 0xFFD700},
    {"GOLDENROD", 0xDAA520},
    {"GRAY", 0x808080},
    {"GREEN", 0x008000},
    {"GREENYELLOW", 0xADFF2F},
    {"HONEYDEW", 0xF0FFF0},
    {"HOTPINK", 0xFF69B4},
    {"INDIANRED", 0xCD5C5C},
    {"INDIGO", 0x4B0082},
    {"IVORY", 0xFFFFF0},
    {"KHAKI", 0xF0E68C},
    {"LAVENDER", 0xE6E6FA},
    {"LAVENDERBLUSH", 0xFFF0F5},
    {"LAWNGREEN", 0x7CFC00},
    {"LEMONCHIFFON", 0xFFFACD},
    {"LIGHTBLUE", 0xADD8E6},
    {"LIGHTCORAL", 0xF08080},
    {"LIGHTCYAN", 0xE0FFFF},
    {"LIGHTGOLDENRODYELLOW", 0xFAFAD2},
    {"LIGHTGRAY", 0xD3D3D3},
    {"LIGHTGREEN", 0x90EE90},
    {"LIGHTPINK", 0xFFB6C1},
    {"LIGHTSALMON", 0xFFA07A},
    {"LIGHTSEAGREEN", 0x20B2AA},
    {"LIGHTSKYBLUE", 0x87CEFA},
    {"LIGHTSLATEGRAY", 0x778899},
    {"LIGHTSTEELBLUE", 0xB0C4DE},
    {"LIGHTYELLOW", 0xFFFFE0},
    {"LIME", 0x00FF00},
    {"LIMEGREEN", 0x32CD32},
    {"LINEN", 0xFAF0E6},
    {"MAGENTA", 0xFF00FF},
    {"MAROON", 0x800000},
    {"MEDIUMAQUAMARINE", 0x66CDAA},
    {"MEDIUMBLUE", 0x0000CD},
    {"MEDIUMORCHID", 0xBA55D3},
    {"MEDIUMPURPLE", 0x9370DB},
    {"MEDIUMSEAGREEN", 0x3CB371},
    {"MEDIUMSLATEBLUE", 0x7B68EE},
    {"MEDIUMSPRINGGREEN", 0x00FA9A},
    {"MEDIUMTURQUOISE", 0x48D1CC},
    {"MEDIUMVIOLETRED", 0xC71585},
    {"MIDNIGHTBLUE", 0x191970},
    {"MINTCREAM", 0xF5FFFA},
    {"MISTYROSE", 0xFFE4E1},
    {"MOCCASIN", 0xFFE4B5},
    {"NAVAJOWHITE", 0xFFDEAD},
    {"NAVY", 0x000080},
    {"OLDLACE", 0xFDF5E6},
    {"OLIVE", 0x808000},
    {"OLIVEDRAB", 0x6B8E23},
    {"ORANGE", 0xFFA500},
    {"ORANGERED", 0xFF4500},
    {"ORCHID", 0xDA70D6},
    {"PALEGOLDENROD", 0xEEE8AA},
    {"PALEGREEN", 0x98FB98},
    {"PALETURQUOISE", 0xAFEEEE},
    {"PALEVIOLETRED", 0xDB7093},
    {"PAPAYAWHIP", 0xFFEFD5},
    {"PEACHPUFF", 0xFFDAB9},
    {"PERU", 0xCD853F},
    {"PINK", 0xFFC0CB},
    {"PLUM", 0xDDA0DD},
    {"POWDERBLUE", 0xB0E0E6},
    {"PURPLE", 0x800080},
    {"RED", 0xFF0000},
    {"ROSYBROWN", 0xBC8F8F},
    {"ROYALBLUE", 0x4169E1},
    {"SADDLEBROWN", 0x8B4513},
    {"SALMON", 0xFA8072},
    {"SANDYBROWN", 0xF4A460},
    {"SEAGREEN", 0x2E8B57},
    {"SEASHELL", 0xFFF5EE},
    {"SIENNA", 0xA0522D},
    {"SILVER", 0xC0C0C0},
    {"SKYBLUE", 0x87CEEB},
    {"SLATEBLUE", 0x6A5ACD},
    {"SLATEGRAY", 0x708090},
    {"SNOW", 0xFFFAFA},
    {"SPRINGGREEN", 0x00FF7F},
    {"STEELBLUE", 0x4682B4},
    {"TAN", 0xD2B48C},
    {"TEAL", 0x008080},
    {"THISTLE", 0xD8BFD8},
    {"TOMATO", 0xFF6347},
    {"TURQUOISE", 0x40E0D0},
    {"VIOLET", 0xEE82EE},
    {"WHEAT", 0xF5DEB3},
    {"WHITE", 0xFFFFFF},
    {"WHITESMOKE", 0xF5F5F5},
    {"YELLOW", 0xFFFF00},
    {"YELLOWGREEN", 0x9ACD32},
    {NULL, 0}
};

unsigned int bm_color_atoi(const char *text) {
    unsigned int col = 0;

    if(!text) return 0;

    while(isspace(text[0]))
        text++;

    if(tolower(text[0]) == 'r' && tolower(text[1]) == 'g' && tolower(text[2]) == 'b') {
        /* Color is given like RGB(r,g,b) */
        int v,i;
        text += 3;
        if(text[0] != '(') return 0;
        text++;

        for(i = 0; i < 3; i++) {
            v = 0;
            while(isspace(text[0]))
                text++;
            while(isdigit(text[0])) {
                v = v * 10 + (text[0] - '0');
                text++;
            }
            while(isspace(text[0]))
                text++;
            if(text[0] != ",,)"[i]) return 0;
            text++;
            col = (col << 8) + v;
        }
        return col;
    } else if(isalpha(text[0])) {
        const char *q, *p;

        int min = 0, max = ((sizeof color_map)/(sizeof color_map[0])) - 1;
        while(min <= max) {
            int i = (max + min) >> 1, r;

            p = text;
            q = color_map[i].name;

            /* Hacky case insensitive strcmp() that ignores spaces in p */
            while(*p) {
                if(*p == ' ') p++;
                else {
                    if(tolower(*p) != tolower(*q))
                        break;
                    p++; q++;
                }
            }
            r = tolower(*p) - tolower(*q);

            if(r == 0)
                return color_map[i].color;
            else if(r < 0) {
                max = i - 1;
            } else {
                min = i + 1;
            }
        }
        /* Drop through: You may be dealing with a colour like 'a6664c' */
    } else if(text[0] == '#') {
        text++;
        if(strlen(text) == 3) {
            /* Special case of #RGB that should be treated as #RRGGBB */
            while(text[0]) {
                int c = tolower(text[0]);
                if(c >= 'a' && c <= 'f') {
                    col = (col << 4) + (c - 'a' + 10);
                    col = (col << 4) + (c - 'a' + 10);
                } else {
                    col = (col << 4) + (c - '0');
                    col = (col << 4) + (c - '0');
                }
                text++;
            }
            return col;
        }
    } else if(text[0] == '0' && tolower(text[1]) == 'x') {
        text += 2;
    } else if(tolower(text[0]) == 'h' && tolower(text[1]) == 's' && tolower(text[2]) == 'l') {
        /* Not supported yet.
        http://en.wikipedia.org/wiki/HSL_color_space
        */
        return 0;
    }

    while(isxdigit(text[0])) {
        int c = tolower(text[0]);
        if(c >= 'a' && c <= 'f') {
            col = (col << 4) + (c - 'a' + 10);
        } else {
            col = (col << 4) + (c - '0');
        }
        text++;
    }
    return col;
}

void bm_set_color_s(Bitmap *bm, const char *text) {
    /* FIXME: Better name for this function */
    bm_set_color(bm, bm_color_atoi(text));
}

void bm_set_color(Bitmap *bm, unsigned int col) {
    bm->color = col;
}

void bm_get_color_rgb(Bitmap *bm, int *r, int *g, int *b) {
    *r = (bm->color >> 16) & 0xFF;
    *g = (bm->color >> 8) & 0xFF;
    *b = bm->color & 0xFF;
}

unsigned int bm_get_color(Bitmap *bm) {
    return bm->color;
}

unsigned int bm_picker(Bitmap *bm, int x, int y) {
    if(x < 0 || x >= bm->w || y < 0 || y >= bm->h)
        return 0;
    bm->color = bm_get(bm, x, y);
    return bm->color;
}

int bm_color_is(Bitmap *bm, int x, int y, int r, int g, int b) {
    return BM_GETR(bm,x,y) == r && BM_GETG(bm,x,y) == g && BM_GETB(bm,x,y) == b;
}

double bm_cdist(int color1, int color2) {
    int r1, g1, b1;
    int r2, g2, b2;
    int dr, dg, db;
    r1 = (color1 >> 16) & 0xFF; g1 = (color1 >> 8) & 0xFF; b1 = (color1 >> 0) & 0xFF;
    r2 = (color2 >> 16) & 0xFF; g2 = (color2 >> 8) & 0xFF; b2 = (color2 >> 0) & 0xFF;
    dr = r1 - r2;
    dg = g1 - g2;
    db = b1 - b2;
    return sqrt(dr * dr + dg * dg + db * db);
}

/* Squared distance between colors; so you don't need to get the root if you're
    only interested in comparing distances. */
static int bm_cdist_sq(int color1, int color2) {
    int r1, g1, b1;
    int r2, g2, b2;
    int dr, dg, db;
    r1 = (color1 >> 16) & 0xFF; g1 = (color1 >> 8) & 0xFF; b1 = (color1 >> 0) & 0xFF;
    r2 = (color2 >> 16) & 0xFF; g2 = (color2 >> 8) & 0xFF; b2 = (color2 >> 0) & 0xFF;
    dr = r1 - r2;
    dg = g1 - g2;
    db = b1 - b2;
    return dr * dr + dg * dg + db * db;
}

int bm_lerp(int color1, int color2, double t) {
    int r1, g1, b1;
    int r2, g2, b2;
    int r3, g3, b3;

    if(t <= 0.0) return color1;
    if(t >= 1.0) return color2;

    r1 = (color1 >> 16) & 0xFF; g1 = (color1 >> 8) & 0xFF; b1 = (color1 >> 0) & 0xFF;
    r2 = (color2 >> 16) & 0xFF; g2 = (color2 >> 8) & 0xFF; b2 = (color2 >> 0) & 0xFF;

    r3 = (r2 - r1) * t + r1;
    g3 = (g2 - g1) * t + g1;
    b3 = (b2 - b1) * t + b1;

    return (r3 << 16) | (g3 << 8) | (b3 << 0);
}

int bm_brightness(int color, double adj) {
    int r, g, b;
    if(adj < 0.0) return 0;

    r = (color >> 16) & 0xFF;
    g = (color >> 8) & 0xFF;
    b = (color >> 0) & 0xFF;

    r = (int)((double)r * adj);
    if(r > 0xFF) r = 0xFF;

    g = (int)((double)g * adj);
    if(g > 0xFF) g = 0xFF;

    b = (int)((double)b * adj);
    if(b > 0xFF) b = 0xFF;

    return (r << 16) | (g << 8) | (b << 0);
}

int bm_width(Bitmap *b) {
    return b->w;
}

int bm_height(Bitmap *b) {
    return b->h;
}

void bm_clear(Bitmap *b) {
    int i, j;
    for(j = 0; j < b->h; j++)
        for(i = 0; i < b->w; i++) {
            BM_SET(b, i, j, b->color);
        }
}

void bm_putpixel(Bitmap *b, int x, int y) {
    if(x < b->clip.x0 || x >= b->clip.x1 || y < b->clip.y0 || y >= b->clip.y1)
        return;
    BM_SET(b, x, y, b->color);
}

void bm_line(Bitmap *b, int x0, int y0, int x1, int y1) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx, sy;
    int err, e2;

    if(dx < 0) dx = -dx;
    if(dy < 0) dy = -dy;

    if(x0 < x1)
        sx = 1;
    else
        sx = -1;
    if(y0 < y1)
        sy = 1;
    else
        sy = -1;

    err = dx - dy;

    for(;;) {
        /* Clipping can probably be more effective... */
        if(x0 >= b->clip.x0 && x0 < b->clip.x1 && y0 >= b->clip.y0 && y0 < b->clip.y1)
            BM_SET(b, x0, y0, b->color);

        if(x0 == x1 && y0 == y1) break;

        e2 = 2 * err;

        if(e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if(e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void bm_rect(Bitmap *b, int x0, int y0, int x1, int y1) {
    bm_line(b, x0, y0, x1, y0);
    bm_line(b, x1, y0, x1, y1);
    bm_line(b, x1, y1, x0, y1);
    bm_line(b, x0, y1, x0, y0);
}

void bm_fillrect(Bitmap *b, int x0, int y0, int x1, int y1) {
    int x,y;
    if(x1 < x0) {
        x = x0;
        x0 = x1;
        x1 = x;
    }
    if(y1 < y0) {
        y = y0;
        y0 = y1;
        y1 = y;
    }
    for(y = MAX(y0, b->clip.y0); y < MIN(y1 + 1, b->clip.y1); y++) {
        for(x = MAX(x0, b->clip.x0); x < MIN(x1 + 1, b->clip.x1); x++) {
            assert(y >= 0 && y < b->h && x >= 0 && x < b->w);
            BM_SET(b, x, y, b->color);
        }
    }
}

void bm_dithrect(Bitmap *b, int x0, int y0, int x1, int y1) {
    int x,y;
    if(x1 < x0) {
        x = x0;
        x0 = x1;
        x1 = x;
    }
    if(y1 < y0) {
        y = y0;
        y0 = y1;
        y1 = y;
    }
    for(y = MAX(y0, b->clip.y0); y < MIN(y1 + 1, b->clip.y1); y++) {
        for(x = MAX(x0, b->clip.x0); x < MIN(x1 + 1, b->clip.x1); x++) {
            if((x + y) & 0x1) continue;
            assert(y >= 0 && y < b->h && x >= 0 && x < b->w);
            BM_SET(b, x, y, b->color);
        }
    }
}

void bm_circle(Bitmap *b, int x0, int y0, int r) {
    int x = -r;
    int y = 0;
    int err = 2 - 2 * r;
    do {
        int xp, yp;

        /* Lower Right */
        xp = x0 - x; yp = y0 + y;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        /* Lower Left */
        xp = x0 - y; yp = y0 - x;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        /* Upper Left */
        xp = x0 + x; yp = y0 - y;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        /* Upper Right */
        xp = x0 + y; yp = y0 + x;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        r = err;
        if(r > x) {
            x++;
            err += x*2 + 1;
        }
        if(r <= y) {
            y++;
            err += y * 2 + 1;
        }
    } while(x < 0);
}

void bm_fillcircle(Bitmap *b, int x0, int y0, int r) {
    int x = -r;
    int y = 0;
    int err = 2 - 2 * r;
    do {
        int i;
        for(i = x0 + x; i <= x0 - x; i++) {
            /* Maybe the clipping can be more effective... */
            int yp = y0 + y;
            if(i >= b->clip.x0 && i < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
                BM_SET(b, i, yp, b->color);
            yp = y0 - y;
            if(i >= b->clip.x0 && i < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
                BM_SET(b, i, yp, b->color);
        }

        r = err;
        if(r > x) {
            x++;
            err += x*2 + 1;
        }
        if(r <= y) {
            y++;
            err += y * 2 + 1;
        }
    } while(x < 0);
}

void bm_ellipse(Bitmap *b, int x0, int y0, int x1, int y1) {
    int a = abs(x1-x0), b0 = abs(y1-y0), b1 = b0 & 1;
    long dx = 4 * (1 - a) * b0 * b0,
        dy = 4*(b1 + 1) * a * a;
    long err = dx + dy + b1*a*a, e2;

    if(x0 > x1) { x0 = x1; x1 += a; }
    if(y0 > y1) { y0 = y1; }
    y0 += (b0+1)/2;
    y1 = y0 - b1;
    a *= 8*a;
    b1 = 8 * b0 * b0;

    do {
        if(x1 >= b->clip.x0 && x1 < b->clip.x1 && y0 >= b->clip.y0 && y0 < b->clip.y1)
            BM_SET(b, x1, y0, b->color);

        if(x0 >= b->clip.x0 && x0 < b->clip.x1 && y0 >= b->clip.y0 && y0 < b->clip.y1)
            BM_SET(b, x0, y0, b->color);

        if(x0 >= b->clip.x0 && x0 < b->clip.x1 && y1 >= b->clip.y0 && y1 < b->clip.y1)
            BM_SET(b, x0, y1, b->color);

        if(x1 >= b->clip.x0 && x1 < b->clip.x1 && y1 >= b->clip.y0 && y1 < b->clip.y1)
            BM_SET(b, x1, y1, b->color);

        e2 = 2 * err;
        if(e2 <= dy) {
            y0++; y1--; err += dy += a;
        }
        if(e2 >= dx || 2*err > dy) {
            x0++; x1--; err += dx += b1;
        }
    } while(x0 <= x1);

    while(y0 - y1 < b0) {
        if(x0 - 1 >= b->clip.x0 && x0 - 1 < b->clip.x1 && y0 >= b->clip.y0 && y0 < b->clip.y1)
            BM_SET(b, x0 - 1, y0, b->color);

        if(x1 + 1 >= b->clip.x0 && x1 + 1 < b->clip.x1 && y0 >= b->clip.y0 && y0 < b->clip.y1)
            BM_SET(b, x1 + 1, y0, b->color);
        y0++;

        if(x0 - 1 >= b->clip.x0 && x0 - 1 < b->clip.x1 && y0 >= b->clip.y0 && y0 < b->clip.y1)
            BM_SET(b, x0 - 1, y1, b->color);

        if(x1 + 1 >= b->clip.x0 && x1 + 1 < b->clip.x1 && y0 >= b->clip.y0 && y0 < b->clip.y1)
            BM_SET(b, x1 + 1, y1, b->color);
        y1--;
    }
}

void bm_roundrect(Bitmap *b, int x0, int y0, int x1, int y1, int r) {
    int x = -r;
    int y = 0;
    int err = 2 - 2 * r;
    int rad = r;

    bm_line(b, x0 + r, y0, x1 - r, y0);
    bm_line(b, x0, y0 + r, x0, y1 - r);
    bm_line(b, x0 + r, y1, x1 - r, y1);
    bm_line(b, x1, y0 + r, x1, y1 - r);

    do {
        int xp, yp;

        /* Lower Right */
        xp = x1 - x - rad; yp = y1 + y - rad;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        /* Lower Left */
        xp = x0 - y + rad; yp = y1 - x - rad;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        /* Upper Left */
        xp = x0 + x + rad; yp = y0 - y + rad;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        /* Upper Right */
        xp = x1 + y - rad; yp = y0 + x + rad;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        r = err;
        if(r > x) {
            x++;
            err += x*2 + 1;
        }
        if(r <= y) {
            y++;
            err += y * 2 + 1;
        }
    } while(x < 0);
}

void bm_fillroundrect(Bitmap *b, int x0, int y0, int x1, int y1, int r) {
    int x = -r;
    int y = 0;
    int err = 2 - 2 * r;
    int rad = r;
    do {
        int xp, xq, yp, i;

        xp = x0 + x + rad;
        xq = x1 - x - rad;
        for(i = xp; i <= xq; i++) {
            yp = y1 + y - rad;
            if(i >= b->clip.x0 && i < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
                BM_SET(b, i, yp, b->color);
            yp = y0 - y + rad;
            if(i >= b->clip.x0 && i < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
                BM_SET(b, i, yp, b->color);
        }

        r = err;
        if(r > x) {
            x++;
            err += x*2 + 1;
        }
        if(r <= y) {
            y++;
            err += y * 2 + 1;
        }
    } while(x < 0);

    for(y = MAX(y0 + rad + 1, b->clip.y0); y < MIN(y1 - rad, b->clip.y1); y++) {
        for(x = MAX(x0, b->clip.x0); x <= MIN(x1,b->clip.x1 - 1); x++) {
            assert(x >= 0 && x < b->w && y >= 0 && y < b->h);
            BM_SET(b, x, y, b->color);
        }
    }
}

/* Bexier curve with 3 control points.
 * See http://devmag.org.za/2011/04/05/bzier-curves-a-tutorial/
 * I tried the more optimized version at
 * http://members.chello.at/~easyfilter/bresenham.html
 * but that one had some caveats.
 */
void bm_bezier3(Bitmap *b, int x0, int y0, int x1, int y1, int x2, int y2) {
    int lx = x0, ly = y0;
    int steps = 12;
    double inc = 1.0/steps;
    double t = inc, dx, dy;

    do {
        dx = (1-t)*(1-t)*x0 + 2*(1-t)*t*x1 + t*t*x2;
        dy = (1-t)*(1-t)*y0 + 2*(1-t)*t*y1 + t*t*y2;
        bm_line(b, lx, ly, dx, dy);
        lx = dx;
        ly = dy;
        t += inc;
    } while(t < 1.0);
    bm_line(b, dx, dy, x2, y2);
}

void bm_fill(Bitmap *b, int x, int y) {
    struct node {int x; int y;}
        *queue,
        n;
    int qs = 0, /* queue size */
        mqs = 128; /* Max queue size */
    unsigned int sc, dc; /* Source and Destination colours */

    dc = b->color;
    bm_picker(b, x, y);
    sc = b->color;

    /* Don't fill if source == dest
     * It leads to major performance problems otherwise
     */
    if(sc == dc)
        return;

    queue = calloc(mqs, sizeof *queue);
    if(!queue)
        return;

    n.x = x; n.y = y;
    queue[qs++] = n;

    while(qs > 0) {
        struct node w,e, nn;
        int i;

        n = queue[--qs];
        w = n;
        e = n;

        if(!bm_picker(b, n.x, n.y) == sc)
            continue;

        while(w.x > b->clip.x0) {
            if(bm_picker(b, w.x-1, w.y) != sc) {
                break;
            }
            w.x--;
        }
        while(e.x < b->clip.x1 - 1) {
            if(bm_picker(b, e.x+1, e.y) != sc) {
                break;
            }
            e.x++;
        }
        for(i = w.x; i <= e.x; i++) {
            assert(i >= 0 && i < b->w);
            BM_SET(b, i, w.y, dc);
            if(w.y > b->clip.y0) {
                if(bm_picker(b, i, w.y - 1) == sc) {
                    nn.x = i; nn.y = w.y - 1;
                    queue[qs++] = nn;
                    if(qs == mqs) {
                        mqs <<= 1;
                        queue = realloc(queue, mqs * sizeof *queue);
                        if(!queue)
                            return;
                    }
                }
            }
            if(w.y < b->clip.y1 - 1) {
                if(bm_picker(b, i, w.y + 1) == sc) {
                    nn.x = i; nn.y = w.y + 1;
                    queue[qs++] = nn;
                    if(qs == mqs) {
                        mqs <<= 1;
                        queue = realloc(queue, mqs * sizeof *queue);
                        if(!queue)
                            return;
                    }
                }
            }
        }
    }
    free(queue);
    b->color = dc;
}

static int closest_color(int c, int palette[], size_t n) {
    int i, m = 0, md = bm_cdist_sq(c, palette[m]);
    for(i = 1; i < n; i++) {
        int d = bm_cdist_sq(c, palette[i]);
        if(d < md) {
            md = d;
            m = i;
        }
    }
    return palette[m];
}

static void fs_add_factor(Bitmap *b, int x, int y, int er, int eg, int eb, int f) {
    int c, R, G, B;
    if(x < 0 || x >= b->w || y < 0 || y >= b->h)
        return;
    c = bm_get(b, x, y);

    R = ((c >> 16) & 0xFF) + ((f * er) >> 4);
    G = ((c >> 8) & 0xFF) + ((f * eg) >> 4);
    B = ((c >> 0) & 0xFF) + ((f * eb) >> 4);

    if(R > 255) R = 255;
    if(R < 0) R = 0;
    if(G > 255) G = 255;
    if(G < 0) G = 0;
    if(B > 255) B = 255;
    if(B < 0) B = 0;

    BM_SET_RGBA(b, x, y, R, G, B, 0);
}

void bm_reduce_palette(Bitmap *b, int palette[], size_t n) {
    /* Floyd–Steinberg (error-diffusion) dithering
        http://en.wikipedia.org/wiki/Floyd%E2%80%93Steinberg_dithering */
    int x, y;
    if(!b)
        return;
    for(y = 0; y < b->h; y++) {
        for(x = 0; x < b->w; x++) {
            int r1, g1, b1;
            int r2, g2, b2;
            int er, eg, eb;
            int newpixel, oldpixel = BM_GET(b, x, y);

            newpixel = closest_color(oldpixel, palette, n);

            bm_set(b, x, y, newpixel);

            r1 = (oldpixel >> 16) & 0xFF; g1 = (oldpixel >> 8) & 0xFF; b1 = (oldpixel >> 0) & 0xFF;
            r2 = (newpixel >> 16) & 0xFF; g2 = (newpixel >> 8) & 0xFF; b2 = (newpixel >> 0) & 0xFF;
            er = r1 - r2; eg = g1 - g2; eb = b1 - b2;

            fs_add_factor(b, x + 1, y    , er, eg, eb, 7);
            fs_add_factor(b, x - 1, y + 1, er, eg, eb, 3);
            fs_add_factor(b, x    , y + 1, er, eg, eb, 5);
            fs_add_factor(b, x + 1, y + 1, er, eg, eb, 1);
        }
    }
}

static int bayer4x4[16] = { /* (1/17) */
    1,  9,  3, 11,
    13, 5, 15,  7,
    4, 12,  2, 10,
    16, 8, 14,  6
};
static int bayer8x8[64] = { /*(1/65)*/
    1,  49, 13, 61,  4, 52, 16, 64,
    33, 17, 45, 29, 36, 20, 48, 32,
    9,  57,  5, 53, 12, 60,  8, 56,
    41, 25, 37, 21, 44, 28, 40, 24,
    3,  51, 15, 63,  2, 50, 14, 62,
    35, 19, 47, 31, 34, 18, 46, 30,
    11, 59,  7, 55, 10, 58,  6, 54,
    43, 27, 39, 23, 42, 26, 38, 22,
};
static void reduce_palette_bayer(Bitmap *b, int palette[], size_t n, int bayer[], int dim, int fac) {
    /* Ordered dithering: https://en.wikipedia.org/wiki/Ordered_dithering
    The resulting image may be of lower quality than you would get with
    Floyd-Steinberg, but it does have some advantages:
        * the repeating patterns compress better
        * it is better suited for line-art graphics
        * if you were to make an animation (not supported at the moment)
            subsequent frames would be less jittery than error-diffusion.
    */
    int x, y;
    int af = dim - 1; /* mod factor */
    int sub = (dim * dim) / 2 - 1; /* 7 if dim = 4, 31 if dim = 8 */
    if(!b)
        return;
    for(y = 0; y < b->h; y++) {
        for(x = 0; x < b->w; x++) {
            int R, G, B;
            int newpixel, oldpixel = BM_GET(b, x, y);

            R = (oldpixel >> 16) & 0xFF; G = (oldpixel >> 8) & 0xFF; B = (oldpixel >> 0) & 0xFF;

            /* The "- sub" below is because otherwise colours are only adjusted upwards,
                causing the resulting image to be brighter than the original.
                This seems to be the same problem this guy http://stackoverflow.com/q/4441388/115589
                ran into, but I can't find anyone else on the web solving it like I did. */
            int f = (bayer[(y & af) * dim + (x & af)] - sub);

            R += R * f / fac;
            if(R > 255) R = 255; if(R < 0) R = 0;
            G += G * f / fac;
            if(G > 255) G = 255; if(G < 0) G = 0;
            B += B * f / fac;
            if(B > 255) B = 255; if(B < 0) B = 0;
            oldpixel = (R << 16) | (G << 8) | B;
            newpixel = closest_color(oldpixel, palette, n);
            BM_SET(b, x, y, newpixel);
        }
    }
}

void bm_reduce_palette_OD4(Bitmap *b, int palette[], size_t n) {
    reduce_palette_bayer(b, palette, n, bayer4x4, 4, 17);
}

void bm_reduce_palette_OD8(Bitmap *b, int palette[], size_t n) {
    reduce_palette_bayer(b, palette, n, bayer8x8, 8, 65);
}

/** FONT FUNCTIONS **********************************************************/

void bm_set_font(Bitmap *b, BmFont *font) {
    if(b->font && b->font->dtor)
        b->font->dtor(b->font);
    b->font = font;
}

int bm_text_width(Bitmap *b, const char *s) {
    int len = 0, max_len = 0;
    int glyph_width;

    if(!b->font || !b->font->width)
        return 0;

    glyph_width = b->font->width(b->font);
    while(*s) {
        if(*s == '\n') {
            if(len > max_len)
                max_len = len;
            len = 0;
        } else if(*s == '\t') {
            len+=4;
        } else if(isprint(*s)) {
            len++;
        }
        s++;
    }
    if(len > max_len)
        max_len = len;
    return max_len * glyph_width;
}

int bm_text_height(Bitmap *b, const char *s) {
    int height = 1;
    int glyph_height;
    if(!b->font || !b->font->height)
        return 0;
    glyph_height = b->font->height(b->font);
    while(*s) {
        if(*s == '\n') height++;
        s++;
    }
    return height * glyph_height;
}

int bm_putc(Bitmap *b, int x, int y, char c) {
    char text[2] = {c, 0};
    return bm_puts(b, x, y, text);
}

int bm_puts(Bitmap *b, int x, int y, const char *text) {
    if(!b->font || !b->font->puts)
        return 0;
    return b->font->puts(b, x, y, text);
}

int bm_printf(Bitmap *b, int x, int y, const char *fmt, ...) {
    char buffer[256];
    va_list arg;
    if(!b->font || !b->font->puts) return 0;
    va_start(arg, fmt);
    vsnprintf(buffer, sizeof buffer, fmt, arg);
    va_end(arg);
    return bm_puts(b, x, y, buffer);
}

/** XBM FONT FUNCTIONS ******************************************************/

typedef struct {
    const unsigned char *bits;
    int spacing;
} XbmFontInfo;

static void xbmf_putc(Bitmap *b, XbmFontInfo *info, int x, int y, char c) {
    int frow, fcol, byte;
    unsigned int col;
    int i, j;
    if(!info || !info->bits || c < 32 || c > 127) return;
    c -= 32;
    fcol = c >> 3;
    frow = c & 0x7;
    byte = frow * FONT_WIDTH + fcol;

    col = bm_get_color(b);

    for(j = 0; j < 8 && y + j < b->clip.y1; j++) {
        if(y + j >= b->clip.y0) {
            char bits = info->bits[byte];
            for(i = 0; i < 8 && x + i < b->clip.x1; i++) {
                if(x + i >= b->clip.x0 && !(bits & (1 << i))) {
                    bm_set(b, x + i, y + j, col);
                }
            }
        }
        byte += FONT_WIDTH >> 3;
    }
}

static int xbmf_puts(Bitmap *b, int x, int y, const char *text) {
    XbmFontInfo *info;
    int xs = x;
    if(!b->font) return 0;
    info = b->font->data;
    while(text[0]) {
        if(text[0] == '\n') {
            y += 8;
            x = xs;
        } else if(text[0] == '\t') {
            /* I briefly toyed with the idea of having tabs line up,
             * but it doesn't really make sense because
             * this isn't exactly a character based terminal.
             */
            x += 4 * info->spacing;
        } else if(text[0] == '\r') {
            /* why would anyone find this useful? */
            x = xs;
        } else {
            xbmf_putc(b, info, x, y, text[0]);
            x += info->spacing;
        }
        text++;
        if(y > b->h) {
            /* I used to check x >= b->w as well,
            but it doesn't take \n's into account */
            return 1;
        }
    }
    return 1;
}
static void xbmf_dtor(struct bitmap_font *font) {
    XbmFontInfo *info = font->data;
    free(info);
    free(font);
}
static int xbmf_width(struct bitmap_font *font) {
    XbmFontInfo *info = font->data;
    return info->spacing;
}
static int xbmf_height(struct bitmap_font *font) {
    return 8;
}

BmFont *bm_make_xbm_font(const unsigned char *bits, int spacing) {
    BmFont *font;
    XbmFontInfo *info;
    font = malloc(sizeof *font);
    if(!font)
        return NULL;
    info = malloc(sizeof *info);
    if(!info) {
        free(font);
        return NULL;
    }

    info->bits = bits;
    info->spacing = spacing;

    font->type = "XBM";
    font->puts = xbmf_puts;
    font->dtor = xbmf_dtor;
    font->width = xbmf_width;
    font->height = xbmf_height;
    font->data = info;

    return font;
}

#ifndef NO_FONTS

static struct xbm_font_info {
    const char *s;
    int i;
    const unsigned char *bits;
    int spacing;
} xbm_font_infos[] = {
    {"NORMAL", BM_FONT_NORMAL, normal_bits, 6},
    {"BOLD", BM_FONT_BOLD, bold_bits, 8},
    {"CIRCUIT", BM_FONT_CIRCUIT, circuit_bits, 7},
    {"HAND", BM_FONT_HAND, hand_bits, 7},
    {"SMALL", BM_FONT_SMALL, small_bits, 5},
    {"SMALL_I", BM_FONT_SMALL_I, smallinv_bits, 7},
    {"THICK", BM_FONT_THICK, thick_bits, 6},
    {NULL, 0}
};

void bm_std_font(Bitmap *b, enum bm_fonts font) {
    struct xbm_font_info *info = &xbm_font_infos[font];
    BmFont * bfont = bm_make_xbm_font(info->bits, info->spacing);
    bm_set_font(b, bfont);
}

int bm_font_index(const char *name) {
    int i = 0;
    char buffer[12], *c = buffer;
    do {
        *c++ = toupper(*name++);
    } while(*name && c - buffer < sizeof buffer - 1);
    *c = '\0';

    while(xbm_font_infos[i].s) {
        if(!strcmp(xbm_font_infos[i].s, buffer)) {
            return xbm_font_infos[i].i;
        }
        i++;
    }
    return BM_FONT_NORMAL;
}

const char *bm_font_name(int index) {
    int i = 0;
    while(xbm_font_infos[i].s) {
        i++;
        if(xbm_font_infos[i].i == index) {
            return xbm_font_infos[i].s;
        }
    }
    return xbm_font_infos[0].s;
}
#endif /* NO_FONTS */
