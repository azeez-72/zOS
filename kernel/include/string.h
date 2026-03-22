#pragma once
#include "stdint.h"

uint32_t  strlen(const char *s);
int       strcmp(const char *a, const char *b);
int       strncmp(const char *a, const char *b, uint32_t n);
char     *strchr(const char *s, char c);
char     *strcpy(char *dest, const char *src);
char     *strncpy(char *dest, const char *src, uint32_t n);
int       atoi(const char *s);
int       memcmp(const void *a, const void *b, uint32_t n);
void     *memmove(void *dest, const void *src, uint32_t n);
long      strtol(const char *s, char **endp, int base);
char    *strcat(char *dest, const char *src);
char    *strncat(char *dest, const char *src, uint32_t n);
char    *strsep(char **str, const char *delim);