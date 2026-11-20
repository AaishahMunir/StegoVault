#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "stego.h"
#include "crypto.h"

/* Put one bit into the least significant bit of one pixel byte. */
static Status embed_bit(BMPImage *img, size_t index, unsigned char bit)
{
    size_t offset;

    if (img == NULL || img->data == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }

    if (index >= bmp_pixel_byte_count(img)) {
        return STATUS_CAPACITY_ERROR;
    }

    offset = bmp_pixel_file_offset(img, index);

    if (offset == SIZE_MAX || offset >= img->file_size) {
        return STATUS_INVALID_BMP;
    }

    img->data[offset] = (unsigned char)((img->data[offset] & 0xFEu) |
                                        (bit & 1u));

    return STATUS_OK;
}

/* Split one byte into 8 bits and hide them, starting with the highest bit. */
static Status embed_byte(BMPImage *img, size_t *index, unsigned char value)
{
    int i;

    if (img == NULL || index == NULL || img->data == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }

    for (i = 7; i >= 0; i--) {
        unsigned char bit = (unsigned char)((value >> i) & 1u);
        Status result = embed_bit(img, *index, bit);

        if (result != STATUS_OK) {
            return result;
        }

        (*index)++;
    }

    return STATUS_OK;
}

/* Read one hidden bit from a pixel byte. */
static Status extract_bit(
    const BMPImage *img,
    size_t index,
    unsigned char *bit_out
)
{
    size_t offset;

    if (img == NULL || img->data == NULL || bit_out == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }

    if (index >= bmp_pixel_byte_count(img)) {
        return STATUS_CAPACITY_ERROR;
    }

    offset = bmp_pixel_file_offset(img, index);

    if (offset == SIZE_MAX || offset >= img->file_size) {
        return STATUS_INVALID_BMP;
    }

    *bit_out = (unsigned char)(img->data[offset] & 1u);
    return STATUS_OK;
}

/* Collect 8 hidden bits and rebuild one byte. */
static Status extract_byte(
    const BMPImage *img,
    size_t *index,
    unsigned char *value_out
)
{
    unsigned char value = 0;
    int i;

    if (img == NULL || index == NULL || value_out == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }

    for (i = 0; i < 8; i++) {
        unsigned char bit;
        Status result = extract_bit(img, *index, &bit);

        if (result != STATUS_OK) {
            return result;
        }

        value = (unsigned char)((value << 1) | bit);
        (*index)++;
    }

    *value_out = value;
    return STATUS_OK;
}

/* Hide a 32-bit number by hiding its four bytes. */
static Status embed_u32(BMPImage *img, size_t *index, uint32_t value)
{
    int i;

    if (img == NULL || index == NULL || img->data == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }

    for (i = 3; i >= 0; i--) {
        unsigned char byte = (unsigned char)((value >> (i * 8)) & 0xFFu);
        Status result = embed_byte(img, index, byte);

        if (result != STATUS_OK) {
            return result;
        }
    }

    return STATUS_OK;
}

/* Read four hidden bytes and rebuild a 32-bit number. */
static Status extract_u32(
    const BMPImage *img,
    size_t *index,
    uint32_t *value_out
)
{
    uint32_t value = 0;
    int i;

    if (img == NULL || index == NULL || value_out == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }

    for (i = 0; i < 4; i++) {
        unsigned char byte;
        Status result = extract_byte(img, index, &byte);

        if (result != STATUS_OK) {
            return result;
        }

        value = (value << 8) | byte;
    }

    *value_out = value;
    return STATUS_OK;
}

/* FNV-1a checksum: process each message byte to produce one 32-bit value. */
static uint32_t calculate_checksum(
    const unsigned char *message,
    size_t length
)
{
    uint32_t hash = 2166136261u;
    size_t i;

    for (i = 0; i < length; i++) {
        hash ^= message[i];
        hash *= 16777619u;
    }

    return hash;
}

size_t stego_max_message_size(const BMPImage *img)
{
    size_t total_pixel_bytes;

    if (img == NULL || img->data == NULL) {
        return 0;
    }

    total_pixel_bytes = bmp_pixel_byte_count(img);

    if (total_pixel_bytes < STEGO_HEADER_BITS) {
        return 0;
    }

    return (total_pixel_bytes - STEGO_HEADER_BITS) / 8;
}

Status stego_encode(
    BMPImage *img,
    const unsigned char *message,
    size_t message_len,
    const unsigned char *password,
    size_t password_len
)
{
    size_t index = 0;
    unsigned char *encrypted;
    uint32_t checksum;
    Status result;
    size_t i;

    if (img == NULL || img->data == NULL ||
        password == NULL || password_len == 0 ||
        password_len > MAX_PASSWORD_SIZE ||
        (message == NULL && message_len > 0)) {
        return STATUS_INVALID_ARGUMENT;
    }

    if (message_len > MAX_MESSAGE_SIZE ||
        message_len > stego_max_message_size(img)) {
        return STATUS_CAPACITY_ERROR;
    }

    encrypted = malloc(message_len > 0 ? message_len : 1);
    if (encrypted == NULL) {
        return STATUS_MEMORY_ERROR;
    }

    if (message_len > 0) {
        memcpy(encrypted, message, message_len);
    }

    /* Checksum is calculated from the original, unencrypted message. */
    checksum = calculate_checksum(message, message_len);

    /* Encrypt the copy, leaving the user's original message unchanged. */
    xor_crypt(encrypted, message_len, password, password_len);

    /* The first 96 hidden bits are the header: magic, length, checksum. */
    result = embed_u32(img, &index, STEGO_MAGIC);
    if (result != STATUS_OK) {
        free(encrypted);
        return result;
    }

    result = embed_u32(img, &index, (uint32_t)message_len);
    if (result != STATUS_OK) {
        free(encrypted);
        return result;
    }

    result = embed_u32(img, &index, checksum);
    if (result != STATUS_OK) {
        free(encrypted);
        return result;
    }

    /* Hide the encrypted message after the header. */
    for (i = 0; i < message_len; i++) {
        result = embed_byte(img, &index, encrypted[i]);

        if (result != STATUS_OK) {
            free(encrypted);
            return result;
        }
    }

    free(encrypted);
    return STATUS_OK;
}

Status stego_decode(
    const BMPImage *img,
    const unsigned char *password,
    size_t password_len,
    unsigned char **message_out,
    size_t *message_len_out
)
{
    size_t index = 0;
    uint32_t magic;
    uint32_t message_len;
    uint32_t stored_checksum;
    unsigned char *message;
    Status result;
    size_t i;

    if (img == NULL || img->data == NULL ||
        password == NULL || password_len == 0 ||
        password_len > MAX_PASSWORD_SIZE ||
        message_out == NULL || message_len_out == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }

    *message_out = NULL;
    *message_len_out = 0;

    result = extract_u32(img, &index, &magic);
    if (result != STATUS_OK) {
        return result;
    }

    if (magic != STEGO_MAGIC) {
        return STATUS_NO_MESSAGE;
    }

    result = extract_u32(img, &index, &message_len);
    if (result != STATUS_OK) {
        return result;
    }

    if (message_len > MAX_MESSAGE_SIZE ||
        message_len > stego_max_message_size(img)) {
        return STATUS_NO_MESSAGE;
    }

    result = extract_u32(img, &index, &stored_checksum);
    if (result != STATUS_OK) {
        return result;
    }

    message = malloc(message_len > 0 ? message_len : 1);
    if (message == NULL) {
        return STATUS_MEMORY_ERROR;
    }

    /* Extract the encrypted bytes. */
    for (i = 0; i < message_len; i++) {
        result = extract_byte(img, &index, &message[i]);

        if (result != STATUS_OK) {
            free(message);
            return result;
        }
    }

    /* The same XOR operation decrypts the bytes. */
    xor_crypt(message, message_len, password, password_len);

    if (calculate_checksum(message, message_len) != stored_checksum) {
        free(message);
        return STATUS_CHECKSUM_MISMATCH;
    }

    *message_out = message;
    *message_len_out = message_len;

    return STATUS_OK;
}
