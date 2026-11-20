#include "crypto.h"

/*
   XOR is used for both encryption and decryption.
   Applying the same password a second time restores the original bytes.
*/
void xor_crypt(
    unsigned char *data,
    size_t data_len,
    const unsigned char *password,
    size_t password_len
)
{
    size_t i;

    if (data == NULL || password == NULL || password_len == 0) {
        return;
    }

    for (i = 0; i < data_len; i++) {
        data[i] = data[i] ^ password[i % password_len];
    }
}
