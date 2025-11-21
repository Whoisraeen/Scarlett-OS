/**
 * @file sha256.c
 * @brief SHA-256 hash implementation
 */

#include "../include/crypto/crypto.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

// SHA-256 context
typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t buffer[64];
    uint32_t buffer_len;
} sha256_context_t;

// SHA-256 constants
static const uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x6f1c0e5b,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// Right rotate
static inline uint32_t rotr(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

// SHA-256 functions
static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

static inline uint32_t sigma0(uint32_t x) {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

static inline uint32_t sigma1(uint32_t x) {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

static inline uint32_t gamma0(uint32_t x) {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

static inline uint32_t gamma1(uint32_t x) {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

// Process 512-bit block
static void sha256_process_block(sha256_context_t* ctx, const uint8_t* block) {
    uint32_t w[64];
    
    // Copy block into w[0..15]
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }
    
    // Extend to w[16..63]
    for (int i = 16; i < 64; i++) {
        w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];
    }
    
    // Initialize working variables
    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];
    
    // Main loop
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + sigma1(e) + ch(e, f, g) + k[i] + w[i];
        uint32_t t2 = sigma0(a) + maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    // Add to state
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

/**
 * Initialize SHA-256 context
 */
error_code_t sha256_init(void** context) {
    if (!context) {
        return ERR_INVALID_ARG;
    }
    
    sha256_context_t* ctx = (sha256_context_t*)kmalloc(sizeof(sha256_context_t));
    if (!ctx) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Initial hash values
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->bit_count = 0;
    ctx->buffer_len = 0;
    
    *context = ctx;
    return ERR_OK;
}

/**
 * Update SHA-256 with more data
 */
error_code_t sha256_update(void* context, const uint8_t* data, size_t data_len) {
    if (!context || !data) {
        return ERR_INVALID_ARG;
    }
    
    sha256_context_t* ctx = (sha256_context_t*)context;
    size_t offset = 0;
    
    while (offset < data_len) {
        size_t space = 64 - ctx->buffer_len;
        size_t copy = (data_len - offset < space) ? (data_len - offset) : space;
        
        memcpy(ctx->buffer + ctx->buffer_len, data + offset, copy);
        ctx->buffer_len += copy;
        offset += copy;
        ctx->bit_count += copy * 8;
        
        if (ctx->buffer_len == 64) {
            sha256_process_block(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
    
    return ERR_OK;
}

/**
 * Finalize SHA-256 and get hash
 */
error_code_t sha256_final(void* context, uint8_t* hash_output) {
    if (!context || !hash_output) {
        return ERR_INVALID_ARG;
    }
    
    sha256_context_t* ctx = (sha256_context_t*)context;
    
    // Add padding
    ctx->buffer[ctx->buffer_len++] = 0x80;
    
    if (ctx->buffer_len > 56) {
        // Need two blocks
        memset(ctx->buffer + ctx->buffer_len, 0, 64 - ctx->buffer_len);
        sha256_process_block(ctx, ctx->buffer);
        ctx->buffer_len = 0;
    }
    
    memset(ctx->buffer + ctx->buffer_len, 0, 56 - ctx->buffer_len);
    
    // Add bit count (big-endian)
    uint64_t bit_count = ctx->bit_count;
    ctx->buffer[56] = (uint8_t)(bit_count >> 56);
    ctx->buffer[57] = (uint8_t)(bit_count >> 48);
    ctx->buffer[58] = (uint8_t)(bit_count >> 40);
    ctx->buffer[59] = (uint8_t)(bit_count >> 32);
    ctx->buffer[60] = (uint8_t)(bit_count >> 24);
    ctx->buffer[61] = (uint8_t)(bit_count >> 16);
    ctx->buffer[62] = (uint8_t)(bit_count >> 8);
    ctx->buffer[63] = (uint8_t)bit_count;
    
    sha256_process_block(ctx, ctx->buffer);
    
    // Output hash (big-endian)
    for (int i = 0; i < 8; i++) {
        hash_output[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        hash_output[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        hash_output[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        hash_output[i * 4 + 3] = (uint8_t)ctx->state[i];
    }
    
    return ERR_OK;
}

/**
 * Free SHA-256 context
 */
void sha256_free(void* context) {
    if (context) {
        kfree(context);
    }
}

/**
 * Hash data with SHA-256 (one-shot)
 */
error_code_t sha256_hash(const uint8_t* data, size_t data_len, uint8_t* hash_output) {
    void* context;
    error_code_t err = sha256_init(&context);
    if (err != ERR_OK) {
        return err;
    }
    
    err = sha256_update(context, data, data_len);
    if (err != ERR_OK) {
        sha256_free(context);
        return err;
    }
    
    err = sha256_final(context, hash_output);
    sha256_free(context);
    return err;
}

