#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    const unsigned char *login;
    uint32_t serial;
    size_t length;
    size_t i;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s LOGIN\n", argv[0]);
        return 1;
    }
    login = (const unsigned char *)argv[1];
    length = strlen(argv[1]);
    if (length <= 5 || length > 31)
    {
        fprintf(stderr, "Login length must be between 6 and 31 characters.\n");
        return 1;
    }
    serial = (login[3] ^ 0x1337U) + 0x5eededU;
    for (i = 0; i < length; i++)
    {
        if (login[i] <= 0x1f)
        {
            fprintf(stderr, "Login contains a non-printable character.\n");
            return 1;
        }
        serial += (login[i] ^ serial) % 0x539U;
    }
    printf("login: %s\n", login);
    printf("serial: %u\n", serial);
    return 0;
}
