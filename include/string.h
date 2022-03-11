#pragma once

#include <stddef.h>  // size_t

size_t strlen(const char *s);
char* strchr(const char *s, int c);
char* strrchr(const char *s, int c);
char *strstr(const char *s1, const char *s2);
int strcmp(const char *p, const char *q);
int strncmp(const char *p, const char *q, size_t n);
char* strcpy(char *dst, const char *src);
char* strncpy(char *dst, const char *src, size_t n);
char *strcat(char *dst, const char *src);
char *strncat(char *dst, const char *src, size_t n);

char *strdup(const char *str);
char *strndup(const char *str, size_t size);

void* memcpy(void *dst, const void *src, size_t n);
void* memmove(void* dst, const void* src, size_t);
void* memset(void* buf, int val, size_t size);
int memcmp(const void *buf1, const void *buf2, size_t n);
