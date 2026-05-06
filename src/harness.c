#include <stdio.h>
#include <stdlib.h>
#include <png.h>

/*
 * Dummy fuzzing harness for libpng — CS-412 EPFL
 *
 * Pipeline:
 *   AFL++ generates a file  →  our main() opens it  →  we hand it to libpng
 *   libpng reads/parses it  →  AFL++ watches which code paths were hit
 *   repeat thousands of times per second
 *
 * Usage (AFL++ replaces @@ with the path to each generated test file):
 *   afl-fuzz -i seeds/ -o findings/ -x png.dict -- ./png_fuzz @@
 */

int main(int argc, char **argv)
{

    /* ── 1. Get the input file from AFL++ ───────────────────────
     * AFL++ calls this binary with a file path as argv[1].
     * That file contains the (possibly mutated) PNG to test.     */
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp)
        return 1; /* AFL++ will just move on */

    /* ── 2. Set up libpng read structures ───────────────────────
     * png_structp  holds the internal decoder state.
     * png_infop    holds image metadata (width, height, color type…) */
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                             NULL, NULL, NULL);
    if (!png)
    {
        fclose(fp);
        return 1;
    }

    png_infop info = png_create_info_struct(png);
    if (!info)
    {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return 1;
    }

    /* ── 3. Error handling with setjmp ──────────────────────────
     * libpng signals errors by calling longjmp() back to here.
     * WITHOUT this block, every malformed input causes an unhandled
     * longjmp → AFL++ logs it as a crash → thousands of false positives.
     * WITH it, we land here cleanly and exit 0.                   */
    if (setjmp(png_jmpbuf(png)))
    {
        /* libpng hit an error — clean up and exit normally */
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0; /* exit 0 = not a crash, AFL++ keeps going */
    }

    /* ── 4. Point libpng at our file ────────────────────────────
     * png_init_io() tells libpng to read from our FILE* handle.   */
    png_init_io(png, fp);

    /* ── 5. Read the header (IHDR + metadata chunks) ───────────
     * This parses the PNG signature, IHDR, and any metadata chunks
     * (gAMA, tEXt, etc.) before the actual pixel data.            */
    png_read_info(png, info);

    /* ── 6. Optional: enable transformations ────────────────────
     * These expand more code paths inside libpng, which means more
     * coverage for AFL++. CVE-2015-8126 lives in png_set_expand().
     * Comment them out to start simple, then add them back.       */
    png_set_expand(png);      /* palette/gray → full RGB         */
    png_set_strip_16(png);    /* 16-bit depth → 8-bit            */
    png_set_gray_to_rgb(png); /* grayscale → RGB                 */
    png_read_update_info(png, info);

    /* ── 7. Allocate row buffers and read pixel data ────────────
     * png_get_image_height() / png_get_rowbytes() come from the
     * IHDR we already parsed. A mutated IHDR could claim 65535×65535
     * pixels — cap it to avoid OOM inside the fuzzer.             */
    png_uint_32 height = png_get_image_height(png, info);
    png_uint_32 rowbytes = png_get_rowbytes(png, info);

    if (height == 0 || rowbytes == 0 || height > 1000 || rowbytes > 10000)
    {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0; /* skip absurd dimensions, not a crash */
    }

    png_bytep *rows = (png_bytep *)malloc(height * sizeof(png_bytep));
    if (!rows)
    {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0;
    }
    for (png_uint_32 i = 0; i < height; i++)
    {
        rows[i] = (png_bytep)malloc(rowbytes);
        if (!rows[i])
        {
            /* free already-allocated rows */
            for (png_uint_32 j = 0; j < i; j++)
                free(rows[j]);
            free(rows);
            png_destroy_read_struct(&png, &info, NULL);
            fclose(fp);
            return 0;
        }
    }

    /* This is where the heavy parsing (IDAT decompression +
     * defiltering) happens — the richest attack surface.          */
    png_read_image(png, rows);

    /* read any trailing chunks after IDAT (e.g. tEXt, tIME)      */
    png_read_end(png, NULL);

    /* ── 8. Clean up ────────────────────────────────────────────*/
    for (png_uint_32 i = 0; i < height; i++)
        free(rows[i]);
    free(rows);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    return 0; /* always exit 0 — real crashes are caught by ASan */
}