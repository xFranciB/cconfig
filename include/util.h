#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#define PACKED __attribute__((packed, aligned(1)))
#define UNUSED(x) ((void)x)
#define MEMBER_SIZE(s, m) (sizeof(((s*)0)->m))

#ifdef __GNUC__
    #define _bswap16(x) __builtin_bswap16(x)
    #define _bswap32(x) __builtin_bswap32(x)
#else
    #define _bswap16(x) ((x >> 8) | (x << 8))
    #define _bswap32(x) ((x >> 24) | ((x >> 8) & 0xFF00) | ((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000))
#endif

#define _bswap24(x) ((x >> 16) | (x & 0xFF00) | ((x << 16) & 0xFF0000))

#define  BS16(x) x = _bswap16(x)
#define  BS24(x) x = _bswap24(x)
#define  BS32(x) x = _bswap32(x)

#define CREATE_DA(T) \
typedef struct {                                                         \
	T *items;                                                            \
	size_t count;                                                        \
	size_t capacity;                                                     \
} T##_da;                                                                \
static inline void T##_da_init(T##_da *arr, long long initial_size) {    \
	assert(initial_size > 0);                                            \
	arr->capacity = initial_size;                                        \
	arr->count = 0;                                                      \
	arr->items = (T*)malloc(arr->capacity * sizeof(T));                  \
}                                                                        \
static inline void T##_da_append(T##_da *arr, T value) {                 \
	if (arr->capacity == arr->count) {                                   \
		arr->capacity *= 2;                                              \
		arr->items = (T*)realloc(arr->items, arr->capacity * sizeof(T)); \
	}                                                                    \
	arr->items[arr->count++] = value;                                    \
}                                                                        \
static inline void T##_da_free(T##_da *arr) {                            \
	free(arr->items);                                                    \
	arr->items = 0;                                                      \
	arr->count = 0;                                                      \
	arr->capacity = 0;                                                   \
}

#define ERROR_EXIT(...) do {           \
	fprintf(stderr, __VA_ARGS__);      \
	exit(EXIT_FAILURE);                \
} while (0)

#define ERROR_EXIT_ERRNO(...) do {            \
	fprintf(stderr, __VA_ARGS__);             \
	fprintf(stderr, ": %s\n", strerror(errno)); \
	exit(EXIT_FAILURE);                       \
} while (0)

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

// Not always true
typedef float f32;
typedef double f64;

#endif // UTILS_H
