#ifndef VS_TEST_CHECK_H
#define VS_TEST_CHECK_H
#include <stdio.h>
#include <stdlib.h>
/* A failed regression is an ordinary nonzero exit, not an application crash. */
#undef assert
#define assert(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)
#endif
