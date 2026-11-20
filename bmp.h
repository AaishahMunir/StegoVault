#ifndef BMP_H
#define BMP_H

#include <stddef.h>
#include <stdint.h>
#include "common.h"

/*
   These fields describe the BMP file and where its pixel bytes are.
   data contains the complete file, including its header.
*/
typedef struct {
    unsigned char *data;
    size_t file_size;
    size_t pixel_offset;
    size_t width;
    size_t height;
    size_t row_stride;
    size_t pixel_bytes_per_row;
} BMPImage;

Status bmp_load(const char *filename, BMPImage *img);
Status bmp_save(const char *filename, const BMPImage *img);
void bmp_free(BMPImage *img);

size_t bmp_pixel_byte_count(const BMPImage *img);
size_t bmp_pixel_file_offset(const BMPImage *img, size_t pixel_index);

#endif
