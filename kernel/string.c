#include <stdint.h>

void *memset(void *s, int c, uint64_t n)
{
    uint8_t *p = s;
    while (n--)
        *p++ = c;
    return s;
}

/* ---------------------------------------------------------------------
 * Implementacoes abaixo foram adicionadas para dar suporte ao TreeFS
 * (M3). As assinaturas ja existiam em include/string.h, mas ainda nao
 * havia implementacao no kernel.
 * --------------------------------------------------------------------- */

void *memcpy(void *dest, const void *src, uint64_t n)
{
    uint8_t *d = dest;
    const uint8_t *s = src;
    while (n--)
        *d++ = *s++;
    return dest;
}

int strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b))
    {
        a++;
        b++;
    }
    return (unsigned char)(*a) - (unsigned char)(*b);
}

char *strcpy(char *dest, const char *src)
{
    char *ret = dest;
    while ((*dest++ = *src++))
        ;
    return ret;
}

uint64_t strlen(const char *s)
{
    uint64_t len = 0;
    while (s[len])
        len++;
    return len;
}