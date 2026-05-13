#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include <string.h>


/* In-memory reader — lets libpng read from a buffer instead of FILE* */
typedef struct {
    unsigned char *data;
    int            size;
    int            pos;
} membuf_t;

static void mem_read_fn(png_structp png, png_bytep dst, png_size_t n)
{
    membuf_t *m = (membuf_t *)png_get_io_ptr(png);
    if (m->pos + (int)n > m->size)
        png_error(png, "unexpected end of data");
    memcpy(dst, m->data + m->pos, n);
    m->pos += (int)n;
}

__AFL_FUZZ_INIT();

int main(void)
{
    __AFL_INIT();

    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

    while (__AFL_LOOP(1000)) {

        int len = __AFL_FUZZ_TESTCASE_LEN;
        if (len <= 0) continue;

        //Set up libpng structures
        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                                 NULL, NULL, NULL);
        if (!png) continue;

        png_infop info = png_create_info_struct(png);
        if (!info) {
            png_destroy_read_struct(&png, NULL, NULL);
            continue;
        }

        //Error handler
        if (setjmp(png_jmpbuf(png))) {
            png_destroy_read_struct(&png, &info, NULL);
            continue;
        }

        //Feed input via in-memory callback
        membuf_t membuf = { buf, len, 0 };
        png_set_read_fn(png, &membuf, mem_read_fn);

        //Read header
        png_read_info(png, info);

        //png_set_text path
        png_textp existing_text;
        int num_comment = 0;
        num_comment = png_get_text(png, info, &existing_text, NULL);
        png_free_data(png, info, PNG_FREE_TEXT, -1);

        png_text t[1];
        t[0].compression = PNG_TEXT_COMPRESSION_zTXt;
        t[0].key = (png_charp)"test";
        t[0].text = (png_charp)"abc";
        t[0].text_length = 3;
        png_set_text(png, info, t, 1);

        //Transformations
        png_uint_32  color_type = png_get_color_type(png, info);
        png_uint_32  bit_depth  = png_get_bit_depth(png, info);
        png_color_8p sig_bit;

        png_set_expand(png);
        if (bit_depth == 16)
            png_set_strip_16(png);
        if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png);
        if (color_type == PNG_COLOR_TYPE_RGB_ALPHA)
            png_set_swap_alpha(png);
        if (color_type & PNG_COLOR_MASK_ALPHA)
            png_set_strip_alpha(png);
        png_set_invert_alpha(png);
        if (bit_depth < 8)
            png_set_packing(png);
        if (png_get_sBIT(png, info, &sig_bit))
            png_set_shift(png, sig_bit);
        if (color_type == PNG_COLOR_TYPE_RGB ||
            color_type == PNG_COLOR_TYPE_RGB_ALPHA)
            png_set_bgr(png);
        if (bit_depth == 1 && color_type == PNG_COLOR_TYPE_GRAY)
            png_set_invert_mono(png);
        if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_invert_mono(png);
        png_set_interlace_handling(png);
        png_read_update_info(png, info);

        //Dimension guard
        png_uint_32 height   = png_get_image_height(png, info);
        png_uint_32 rowbytes = png_get_rowbytes(png, info);

        if (height == 0 || rowbytes == 0 ||
            height > 4096 || rowbytes > 65536) {
            png_destroy_read_struct(&png, &info, NULL);
            continue;
        }

        //Allocate row buffers
        png_bytep *rows = (png_bytep *)malloc(height * sizeof(png_bytep));
        if (!rows) {
            png_destroy_read_struct(&png, &info, NULL);
            continue;
        }

        png_uint_32 i;
        int alloc_ok = 1;
        for (i = 0; i < height; i++) {
            rows[i] = (png_bytep)malloc(rowbytes);
            if (!rows[i]) {
                for (png_uint_32 j = 0; j < i; j++) free(rows[j]);
                free(rows);
                png_destroy_read_struct(&png, &info, NULL);
                alloc_ok = 0;
                break;
            }
        }
        if (!alloc_ok) continue;

        //Decode pixel data
        png_read_image(png, rows);
        png_read_end(png, NULL);

        //Clean up
        for (i = 0; i < height; i++) free(rows[i]);
        free(rows);
        png_destroy_read_struct(&png, &info, NULL);
    }

    return 0;
}