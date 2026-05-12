#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include <string.h>


int main(int argc, char **argv)
{

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp)
        return 1; 

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

    if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0; 
    }

    png_init_io(png, fp);
    png_read_info(png, info);
   
    //Try to find issue with png_set_text
    png_textp existing_text;
    
    int num_comment = 0;
    num_comment = png_get_text(png, info, &existing_text, NULL);
    png_free_data(png, info, PNG_FREE_TEXT, -1);
    
    png_text t[1];
    //memset(&t, 0, sizeof(t));
    t[0].compression = PNG_TEXT_COMPRESSION_zTXt;
    t[0].key = "test";
    t[0].text = "abc";
    t[0].text_length = 3;
    png_set_text(png, info, t, 1);


    // Try more input transformations to reach more code paths
    png_uint_32 color_type = png_get_color_type(png, info);
    png_uint_32 bit_depth = png_get_bit_depth(png, info);
    png_color_8p sig_bit;
    
    
    png_set_expand(png);   
    if (bit_depth == 16)
        png_set_strip_16(png);   
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
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
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGB_ALPHA)
        png_set_bgr(png);
    if (bit_depth == 1 && color_type == PNG_COLOR_TYPE_GRAY)
        png_set_invert_mono(png);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_invert_mono(png);
    png_set_interlace_handling(png);
    png_read_update_info(png, info);
    
 
    png_uint_32 height = png_get_image_height(png, info);
    png_uint_32 rowbytes = png_get_rowbytes(png, info);

    if (height == 0 || rowbytes == 0 || height > 4096 || rowbytes > 65536)
    {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return 0; 
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
            for (png_uint_32 j = 0; j < i; j++)
                free(rows[j]);
            free(rows);
            png_destroy_read_struct(&png, &info, NULL);
            fclose(fp);
            return 0;
        }
    }

    png_read_image(png, rows);
    png_read_end(png, NULL);

    for (png_uint_32 i = 0; i < height; i++)
        free(rows[i]);
    free(rows);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    return 0;
}