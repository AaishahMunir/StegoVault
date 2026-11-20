#include <stdio.h>
#include "bmp.h"

int main(int argc, char *argv[])
{
    const char *input_name = "input.bmp";
    BMPImage img = {0};
    Status result;

    if (argc > 1) {
        input_name = argv[1];
    }

    result = bmp_load(input_name, &img);

    if (result != STATUS_OK) {
        printf("Could not load %s. Status: %d\n", input_name, result);
        return 1;
    }

    printf("Width: %zu\n", img.width);
    printf("Height: %zu\n", img.height);
    printf("Usable pixel bytes: %zu\n", bmp_pixel_byte_count(&img));

    result = bmp_save("copy.bmp", &img);

    if (result == STATUS_OK) {
        printf("copy.bmp created.\n");
    } else {
        printf("Could not save copy.bmp. Status: %d\n", result);
    }

    bmp_free(&img);

    return result == STATUS_OK ? 0 : 1;
}
