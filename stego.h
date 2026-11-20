#ifndef STEGO_H
#define STEGO_H

#include <stddef.h>
#include "common.h"
#include "bmp.h"

size_t stego_max_message_size(const BMPImage *img);

Status stego_encode(
    BMPImage *img,
    const unsigned char *message,
    size_t message_len,
    const unsigned char *password,
    size_t password_len
);

Status stego_decode(
    const BMPImage *img,
    const unsigned char *password,
    size_t password_len,
    unsigned char **message_out,
    size_t *message_len_out
);

#endif
