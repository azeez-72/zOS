#include <string.h>
#include <stdint.h>


uint32_t strlen(const char *s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}


int strcmp(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return (unsigned char)*a - (unsigned char)*b;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}


int strncmp(const char *a, const char *b, uint32_t n) {
    if (n == 0) return 0;
    while (n-- > 0) {
        if (*a != *b) return (unsigned char)*a - (unsigned char)*b;
        if (*a == 0)  return 0;
        a++; b++;
    }
    return 0;
}


char *strchr(const char *s, char c) {
    while (*s) {
        if (*s == c) return (char *)s;
        s++;
    }
    return 0;
}


char *strcpy(char *dest, const char *src) {
    char *ret = dest;
    while ((*dest++ = *src++) != 0);
    return ret;
}


char *strncpy(char *dest, const char *src, uint32_t n) {
    if (n == 0) return dest;
    char *ret = dest;
    while (n-- > 0) {
        if ((*dest++ = *src++) == 0) {
            while (n-- > 0) *dest++ = 0;  // pad with nulls
            break;
        }
    }
    return ret;
}


int atoi(const char *s) {
    int n    = 0;
    int sign = 1;

    // skip whitespace
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;

    // handle sign
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    // convert digits
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }

    return sign * n;
}


int memcmp(const void *a, const void *b, uint32_t n) {
    const unsigned char *p1 = (const unsigned char *)a;
    const unsigned char *p2 = (const unsigned char *)b;
    while (n-- > 0) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}


void *memmove(void *dest, const void *src, uint32_t n) {
    unsigned char       *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d <= s) {
        // copy forward — dest is before src, no overlap issue
        while (n-- > 0) *d++ = *s++;
    } else {
        // copy backward — dest is after src, avoid clobbering src
        d += n;
        s += n;
        while (n-- > 0) *--d = *--s;
    }
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *ret = dest;
    while (*dest) dest++;   // find end of dest
    while ((*dest++ = *src++) != 0);  // copy src
    return ret;
}


char *strncat(char *dest, const char *src, uint32_t n) {
    if (n == 0) return dest;
    char *ret = dest;
    while (*dest) dest++;       // find end of dest
    while (n-- > 0) {
        if ((*dest++ = *src++) == 0)
            return ret;         // src ended, already null terminated
    }
    *dest = '\0';               // null terminate after n chars
    return ret;
}


char *strsep(char **str, const char *delim) {
    if (*str == 0) return 0;    // nothing left

    char *start = *str;
    char *p     = start;

    while (*p != '\0') {
        // check if current char is a delimiter
        const char *d = delim;
        while (*d != '\0') {
            if (*p == *d) {
                *p   = '\0';    // replace delimiter with null terminator
                *str = p + 1;   // advance str past delimiter
                return start;   // return token
            }
            d++;
        }
        p++;
    }

    // reached end of string — last token
    *str = 0;       // signal no more tokens
    return start;
}


long strtol(const char *s, char **endp, int base) {
    long acc  = 0;
    int  sign = 1;

    // skip whitespace
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;

    // handle sign
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    // handle 0x prefix for hex
    if ((base == 0 || base == 16) &&
        *s == '0' && (s[1] == 'x' || s[1] == 'X')) {
        if (base == 0) base = 16;
        s += 2;
    }

    if (base == 0) base = 10;

    // convert digits
    while (*s) {
        int digit;
        if      (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
        else break;

        if (digit >= base) break;

        acc = acc * base + digit;
        s++;
    }

    if (endp) *endp = (char *)s;
    return sign * acc;
}