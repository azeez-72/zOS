#pragma once

// Exact-width integer types
typedef unsigned char     uint8_t;
typedef unsigned short    uint16_t;
typedef unsigned int      uint32_t;
typedef unsigned long     uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

// Pointer-sized types (64-bit on RV64)
typedef uint64_t           uintptr_t;
typedef int64_t            intptr_t;

// Largest integer types
typedef uint64_t           uintmax_t;
typedef int64_t            intmax_t;

// size_t and ssize_t
typedef uint64_t           size_t;
typedef int64_t            ssize_t;

// Limits
#define UINT8_MAX   0xFFU
#define UINT16_MAX  0xFFFFU
#define UINT32_MAX  0xFFFFFFFFU
#define UINT64_MAX  0xFFFFFFFFFFFFFFFFULL

#define INT8_MAX    0x7F
#define INT16_MAX   0x7FFF
#define INT32_MAX   0x7FFFFFFF
#define INT64_MAX   0x7FFFFFFFFFFFFFFFLL

#define INT8_MIN    (-INT8_MAX  - 1)
#define INT16_MIN   (-INT16_MAX - 1)
#define INT32_MIN   (-INT32_MAX - 1)
#define INT64_MIN   (-INT64_MAX - 1)

// NULL
#ifndef NULL
#define NULL ((void*)0)
#endif