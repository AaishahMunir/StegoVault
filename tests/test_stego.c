#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../stego.h"

/* Make a simple image-shaped block of memory for testing.
   It is not a real BMP file; it only gives the stego functions pixel data. */
static int make_test_image(BMPImage *img)
{
    memset(img, 0, sizeof(*img));

    img->width = 100;
    img->height = 100;
    img->pixel_offset = 54;
    img->pixel_bytes_per_row = 300;
    img->row_stride = 300;
    img->file_size = 54 + (300 * 100);

    img->data = calloc(img->file_size, sizeof(unsigned char));

    return img->data != NULL;
}

static void report_test(const char *name, int passed, int *failures)
{
    if (passed) {
        printf("%s: PASSED\n", name);
    } else {
        printf("%s: FAILED\n", name);
        (*failures)++;
    }
}

int main(void)
{
    BMPImage img = {0};
    BMPImage empty_img = {0};

    const unsigned char message[] = "Hello Stego!";
    const unsigned char password[] = "test123";
    const unsigned char wrong_password[] = "wrong456";

    size_t message_len = strlen((const char *)message);
    size_t password_len = strlen((const char *)password);

    unsigned char *decoded_message = NULL;
    size_t decoded_len = 0;
    Status result;
    int failures = 0;

    printf("Testing StegoVault...\n");

    if (!make_test_image(&img)) {
        printf("Could not allocate test image.\n");
        return 1;
    }

    /* Test 1: hide the message. */
    result = stego_encode(&img, message, message_len, password, password_len);
    report_test("Encoding", result == STATUS_OK, &failures);

    /* Test 2: recover the message. */
    result = stego_decode(
        &img, password, password_len, &decoded_message, &decoded_len
    );
    report_test("Decoding", result == STATUS_OK, &failures);

    /* Check both the length and the actual bytes. */
    if (result == STATUS_OK &&
        decoded_len == message_len &&
        memcmp(message, decoded_message, message_len) == 0) {
        report_test("Message comparison", 1, &failures);
        printf("Original: %s\n", message);
        printf("Decoded:  ");
        fwrite(decoded_message, 1, decoded_len, stdout);
        printf("\n");
    } else {
        report_test("Message comparison", 0, &failures);
    }

    /* Test 3: a message that is bigger than the image can hold. */
    {
        unsigned char large_message[4000];
        memset(large_message, 'A', sizeof(large_message));

        result = stego_encode(
            &img, large_message, sizeof(large_message), password, password_len
        );

        report_test("Capacity test", result == STATUS_CAPACITY_ERROR, &failures);
    }

    /* Test 4: an image that has no StegoVault message in it. */
    if (!make_test_image(&empty_img)) {
        printf("Could not allocate second test image.\n");
        free(decoded_message);
        bmp_free(&img);
        return 1;
    }

    {
        unsigned char *empty_decoded = NULL;
        size_t empty_decoded_len = 0;

        result = stego_decode(
            &empty_img, password, password_len,
            &empty_decoded, &empty_decoded_len
        );

        report_test(
            "No hidden message test",
            result == STATUS_NO_MESSAGE,
            &failures
        );

        free(empty_decoded);
    }

    /* Test 5: use a different password. */
    {
        unsigned char *wrong_decoded = NULL;
        size_t wrong_decoded_len = 0;

        result = stego_decode(
            &img,
            wrong_password,
            strlen((const char *)wrong_password),
            &wrong_decoded,
            &wrong_decoded_len
        );

        report_test(
            "Wrong password test",
            result == STATUS_CHECKSUM_MISMATCH,
            &failures
        );

        free(wrong_decoded);
    }

    /* Test 6: change one hidden message bit and try to decode again. */
    {
        size_t offset = bmp_pixel_file_offset(&img, STEGO_HEADER_BITS);
        unsigned char *changed_message = NULL;
        size_t changed_len = 0;

        if (offset != SIZE_MAX && offset < img.file_size) {
            img.data[offset] ^= 1u;
        }

        result = stego_decode(
            &img, password, password_len, &changed_message, &changed_len
        );

        report_test(
            "Modified message test",
            result == STATUS_CHECKSUM_MISMATCH,
            &failures
        );

        free(changed_message);
    }

    free(decoded_message);
    bmp_free(&empty_img);
    bmp_free(&img);

    if (failures == 0) {
        printf("\nAll tests passed.\n");
        return 0;
    }

    printf("\n%d test(s) failed.\n", failures);
    return 1;
}
