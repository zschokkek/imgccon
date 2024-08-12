#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <png.h>

void convert_png_to_jpg(const char *png_filename, const char *jpg_filename) {
    FILE *fp = fopen(png_filename, "rb");
    if (!fp) {
        fprintf(stderr, "Could not open file %s for reading\n", png_filename);
        return;
    }

    // Read PNG file
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        fprintf(stderr, "Could not create PNG read struct\n");
        return;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        fclose(fp);
        png_destroy_read_struct(&png, NULL, NULL);
        fprintf(stderr, "Could not create PNG info struct\n");
        return;
    }

    if (setjmp(png_jmpbuf(png))) {
        fclose(fp);
        png_destroy_read_struct(&png, &info, NULL);
        fprintf(stderr, "Error during PNG read\n");
        return;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    // Handle bit depth and color types with switch statements
    switch (bit_depth) {
        case 16:
            png_set_strip_16(png);
            break;
        case 8:
        case 4:
        case 2:
        case 1:
            // Already 8-bit or lower, no need to change
            break;
        default:
            fprintf(stderr, "Unsupported bit depth %d\n", bit_depth);
            return;
    }

    switch (color_type) {
        case PNG_COLOR_TYPE_PALETTE:
            png_set_palette_to_rgb(png);
            break;
        case PNG_COLOR_TYPE_GRAY:
            if (bit_depth < 8)
                png_set_expand_gray_1_2_4_to_8(png);
            png_set_gray_to_rgb(png);
            break;
        case PNG_COLOR_TYPE_RGB:
            // Already RGB, no conversion needed
            break;
        case PNG_COLOR_TYPE_RGBA:
            // Alpha channel, add filler
            break;
        case PNG_COLOR_TYPE_GRAY_ALPHA:
            png_set_gray_to_rgb(png);
            break;
        default:
            fprintf(stderr, "Unsupported color type %d\n", color_type);
            return;
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }

    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    png_read_update_info(png, info);

    png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte *)malloc(png_get_rowbytes(png, info));
    }

    png_read_image(png, row_pointers);

    fclose(fp);

    // Write JPG file
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    FILE *outfile = fopen(jpg_filename, "wb");
    if (!outfile) {
        fprintf(stderr, "Could not open file %s for writing\n", jpg_filename);
        return;
    }
    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_start_compress(&cinfo, TRUE);

    JSAMPROW row_pointer;
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer = (JSAMPROW)row_pointers[cinfo.next_scanline];
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);

    // Free memory
    for (int y = 0; y < height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);
    png_destroy_read_struct(&png, &info, NULL);
}

int main() {
    convert_png_to_jpg("input.png", "output.jpg");
    return 0;
}

