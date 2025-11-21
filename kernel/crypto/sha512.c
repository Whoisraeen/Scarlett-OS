/**
 * @file sha512.c
 * @brief SHA-512 hash implementation
 */

#include "../include/crypto/crypto.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

// SHA-512 context
typedef struct {
    uint64_t state[8];
    uint64_t bit_count[2];  // 128-bit bit count
    uint8_t buffer[128];
    uint32_t buffer_len;
} sha512_context_t;

// SHA-512 constants
static const uint64_t k[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x14292967a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x6f1c0e5b6c823b93ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

// Right rotate (64-bit)
static inline uint64_t rotr64(uint64_t x, int n) {
    return (x >> n) | (x << (64 - n));
}

// SHA-512 functions
static inline uint64_t ch(uint64_t x, uint64_t y, uint64_t z) {
    return (x & y) ^ (~x & z);
}

static inline uint64_t maj(uint64_t x, uint64_t y, uint64_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

static inline uint64_t sigma0(uint64_t x) {
    return rotr64(x, 28) ^ rotr64(x, 34) ^ rotr64(x, 39);
}

static inline uint64_t sigma1(uint64_t x) {
    return rotr64(x, 14) ^ rotr64(x, 18) ^ rotr64(x, 41);
}

static inline uint64_t gamma0(uint64_t x) {
    return rotr64(x, 1) ^ rotr64(x, 8) ^ (x >> 7);
}

static inline uint64_t gamma1(uint64_t x) {
    return rotr64(x, 19) ^ rotr64(x, 61) ^ (x >> 6);
}

// Process 1024-bit block
static void sha512_process_block(sha512_context_t* ctx, const uint8_t* block) {
    uint64_t w[80];
    
    // Copy block into w[0..15] (big-endian)
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint64_t)block[i * 8] << 56) |
               ((uint64_t)block[i * 8 + 1] << 48) |
               ((uint64_t)block[i * 8 + 2] << 40) |
               ((uint64_t)block[i * 8 + 3] << 32) |
               ((uint64_t)block[i * 8 + 4] << 24) |
               ((uint64_t)block[i * 8 + 5] << 16) |
               ((uint64_t)block[i * 8 + 6] << 8) |
               ((uint64_t)block[i * 8 + 7]);
    }
    
    // Extend to w[16..79]
    for (int i = 16; i < 80; i++) {
        w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];
    }
    
    // Initialize working variables
    uint64_t a = ctx->state[0];
    uint64_t b = ctx->state[1];
    uint64_t c = ctx->state[2];
    uint64_t d = ctx->state[3];
    uint64_t e = ctx->state[4];
    uint64_t f = ctx->state[5];
    uint64_t g = ctx->state[6];
    uint64_t h = ctx->state[7];
    
    // Main loop
    for (int i = 0; i < 80; i++) {
        uint64_t t1 = h + sigma1(e) + ch(e, f, g) + k[i] + w[i];
        uint64_t t2 = sigma0(a) + maj(a, b, c);
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
 * Initialize SHA-512 context
 */
error_code_t sha512_init(void** context) {
    if (!context) {
        return ERR_INVALID_ARG;
    }
    
    sha512_context_t* ctx = (sha512_context_t*)kmalloc(sizeof(sha512_context_t));
    if (!ctx) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Initial hash values
    ctx->state[0] = 0x6a09e667f3bcc908ULL;
    ctx->state[1] = 0xbb67ae8584caa73bULL;
    ctx->state[2] = 0x3c6ef372fe94f82bULL;
    ctx->state[3] = 0xa54ff53a5f1d36f1ULL;
    ctx->state[4] = 0x510e527fade682d1ULL;
    ctx->state[5] = 0x9b05688c2b3e6c1fULL;
    ctx->state[6] = 0x1f83d9abfb41bd6bULL;
    ctx->state[7] = 0x5be0cd19137e2179ULL;
    ctx->bit_count[0] = 0;
    ctx->bit_count[1] = 0;
    ctx->buffer_len = 0;
    
    *context = ctx;
    return ERR_OK;
}

/**
 * Update SHA-512 with more data
 */
error_code_t sha512_update(void* context, const uint8_t* data, size_t data_len) {
    if (!context || !data) {
        return ERR_INVALID_ARG;
    }
    
    sha512_context_t* ctx = (sha512_context_t*)context;
    size_t offset = 0;
    
    while (offset < data_len) {
        size_t space = 128 - ctx->buffer_len;
        size_t copy = (data_len - offset < space) ? (data_len - offset) : space;
        
        memcpy(ctx->buffer + ctx->buffer_len, data + offset, copy);
        ctx->buffer_len += copy;
        offset += copy;
        
        // Update bit count (128-bit)
        uint64_t bits = (uint64_t)copy * 8;
        uint64_t old_low = ctx->bit_count[1];
        ctx->bit_count[1] += bits;
        if (ctx->bit_count[1] < old_low) {
            ctx->bit_count[0]++;  // Carry
        }
        
        if (ctx->buffer_len == 128) {
            sha512_process_block(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
    
    return ERR_OK;
}

/**
 * Finalize SHA-512 and get hash
 */
error_code_t sha512_final(void* context, uint8_t* hash_output) {
    if (!context || !hash_output) {
        return ERR_INVALID_ARG;
    }
    
    sha512_context_t* ctx = (sha512_context_t*)context;
    
    // Add padding
    ctx->buffer[ctx->buffer_len++] = 0x80;
    
    if (ctx->buffer_len > 112) {
        // Need two blocks
        memset(ctx->buffer + ctx->buffer_len, 0, 128 - ctx->buffer_len);
        sha512_process_block(ctx, ctx->buffer);
        ctx->buffer_len = 0;
    }
    
    memset(ctx->buffer + ctx->buffer_len, 0, 112 - ctx->buffer_len);
    
    // Add bit count (big-endian, 128-bit)
    for (int i = 0; i < 8; i++) {
        ctx->buffer[120 - i] = (uint8_t)(ctx->bit_count[0] >> (i * 8));
    }
    for (int i = 0; i < 8; i++) {
        ctx->buffer[128 - i] = (uint8_t)(ctx->bit_count[1] >> (i * 8));
    }
    
    sha512_process_block(ctx, ctx->buffer);
    
    // Output hash (big-endian)
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            hash_output[i * 8 + j] = (uint8_t)(ctx->state[i] >> (56 - j * 8));
        }
    }
    
    return ERR_OK;
}

/**
 * Free SHA-512 context
 */
void sha512_free(void* context) {
    if (context) {
        kfree(context);
    }
}

/**
 * Hash data with SHA-512 (one-shot)
 */
error_code_t sha512_hash(const uint8_t* data, size_t data_len, uint8_t* hash_output) {
    void* context;
    error_code_t err = sha512_init(&context);
    if (err != ERR_OK) {
        return err;
    }
    
    err = sha512_update(context, data, data_len);
    if (err != ERR_OK) {
        sha512_free(context);
        return err;
    }
    
    err = sha512_final(context, hash_output);
    sha512_free(context);
    return err;
}

