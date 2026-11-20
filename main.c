#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "bmp.h"
#include "stego.h"

static int read_line(const char *prompt, char *buffer, size_t size)
{
    size_t len;
    int ch;

    printf("%s", prompt);

    if (fgets(buffer, (int)size, stdin) == NULL) {
        return 0;
    }

    len = strlen(buffer);

    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[--len] = '\0';
    }
    else if (!feof(stdin)) {
        ch = getchar();

        if (ch != '\n' && ch != EOF) {
            while (ch != '\n' && ch != EOF) {
                ch = getchar();
            }

            return -1;
        }
    }

    if (len > 0 && buffer[len - 1] == '\r') {
        buffer[--len] = '\0';
    }

    return 1;
}

static const char *get_status_message(Status status)
{
    switch (status) {
        case STATUS_OK:
            return "Operation successful.";

        case STATUS_INVALID_ARGUMENT:
            return "Invalid input argument.";

        case STATUS_FILE_ERROR:
            return "Unable to access the file.";

        case STATUS_MEMORY_ERROR:
            return "Memory allocation failed.";

        case STATUS_INVALID_BMP:
            return "Invalid BMP image.";

        case STATUS_UNSUPPORTED_BMP:
            return "Unsupported BMP format. Use 24-bit uncompressed BMP.";

        case STATUS_CAPACITY_ERROR:
            return "Message is too large for this image.";

        case STATUS_NO_MESSAGE:
            return "No hidden message found.";

        case STATUS_CHECKSUM_MISMATCH:
            return "Incorrect password or corrupted message.";

        default:
            return "Unknown error.";
    }
}

static void encode_message(void)
{
    BMPImage img = {0};

    char input_path[512];
    char output_path[512];
    char message[MAX_MESSAGE_SIZE + 1];
    char password[MAX_PASSWORD_SIZE + 1];

    size_t message_len;
    size_t password_len;
    size_t capacity;

    Status status;
    int result;

    printf("\n========== ENCODE MESSAGE ==========\n");

    result = read_line("Enter input BMP filename: ",
                       input_path, sizeof(input_path));

    if (result != 1 || input_path[0] == '\0') {
        printf("Invalid input filename.\n");
        return;
    }

    result = read_line("Enter output BMP filename: ",
                       output_path, sizeof(output_path));

    if (result != 1 || output_path[0] == '\0') {
        printf("Invalid output filename.\n");
        return;
    }

    status = bmp_load(input_path, &img);

    if (status != STATUS_OK) {
        printf("Error: %s\n", get_status_message(status));
        return;
    }

    capacity = stego_max_message_size(&img);

    printf("Maximum message capacity: %zu bytes\n", capacity);

    result = read_line("Enter your secret message: ",
                       message, sizeof(message));

    if (result != 1) {
        printf("Invalid or oversized message.\n");
        bmp_free(&img);
        return;
    }

    result = read_line("Enter encryption password: ",
                       password, sizeof(password));

    if (result != 1 || password[0] == '\0') {
        printf("Password cannot be empty or oversized.\n");
        bmp_free(&img);
        return;
    }

    message_len = strlen(message);
    password_len = strlen(password);

    if (message_len > MAX_MESSAGE_SIZE ||
        message_len > capacity) {
        printf("Message exceeds available image capacity.\n");
        bmp_free(&img);
        return;
    }

    status = stego_encode(
        &img,
        (const unsigned char *)message,
        message_len,
        (const unsigned char *)password,
        password_len
    );

    if (status != STATUS_OK) {
        printf("Encoding failed: %s\n",
               get_status_message(status));

        bmp_free(&img);
        return;
    }

    status = bmp_save(output_path, &img);

    if (status == STATUS_OK) {
        printf("\nMessage encrypted and hidden successfully!\n");
        printf("Output image: %s\n", output_path);
    }
    else {
        printf("Saving failed: %s\n",
               get_status_message(status));
    }

    bmp_free(&img);
}

static void decode_message(void)
{
    BMPImage img = {0};

    char input_path[512];
    char password[MAX_PASSWORD_SIZE + 1];

    unsigned char *message = NULL;
    size_t message_len = 0;
    size_t password_len;

    Status status;
    int result;

    printf("\n========== DECODE MESSAGE ==========\n");

    result = read_line("Enter BMP filename: ",
                       input_path, sizeof(input_path));

    if (result != 1 || input_path[0] == '\0') {
        printf("Invalid filename.\n");
        return;
    }

    status = bmp_load(input_path, &img);

    if (status != STATUS_OK) {
        printf("Error: %s\n", get_status_message(status));
        return;
    }

    result = read_line("Enter decryption password: ",
                       password, sizeof(password));

    if (result != 1 || password[0] == '\0') {
        printf("Password cannot be empty or oversized.\n");
        bmp_free(&img);
        return;
    }

    password_len = strlen(password);

    status = stego_decode(
        &img,
        (const unsigned char *)password,
        password_len,
        &message,
        &message_len
    );

    if (status == STATUS_OK) {
        printf("\nHidden message recovered successfully!\n");
        printf("Message: ");

        fwrite(message, 1, message_len, stdout);

        printf("\n");
    }
    else {
        printf("Decoding failed: %s\n",
               get_status_message(status));
    }

    free(message);
    bmp_free(&img);
}

int main(void)
{
    char choice[16];
    int result;

    printf("====================================\n");
    printf("          STEGOVAULT\n");
    printf("   Secure Image Steganography\n");
    printf("====================================\n");

    while (1) {

        printf("\n1. Encode / Hide Message\n");
        printf("2. Decode / Extract Message\n");
        printf("3. Exit\n");

        printf("------------------------------------\n");

        result = read_line("Enter your choice: ",
                           choice, sizeof(choice));

        if (result == 0) {
            break;
        }

        if (result != 1) {
            printf("Invalid choice.\n");
            continue;
        }

        if (strcmp(choice, "1") == 0) {
            encode_message();
        }
        else if (strcmp(choice, "2") == 0) {
            decode_message();
        }
        else if (strcmp(choice, "3") == 0) {
            printf("\nThank you for using StegoVault!\n");
            break;
        }
        else {
            printf("Invalid choice. Please select 1, 2 or 3.\n");
        }
    }

    return 0;
}
