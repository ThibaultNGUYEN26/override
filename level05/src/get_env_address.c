#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    char *address;
    ptrdiff_t path_adjustment;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s VARIABLE TARGET_PATH\n", argv[0]);
        return 1;
    }
    address = getenv(argv[1]);
    if (address == NULL)
    {
        fprintf(stderr, "%s is not defined.\n", argv[1]);
        return 1;
    }
    path_adjustment = (ptrdiff_t)strlen(argv[0]) -
        (ptrdiff_t)strlen(argv[2]);
    address += path_adjustment * 2;
    printf("%s: %p\n", argv[1], (void *)address);
    return 0;
}
