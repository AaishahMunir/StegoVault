#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#include "bmp.h"

/* Read a 16-bit little-endian number from the file data. */
static uint16_t read_u16(const unsigned char *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

/* Read a 32-bit little-endian number from the file data. */
static uint32_t read_u32(const unsigned char *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

Status bmp_load(const char *filename, BMPImage *img)
{
    FILE *fp;
    long file_length;
    uint32_t dib_size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t pixel_offset;
    size_t row_bytes;
    size_t row_stride;
    size_t image_height;
    size_t pixel_data_size;

    if (filename == NULL || img == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }

    memset(img, 0, sizeof(*img));

    fp = fopen(filename, "rb");
    if (fp == NULL) {
        return STATUS_FILE_ERROR;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return STATUS_FILE_ERROR;
    }

    file_length = ftell(fp);
    if (file_length < 54) {
        fclose(fp);
        return STATUS_INVALID_BMP;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return STATUS_FILE_ERROR;
    }

    img->file_size = (size_t)file_length;
    img->data = malloc(img->file_size);

    if (img->data == NULL) {
        fclose(fp);
        img->file_size = 0;
        return STATUS_MEMORY_ERROR;
    }

    if (fread(img->data, 1, img->file_size, fp) != img->file_size) {
        fclose(fp);
        bmp_free(img);
        return STATUS_FILE_ERROR;
    }

    fclose(fp);

    /* A BMP file starts with the two characters B and M. */
    if (img->data[0] != 'B' || img->data[1] != 'M') {
        bmp_free(img);
        return STATUS_INVALID_BMP;
    }

    pixel_offset = read_u32(img->data + 10);
    dib_size = read_u32(img->data + 14);

    if (dib_size < 40 || pixel_offset < 54 ||
        pixel_offset > img->file_size) {
        bmp_free(img);
        return STATUS_UNSUPPORTED_BMP;
    }

    width = (int32_t)read_u32(img->data + 18);
    height = (int32_t)read_u32(img->data + 22);
    planes = read_u16(img->data + 26);
    bits_per_pixel = read_u16(img->data + 28);
    compression = read_u32(img->data + 30);

    if (width <= 0 || height == 0 || height == INT32_MIN || planes != 1) {
        bmp_free(img);
        return STATUS_INVALID_BMP;
    }

    if (bits_per_pixel != 24 || compression != 0) {
        bmp_free(img);
        return STATUS_UNSUPPORTED_BMP;
    }

    image_height = (size_t)(height < 0 ? -height : height);
    img->width = (size_t)width;
    img->height = image_height;
    img->pixel_offset = (size_t)pixel_offset;

    img->pixel_bytes_per_row = img->width * 3;

    /* BMP rows are padded so that each row's size is a multiple of 4. */
    row_bytes = img->pixel_bytes_per_row;
    row_stride = ((row_bytes + 3) / 4) * 4;
    img->row_stride = row_stride;

    if (image_height > (SIZE_MAX / row_stride)) {
        bmp_free(img);
        return STATUS_INVALID_BMP;
    }

    pixel_data_size = row_stride * image_height;

    if (pixel_data_size > img->file_size - img->pixel_offset) {
        bmp_free(img);
        return STATUS_INVALID_BMP;
    }

    return STATUS_OK;
}

Status bmp_save(const char *filename, const BMPImage *img)
{
    FILE *fp;

    if (filename == NULL || img == NULL || img->data == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }

    fp = fopen(filename, "wb");
    if (fp == NULL) {
        return STATUS_FILE_ERROR;
    }

    if (fwrite(img->data, 1, img->file_size, fp) != img->file_size) {
        fclose(fp);
        return STATUS_FILE_ERROR;
    }

    fclose(fp);
    return STATUS_OK;
}

void bmp_free(BMPImage *img)
{
    if (img == NULL) {
        return;
    }

    free(img->data);
    memset(img, 0, sizeof(*img));
}

/* Number of usable pixel bytes, excluding BMP row-padding bytes. */
size_t bmp_pixel_byte_count(const BMPImage *img)
{
    if (img == NULL) {
        return 0;
    }

    return img->pixel_bytes_per_row * img->height;
}

/*
   Convert a pixel-data index into an offset in the complete BMP file.
   The calculation skips padding at the end of each BMP row.
   Returns SIZE_MAX if the requested index is outside the pixel data.
*/
size_t bmp_pixel_file_offset(const BMPImage *img, size_t pixel_index)
{
    size_t row;
    size_t column;

    if (img == NULL || img->data == NULL ||
        img->pixel_bytes_per_row == 0 ||
        pixel_index >= bmp_pixel_byte_count(img)) {
        return SIZE_MAX;
    }

    row = pixel_index / img->pixel_bytes_per_row;
    column = pixel_index % img->pixel_bytes_per_row;

    return img->pixel_offset + (row * img->row_stride) + column;
}
