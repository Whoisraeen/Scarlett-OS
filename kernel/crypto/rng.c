/**
 * @file rng.c
 * @brief Cryptographically secure random number generator
 */

#include "../include/crypto/crypto.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"
#include "../include/time.h"

// RNG state (ChaCha20-based PRNG)
typedef struct {
    uint32_t state[16];
    uint8_t buffer[64];
    size_t buffer_pos;
    bool initialized;
} rng_state_t;

static rng_state_t rng_state = {0};

// ChaCha20 quarter round
static inline void chacha20_quarter_round(uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    *a += *b; *d ^= *a; *d = ((*d << 16) | (*d >> 16));
    *c += *d; *b ^= *c; *b = ((*b << 12) | (*b >> 20));
    *a += *b; *d ^= *a; *d = ((*d << 8) | (*d >> 24));
    *c += *d; *b ^= *c; *b = ((*b << 7) | (*b >> 25));
}

// ChaCha20 block function
static void chacha20_block(const uint32_t* input, uint32_t* output) {
    uint32_t x[16];
    for (int i = 0; i < 16; i++) {
        x[i] = input[i];
    }
    
    // 10 rounds (ChaCha20)
    for (int i = 0; i < 10; i++) {
        // Column rounds
        chacha20_quarter_round(&x[0], &x[4], &x[8], &x[12]);
        chacha20_quarter_round(&x[1], &x[5], &x[9], &x[13]);
        chacha20_quarter_round(&x[2], &x[6], &x[10], &x[14]);
        chacha20_quarter_round(&x[3], &x[7], &x[11], &x[15]);
        // Diagonal rounds
        chacha20_quarter_round(&x[0], &x[5], &x[10], &x[15]);
        chacha20_quarter_round(&x[1], &x[6], &x[11], &x[12]);
        chacha20_quarter_round(&x[2], &x[7], &x[8], &x[13]);
        chacha20_quarter_round(&x[3], &x[4], &x[9], &x[14]);
    }
    
    for (int i = 0; i < 16; i++) {
        output[i] = x[i] + input[i];
    }
}

// Collect entropy from various sources
static uint64_t collect_entropy(void) {
    uint64_t entropy = 0;
    
    // Use time (nanoseconds or ticks)
    extern uint64_t time_get_uptime_ms(void);
    entropy ^= time_get_uptime_ms();
    entropy = (entropy << 32) | (entropy >> 32);  // Rotate
    
    // Use CPU cycle counter if available
    #if defined(__x86_64__)
    uint64_t cycles;
    __asm__ volatile("rdtsc" : "=A"(cycles));
    entropy ^= cycles;
    #endif
    
    // Use memory addresses (ASLR-like)
    void* ptr = kmalloc(1);
    if (ptr) {
        entropy ^= (uint64_t)(uintptr_t)ptr;
        kfree(ptr);
    }
    
    return entropy;
}

/**
 * Initialize RNG
 */
error_code_t rng_init(void) {
    if (rng_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing RNG...\n");
    
    // Collect entropy
    uint64_t entropy1 = collect_entropy();
    uint64_t entropy2 = collect_entropy();
    uint64_t entropy3 = collect_entropy();
    uint64_t entropy4 = collect_entropy();
    
    // Initialize ChaCha20 state
    // Constants
    rng_state.state[0] = 0x61707865;
    rng_state.state[1] = 0x3320646e;
    rng_state.state[2] = 0x79622d32;
    rng_state.state[3] = 0x6b206574;
    
    // Key (from entropy)
    rng_state.state[4] = (uint32_t)(entropy1);
    rng_state.state[5] = (uint32_t)(entropy1 >> 32);
    rng_state.state[6] = (uint32_t)(entropy2);
    rng_state.state[7] = (uint32_t)(entropy2 >> 32);
    rng_state.state[8] = (uint32_t)(entropy3);
    rng_state.state[9] = (uint32_t)(entropy3 >> 32);
    rng_state.state[10] = (uint32_t)(entropy4);
    rng_state.state[11] = (uint32_t)(entropy4 >> 32);
    
    // Counter and nonce
    rng_state.state[12] = 0;
    rng_state.state[13] = 0;
    rng_state.state[14] = (uint32_t)(entropy1 ^ entropy2);
    rng_state.state[15] = (uint32_t)((entropy1 ^ entropy2) >> 32);
    
    rng_state.buffer_pos = 64;  // Force refill
    rng_state.initialized = true;
    
    kinfo("RNG initialized\n");
    return ERR_OK;
}

/**
 * Get random bytes
 */
error_code_t rng_get_bytes(uint8_t* buffer, size_t len) {
    if (!buffer) {
        return ERR_INVALID_ARG;
    }
    
    if (!rng_state.initialized) {
        error_code_t err = rng_init();
        if (err != ERR_OK) {
            return err;
        }
    }
    
    for (size_t i = 0; i < len; i++) {
        if (rng_state.buffer_pos >= 64) {
            // Generate new block
            uint32_t output[16];
            chacha20_block(rng_state.state, output);
            
            // Copy to buffer
            for (int j = 0; j < 16; j++) {
                ((uint32_t*)rng_state.buffer)[j] = output[j];
            }
            
            // Increment counter
            rng_state.state[12]++;
            if (rng_state.state[12] == 0) {
                rng_state.state[13]++;
            }
            
            // Add entropy occasionally
            if ((rng_state.state[12] & 0xFFF) == 0) {
                uint64_t entropy = collect_entropy();
                rng_state.state[14] ^= (uint32_t)entropy;
                rng_state.state[15] ^= (uint32_t)(entropy >> 32);
            }
            
            rng_state.buffer_pos = 0;
        }
        
        buffer[i] = rng_state.buffer[rng_state.buffer_pos++];
    }
    
    return ERR_OK;
}

