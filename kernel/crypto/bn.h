/**
 * @file bn.h
 * @brief Big Number (Multi-precision integer) library
 */

#ifndef KERNEL_CRYPTO_BN_H
#define KERNEL_CRYPTO_BN_H

#include "../include/types.h"
#include "../include/errors.h"

// Big Number structure
// Represents a non-negative integer
typedef struct {
    uint64_t* words;    // Array of 64-bit words, little endian
    size_t size;        // Number of words allocated
    size_t top;         // Index of highest non-zero word + 1 (0 if value is 0)
    int sign;           // 1 for positive, -1 for negative (though we mostly use unsigned for crypto)
} bn_t;

// Initialize a big number
bn_t* bn_alloc(void);

// Free a big number
void bn_free(bn_t* bn);

// Copy a big number
error_code_t bn_copy(bn_t* dest, const bn_t* src);

// Initialize from integer
error_code_t bn_from_int(bn_t* bn, uint64_t value);

// Initialize from bytes (big endian)
error_code_t bn_from_bytes(bn_t* bn, const uint8_t* buf, size_t len);

// Export to bytes (big endian)
error_code_t bn_to_bytes(const bn_t* bn, uint8_t* buf, size_t len);

// Get number of significant bits
size_t bn_bit_count(const bn_t* bn);

// Get number of significant bytes
size_t bn_byte_count(const bn_t* bn);

// Basic arithmetic
error_code_t bn_add(bn_t* result, const bn_t* a, const bn_t* b);
error_code_t bn_sub(bn_t* result, const bn_t* a, const bn_t* b);
error_code_t bn_mul(bn_t* result, const bn_t* a, const bn_t* b);
error_code_t bn_div(bn_t* quotient, bn_t* remainder, const bn_t* a, const bn_t* b);
error_code_t bn_mod(bn_t* result, const bn_t* a, const bn_t* m);

// Comparison (-1 if a < b, 0 if a == b, 1 if a > b)
int bn_cmp(const bn_t* a, const bn_t* b);

// Modular arithmetic
error_code_t bn_mod_exp(bn_t* result, const bn_t* base, const bn_t* exp, const bn_t* mod);
error_code_t bn_mod_inv(bn_t* result, const bn_t* a, const bn_t* m);

// GCD
error_code_t bn_gcd(bn_t* result, const bn_t* a, const bn_t* b);

// Random generation
error_code_t bn_rand(bn_t* bn, size_t bits);

// Bitwise operations
error_code_t bn_lshift(bn_t* result, const bn_t* a, size_t bits);
error_code_t bn_rshift(bn_t* result, const bn_t* a, size_t bits);

// Primality testing (Miller-Rabin)
// rounds: number of rounds (e.g., 40)
bool bn_is_prime(const bn_t* bn, int rounds);

#endif // KERNEL_CRYPTO_BN_H
