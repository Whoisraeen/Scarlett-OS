/**
 * @file compiler_builtins.c
 * @brief Compiler intrinsic functions for 128-bit arithmetic
 *
 * Provides __umodti3 and __udivti3 for 128-bit division operations
 * These are normally provided by libgcc but we implement them here for freestanding
 */

#include "../include/types.h"

typedef unsigned __int128 uint128_t;

/**
 * 128-bit unsigned division
 */
uint128_t __udivti3(uint128_t a, uint128_t b) {
    if (b == 0) {
        return 0;  // Division by zero
    }

    if (b > a) {
        return 0;
    }

    if (b == a) {
        return 1;
    }

    // Simple implementation using shift and subtract
    uint128_t quotient = 0;
    uint128_t remainder = 0;

    for (int i = 127; i >= 0; i--) {
        remainder <<= 1;
        remainder |= (a >> i) & 1;

        if (remainder >= b) {
            remainder -= b;
            quotient |= (uint128_t)1 << i;
        }
    }

    return quotient;
}

/**
 * 128-bit unsigned modulo
 */
uint128_t __umodti3(uint128_t a, uint128_t b) {
    if (b == 0) {
        return 0;  // Division by zero
    }

    if (b > a) {
        return a;
    }

    if (b == a) {
        return 0;
    }

    // Simple implementation using shift and subtract
    uint128_t remainder = 0;

    for (int i = 127; i >= 0; i--) {
        remainder <<= 1;
        remainder |= (a >> i) & 1;

        if (remainder >= b) {
            remainder -= b;
        }
    }

    return remainder;
}
