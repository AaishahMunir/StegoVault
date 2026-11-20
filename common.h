#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define MAX_MESSAGE_SIZE 4096
#define MAX_PASSWORD_SIZE 128

#define STEGO_MAGIC UINT32_C(0x53545631)  /* "STV1" */
#define STEGO_HEADER_BITS 96              /* magic + length + checksum */

typedef enum {
    STATUS_OK,
    STATUS_INVALID_ARGUMENT,
    STATUS_FILE_ERROR,
    STATUS_MEMORY_ERROR,
    STATUS_INVALID_BMP,
    STATUS_UNSUPPORTED_BMP,
    STATUS_CAPACITY_ERROR,
    STATUS_NO_MESSAGE,
    STATUS_CHECKSUM_MISMATCH
} Status;

#endif
