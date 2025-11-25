/**
 * @file bn.c
 * @brief Big Number implementation
 */

#include "bn.h"
#include "../include/mm/heap.h"
#include "../include/string.h"
#include "../include/crypto/crypto.h" // For RNG

#define BN_DEFAULT_CAPACITY 32 // Initial capacity in words

// Helper: Ensure capacity
static error_code_t bn_ensure(bn_t* bn, size_t words) {
    if (bn->size >= words) return ERR_OK;
    
    size_t new_size = words > bn->size * 2 ? words : bn->size * 2;
    if (new_size < BN_DEFAULT_CAPACITY) new_size = BN_DEFAULT_CAPACITY;
    
    uint64_t* new_words = (uint64_t*)kmalloc(new_size * sizeof(uint64_t));
    if (!new_words) return ERR_OUT_OF_MEMORY;
    
    memset(new_words, 0, new_size * sizeof(uint64_t));
    if (bn->words) {
        memcpy(new_words, bn->words, bn->size * sizeof(uint64_t));
        kfree(bn->words);
    }
    
    bn->words = new_words;
    bn->size = new_size;
    return ERR_OK;
}

// Helper: Update top
static void bn_fix_top(bn_t* bn) {
    size_t i = bn->size;
    while (i > 0 && bn->words[i - 1] == 0) {
        i--;
    }
    bn->top = i;
}

bn_t* bn_alloc(void) {
    bn_t* bn = (bn_t*)kmalloc(sizeof(bn_t));
    if (!bn) return NULL;
    
    bn->words = (uint64_t*)kmalloc(BN_DEFAULT_CAPACITY * sizeof(uint64_t));
    if (!bn->words) {
        kfree(bn);
        return NULL;
    }
    
    memset(bn->words, 0, BN_DEFAULT_CAPACITY * sizeof(uint64_t));
    bn->size = BN_DEFAULT_CAPACITY;
    bn->top = 0;
    bn->sign = 1;
    return bn;
}

void bn_free(bn_t* bn) {
    if (!bn) return;
    if (bn->words) kfree(bn->words);
    kfree(bn);
}

error_code_t bn_copy(bn_t* dest, const bn_t* src) {
    if (!dest || !src) return ERR_INVALID_ARG;
    
    error_code_t err = bn_ensure(dest, src->top);
    if (err != ERR_OK) return err;
    
    memset(dest->words, 0, dest->size * sizeof(uint64_t));
    memcpy(dest->words, src->words, src->top * sizeof(uint64_t));
    dest->top = src->top;
    dest->sign = src->sign;
    return ERR_OK;
}

error_code_t bn_from_int(bn_t* bn, uint64_t value) {
    if (!bn) return ERR_INVALID_ARG;
    
    error_code_t err = bn_ensure(bn, 1);
    if (err != ERR_OK) return err;
    
    memset(bn->words, 0, bn->size * sizeof(uint64_t));
    bn->words[0] = value;
    bn->top = (value == 0) ? 0 : 1;
    bn->sign = 1;
    return ERR_OK;
}

error_code_t bn_from_bytes(bn_t* bn, const uint8_t* buf, size_t len) {
    if (!bn || !buf) return ERR_INVALID_ARG;
    
    // Calculate words needed
    size_t words_needed = (len + 7) / 8;
    error_code_t err = bn_ensure(bn, words_needed);
    if (err != ERR_OK) return err;
    
    memset(bn->words, 0, bn->size * sizeof(uint64_t));
    
    // Import big endian bytes
    for (size_t i = 0; i < len; i++) {
        size_t word_idx = (len - 1 - i) / 8;
        size_t byte_idx = (len - 1 - i) % 8;
        bn->words[word_idx] |= ((uint64_t)buf[i]) << (byte_idx * 8);
    }
    
    bn_fix_top(bn);
    bn->sign = 1;
    return ERR_OK;
}

error_code_t bn_to_bytes(const bn_t* bn, uint8_t* buf, size_t len) {
    if (!bn || !buf) return ERR_INVALID_ARG;
    
    memset(buf, 0, len);
    
    // Export to big endian
    size_t bytes_count = bn_byte_count(bn);
    if (bytes_count > len) bytes_count = len; // Truncate if buffer too small
    
    for (size_t i = 0; i < bytes_count; i++) {
        size_t word_idx = i / 8;
        size_t byte_idx = i % 8;
        uint8_t byte = (bn->words[word_idx] >> (byte_idx * 8)) & 0xFF;
        buf[len - 1 - i] = byte;
    }
    
    return ERR_OK;
}

size_t bn_bit_count(const bn_t* bn) {
    if (bn->top == 0) return 0;
    
    size_t bits = (bn->top - 1) * 64;
    uint64_t top_word = bn->words[bn->top - 1];
    
    while (top_word) {
        bits++;
        top_word >>= 1;
    }
    
    return bits;
}

size_t bn_byte_count(const bn_t* bn) {
    return (bn_bit_count(bn) + 7) / 8;
}

int bn_cmp(const bn_t* a, const bn_t* b) {
    if (a->top > b->top) return 1;
    if (a->top < b->top) return -1;
    
    for (size_t i = a->top; i > 0; i--) {
        if (a->words[i - 1] > b->words[i - 1]) return 1;
        if (a->words[i - 1] < b->words[i - 1]) return -1;
    }
    
    return 0;
}

error_code_t bn_add(bn_t* result, const bn_t* a, const bn_t* b) {
    size_t max_len = (a->top > b->top ? a->top : b->top) + 1;
    error_code_t err = bn_ensure(result, max_len);
    if (err != ERR_OK) return err;
    
    uint64_t carry = 0;
    for (size_t i = 0; i < max_len; i++) {
        uint64_t wa = (i < a->top) ? a->words[i] : 0;
        uint64_t wb = (i < b->top) ? b->words[i] : 0;
        
        uint64_t sum = wa + wb + carry;
        result->words[i] = sum;
        
        // Check for overflow
        carry = (sum < wa) || (carry && sum == wa);
    }
    
    bn_fix_top(result);
    return ERR_OK;
}

error_code_t bn_sub(bn_t* result, const bn_t* a, const bn_t* b) {
    // Assume a >= b for now (unsigned arithmetic)
    if (bn_cmp(a, b) < 0) return ERR_INVALID_ARG; // Result would be negative
    
    error_code_t err = bn_ensure(result, a->top);
    if (err != ERR_OK) return err;
    
    uint64_t borrow = 0;
    for (size_t i = 0; i < a->top; i++) {
        uint64_t wa = a->words[i];
        uint64_t wb = (i < b->top) ? b->words[i] : 0;
        
        uint64_t diff = wa - wb - borrow;
        result->words[i] = diff;
        
        // Check for underflow
        borrow = (wa < wb + borrow) || (borrow && wb == UINT64_MAX); 
    }
    
    // Clear remaining words if result was re-used
    if (result->size > a->top) {
        memset(result->words + a->top, 0, (result->size - a->top) * sizeof(uint64_t));
    }
    
    bn_fix_top(result);
    return ERR_OK;
}

error_code_t bn_mul(bn_t* result, const bn_t* a, const bn_t* b) {
    bn_t* res = bn_alloc();
    if (!res) return ERR_OUT_OF_MEMORY;
    
    error_code_t err = bn_ensure(res, a->top + b->top);
    if (err != ERR_OK) {
        bn_free(res);
        return err;
    }
    
    // Initialize result to 0
    memset(res->words, 0, res->size * sizeof(uint64_t));
    
    for (size_t i = 0; i < a->top; i++) {
        uint64_t carry = 0;
        for (size_t j = 0; j < b->top; j++) {
            // Using 128-bit arithmetic: (carry, product) = a[i] * b[j] + res[i+j] + carry
            unsigned __int128 prod = (unsigned __int128)a->words[i] * b->words[j];
            prod += res->words[i + j];
            prod += carry;
            
            res->words[i + j] = (uint64_t)prod;
            carry = (uint64_t)(prod >> 64);
        }
        res->words[i + b->top] += carry;
    }
    
    bn_fix_top(res);
    bn_copy(result, res);
    bn_free(res);
    return ERR_OK;
}

error_code_t bn_lshift(bn_t* result, const bn_t* a, size_t bits) {
    if (bits == 0) return bn_copy(result, a);
    
    size_t word_shift = bits / 64;
    size_t bit_shift = bits % 64;
    
    bn_t* res = bn_alloc();
    if (!res) return ERR_OUT_OF_MEMORY;
    
    error_code_t err = bn_ensure(res, a->top + word_shift + 1);
    if (err != ERR_OK) {
        bn_free(res);
        return err;
    }
    
    memset(res->words, 0, res->size * sizeof(uint64_t));
    
    if (bit_shift == 0) {
        for (size_t i = 0; i < a->top; i++) {
            res->words[i + word_shift] = a->words[i];
        }
    } else {
        uint64_t carry = 0;
        for (size_t i = 0; i < a->top; i++) {
            res->words[i + word_shift] = (a->words[i] << bit_shift) | carry;
            carry = a->words[i] >> (64 - bit_shift);
        }
        res->words[a->top + word_shift] = carry;
    }
    
    bn_fix_top(res);
    bn_copy(result, res);
    bn_free(res);
    return ERR_OK;
}

error_code_t bn_rshift(bn_t* result, const bn_t* a, size_t bits) {
    if (bits == 0) return bn_copy(result, a);
    
    size_t word_shift = bits / 64;
    size_t bit_shift = bits % 64;
    
    if (word_shift >= a->top) {
        // Shifted everything out
        return bn_from_int(result, 0);
    }
    
    bn_t* res = bn_alloc();
    if (!res) return ERR_OUT_OF_MEMORY;
    
    error_code_t err = bn_ensure(res, a->top - word_shift);
    if (err != ERR_OK) {
        bn_free(res);
        return err;
    }
    
    memset(res->words, 0, res->size * sizeof(uint64_t));
    
    if (bit_shift == 0) {
        for (size_t i = word_shift; i < a->top; i++) {
            res->words[i - word_shift] = a->words[i];
        }
    } else {
        uint64_t carry = 0;
        for (size_t i = a->top; i > word_shift; i--) {
            size_t idx = i - 1;
            uint64_t val = a->words[idx];
            res->words[idx - word_shift] = (val >> bit_shift) | carry;
            carry = val << (64 - bit_shift);
        }
    }
    
    bn_fix_top(res);
    bn_copy(result, res);
    bn_free(res);
    return ERR_OK;
}

// Division (Knuth Algorithm D)
error_code_t bn_div(bn_t* quotient, bn_t* remainder, const bn_t* a, const bn_t* b) {
    if (b->top == 0) return ERR_INVALID_ARG; // Divide by zero
    
    if (bn_cmp(a, b) < 0) {
        if (quotient) bn_from_int(quotient, 0);
        if (remainder) bn_copy(remainder, a);
        return ERR_OK;
    }
    
    // Simple case: divisor is one word
    if (b->top == 1) {
        uint64_t divisor = b->words[0];
        uint64_t rem = 0;
        
        bn_t* q = NULL;
        if (quotient) {
            q = bn_alloc();
            bn_ensure(q, a->top);
            q->top = a->top;
        }
        
        for (size_t i = a->top; i > 0; i--) {
            unsigned __int128 val = ((unsigned __int128)rem << 64) | a->words[i - 1];
            if (quotient) q->words[i - 1] = (uint64_t)(val / divisor);
            rem = (uint64_t)(val % divisor);
        }
        
        if (quotient) {
            bn_fix_top(q);
            bn_copy(quotient, q);
            bn_free(q);
        }
        
        if (remainder) {
            bn_from_int(remainder, rem);
        }
        return ERR_OK;
    }
    
    // Full division not implemented here to keep it relatively short,
    // but standard Knuth Algo D should be here for a real advanced implementation.
    // For this task, since we need Modular Exponentiation mostly, and we can implement
    // bitwise reduction for modulo if we don't need full division often?
    // No, ModExp needs Mod, which needs Div.
    
    // Implementation of bitwise long division
    bn_t* q = bn_alloc();
    bn_t* r = bn_alloc();
    bn_t* temp_b = bn_alloc();
    
    bn_from_int(q, 0);
    bn_from_int(r, 0);
    
    // Iterate from MSB of a
    size_t n_bits = bn_bit_count(a);
    
    for (size_t i = n_bits; i > 0; i--) {
        // r = r << 1
        bn_lshift(r, r, 1);
        
        // r(0) = a(i-1)
        size_t word_idx = (i - 1) / 64;
        size_t bit_idx = (i - 1) % 64;
        if (a->words[word_idx] & (1ULL << bit_idx)) {
            r->words[0] |= 1;
        }
        
        // if r >= b
        if (bn_cmp(r, b) >= 0) {
            bn_sub(r, r, b);
            // q(i-1) = 1
            // Set bit in q
            size_t qw = (i - 1) / 64;
            size_t qb = (i - 1) % 64;
            if (q->size <= qw) bn_ensure(q, qw + 1);
            q->words[qw] |= (1ULL << qb);
            if (qw + 1 > q->top) q->top = qw + 1;
        }
    }
    
    if (quotient) bn_copy(quotient, q);
    if (remainder) bn_copy(remainder, r);
    
    bn_free(q);
    bn_free(r);
    bn_free(temp_b);
    
    return ERR_OK;
}

error_code_t bn_mod(bn_t* result, const bn_t* a, const bn_t* m) {
    return bn_div(NULL, result, a, m);
}

// Modular Exponentiation: result = base^exp % mod
error_code_t bn_mod_exp(bn_t* result, const bn_t* base, const bn_t* exp, const bn_t* mod) {
    bn_t* r = bn_alloc();
    bn_t* b = bn_alloc();
    
    bn_from_int(r, 1);
    bn_copy(b, base);
    
    size_t bits = bn_bit_count(exp);
    
    for (size_t i = 0; i < bits; i++) {
        // Check if bit i is set
        size_t word_idx = i / 64;
        size_t bit_idx = i % 64;
        
        if (exp->words[word_idx] & (1ULL << bit_idx)) {
            // r = (r * b) % mod
            bn_mul(r, r, b);
            bn_mod(r, r, mod);
        }
        
        // b = (b * b) % mod
        bn_mul(b, b, b);
        bn_mod(b, b, mod);
    }
    
    bn_copy(result, r);
    bn_free(r);
    bn_free(b);
    return ERR_OK;
}

// Extended Euclidean for Modular Inverse
error_code_t bn_mod_inv(bn_t* result, const bn_t* a, const bn_t* m) {
    bn_t* t = bn_alloc();
    bn_t* newt = bn_alloc();
    bn_t* r = bn_alloc();
    bn_t* newr = bn_alloc();
    bn_t* q = bn_alloc();
    bn_t* temp = bn_alloc();
    bn_t* term = bn_alloc();
    bn_t* diff = bn_alloc();
    
    // t := 0; newt := 1
    bn_from_int(t, 0);
    bn_from_int(newt, 1);
    
    // r := m; newr := a
    bn_copy(r, m);
    bn_copy(newr, a);
    
    while (newr->top > 0) { // while newr != 0
        // quotient := r div newr
        bn_div(q, NULL, r, newr);
        
        // (t, newt) := (newt, t - quotient * newt)
        // Calculation performed modulo m to keep numbers positive
        
        // Calculate term = (quotient * newt) % m
        bn_mul(term, q, newt);
        bn_mod(term, term, m);
        
        bn_copy(temp, t); // temp = old t
        bn_copy(t, newt); // t = old newt
        
        // newt = (temp - term) mod m
        if (bn_cmp(temp, term) < 0) {
            // temp < term, so result is negative
            // result = temp - term + m = m - (term - temp)
            bn_sub(diff, term, temp);
            bn_sub(newt, m, diff);
        } else {
            bn_sub(newt, temp, term);
        }
        
        // (r, newr) := (newr, r - quotient * newr)
        // Standard euclidean step
        bn_mul(term, q, newr);
        bn_sub(diff, r, term); // r - q*newr (should be remainder)
        
        bn_copy(r, newr);
        bn_copy(newr, diff);
    }
    
    // if r > 1 then a is not invertible
    if (r->top > 1 || (r->top == 1 && r->words[0] > 1)) {
        bn_free(t); bn_free(newt); bn_free(r); bn_free(newr);
        bn_free(q); bn_free(temp); bn_free(term); bn_free(diff);
        return ERR_INVALID_ARG; // Not invertible
    }
    
    bn_copy(result, t);
    
    bn_free(t); bn_free(newt); bn_free(r); bn_free(newr);
    bn_free(q); bn_free(temp); bn_free(term); bn_free(diff);
    return ERR_OK;
}

error_code_t bn_gcd(bn_t* result, const bn_t* a, const bn_t* b) {
    bn_t* t1 = bn_alloc();
    bn_t* t2 = bn_alloc();
    bn_t* temp = bn_alloc();
    
    bn_copy(t1, a);
    bn_copy(t2, b);
    
    while (t2->top > 0) {
        bn_mod(temp, t1, t2);
        bn_copy(t1, t2);
        bn_copy(t2, temp);
    }
    
    bn_copy(result, t1);
    
    bn_free(t1);
    bn_free(t2);
    bn_free(temp);
    return ERR_OK;
}

// Random number generation
error_code_t bn_rand(bn_t* bn, size_t bits) {
    size_t bytes = (bits + 7) / 8;
    uint8_t* buf = (uint8_t*)kmalloc(bytes);
    if (!buf) return ERR_OUT_OF_MEMORY;
    
    crypto_random_bytes(buf, bytes);
    
    // Mask excess bits
    if (bits % 8) {
        buf[0] &= (1 << (bits % 8)) - 1;
    }
    
    bn_from_bytes(bn, buf, bytes);
    kfree(buf);
    return ERR_OK;
}

// Miller-Rabin primality test
bool bn_is_prime(const bn_t* bn, int rounds) {
    if (bn->top == 0) return false;
    if (bn->top == 1 && bn->words[0] < 2) return false;
    if (bn->top == 1 && bn->words[0] == 2) return true;
    
    // Even numbers > 2 are composite
    if ((bn->words[0] & 1) == 0) return false;
    
    // n_minus_1 = bn - 1
    bn_t* n_minus_1 = bn_alloc();
    bn_t* one = bn_alloc();
    bn_from_int(one, 1);
    bn_sub(n_minus_1, bn, one);
    
    // d = n - 1, s = 0
    bn_t* d = bn_alloc();
    bn_copy(d, n_minus_1);
    int s = 0;
    
    while ((d->words[0] & 1) == 0) {
        bn_rshift(d, d, 1);
        s++;
    }
    
    bn_t* a = bn_alloc();
    bn_t* x = bn_alloc();
    bn_t* temp = bn_alloc(); // for intermediate calculations
    
    bool is_prime = true;
    
    for (int i = 0; i < rounds; i++) {
        // a = rand(2, n-2) -> roughly just rand(bits) % (n-3) + 2
        // For simplicity just rand(bits) % n. If 0 or 1, try again.
        
        do {
            bn_rand(a, bn_bit_count(bn));
            bn_mod(a, a, bn);
        } while (a->top == 0 || (a->top == 1 && a->words[0] <= 1));
        
        // x = a^d % n
        bn_mod_exp(x, a, d, bn);
        
        // if x == 1 or x == n-1
        if ((x->top == 1 && x->words[0] == 1) || bn_cmp(x, n_minus_1) == 0) {
            continue;
        }
        
        bool composite = true;
        for (int r = 1; r < s; r++) {
            // x = x^2 % n
            bn_mul(x, x, x);
            bn_mod(x, x, bn);
            
            if (bn_cmp(x, n_minus_1) == 0) {
                composite = false;
                break;
            }
        }
        
        if (composite) {
            is_prime = false;
            break;
        }
    }
    
    bn_free(n_minus_1);
    bn_free(one);
    bn_free(d);
    bn_free(a);
    bn_free(x);
    bn_free(temp);
    
    return is_prime;
}
