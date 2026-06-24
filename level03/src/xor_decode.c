#include <stdio.h>
#include <string.h>

int main(void)
{
    const unsigned char encrypted[] = "Q}|u`sfg~sf{}|a3";
    const char expected[] = "Congratulations!";
    const unsigned int test_constant = 0x1337d00d;
    char decrypted[sizeof(encrypted)];
    unsigned int key;
    size_t i;

    for (key = 0; key <= 0xff; key++)
    {
        for (i = 0; i < sizeof(encrypted) - 1; i++)
            decrypted[i] = encrypted[i] ^ key;
        decrypted[i] = '\0';
        if (strcmp(decrypted, expected) == 0)
        {
            printf("key: 0x%02x (%u)\n", key, key);
            printf("encrypted: %s\n", encrypted);
            printf("decrypted: %s\n", decrypted);
            printf("password: %u\n", test_constant - key);
            return 0;
        }
    }
    fprintf(stderr, "No matching XOR key found.\n");
    return 1;
}
