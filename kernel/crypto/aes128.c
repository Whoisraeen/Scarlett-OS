/**
 * @file aes128.c
 * @brief AES-128 encryption/decryption implementation
 */

#include "../include/crypto/crypto.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

// AES S-box (shared with AES-256)
extern const uint8_t sbox[256];
extern const uint8_t inv_sbox[256];
extern const uint8_t rcon[11];

// AES-128: 10 rounds
#define AES128_ROUNDS 10
#define AES128_KEY_WORDS 4

// Key expansion for AES-128
static void aes128_key_expansion(const uint8_t* key, uint32_t* round_keys) {
    // Copy initial key
    for (int i = 0; i < AES128_KEY_WORDS; i++) {
        round_keys[i] = ((uint32_t)key[i * 4] << 24) |
                        ((uint32_t)key[i * 4 + 1] << 16) |
                        ((uint32_t)key[i * 4 + 2] << 8) |
                        ((uint32_t)key[i * 4 + 3]);
    }
    
    // Generate remaining round keys
    for (int i = AES128_KEY_WORDS; i < 4 * (AES128_ROUNDS + 1); i++) {
        uint32_t temp = round_keys[i - 1];
        
        if (i % AES128_KEY_WORDS == 0) {
            // RotWord, SubWord, XOR with Rcon
            temp = ((temp << 8) | (temp >> 24));
            temp = ((uint32_t)sbox[(temp >> 24) & 0xFF] << 24) |
                   ((uint32_t)sbox[(temp >> 16) & 0xFF] << 16) |
                   ((uint32_t)sbox[(temp >> 8) & 0xFF] << 8) |
                   ((uint32_t)sbox[temp & 0xFF]);
            temp ^= ((uint32_t)rcon[i / AES128_KEY_WORDS] << 24);
        }
        
        round_keys[i] = round_keys[i - AES128_KEY_WORDS] ^ temp;
    }
}

// Forward declarations for AES operations (shared with AES-256)
extern void add_round_key(uint8_t* state, const uint32_t* round_key);
extern void sub_bytes(uint8_t* state);
extern void inv_sub_bytes(uint8_t* state);
extern void shift_rows(uint8_t* state);
extern void inv_shift_rows(uint8_t* state);
extern void mix_columns(uint8_t* state);
extern void inv_mix_columns(uint8_t* state);

/**
 * AES-128 encrypt (CBC mode)
 */
error_code_t aes128_encrypt(const uint8_t* key, const uint8_t* iv,
                           const uint8_t* plaintext, size_t plaintext_len, uint8_t* ciphertext) {
    if (!key || !plaintext || !ciphertext) {
        return ERR_INVALID_ARG;
    }
    
    if (plaintext_len % AES_BLOCK_SIZE != 0) {
        return ERR_INVALID_ARG;  // Must be multiple of block size
    }
    
    // Expand key
    uint32_t round_keys[4 * (AES128_ROUNDS + 1)];
    aes128_key_expansion(key, round_keys);
    
    uint8_t prev_block[AES_BLOCK_SIZE];
    if (iv) {
        memcpy(prev_block, iv, AES_BLOCK_SIZE);
    } else {
        memset(prev_block, 0, AES_BLOCK_SIZE);
    }
    
    // Process each block
    for (size_t block = 0; block < plaintext_len / AES_BLOCK_SIZE; block++) {
        uint8_t state[AES_BLOCK_SIZE];
        
        // XOR with previous ciphertext block (CBC)
        for (int i = 0; i < AES_BLOCK_SIZE; i++) {
            state[i] = plaintext[block * AES_BLOCK_SIZE + i] ^ prev_block[i];
        }
        
        // Initial round
        add_round_key(state, &round_keys[0]);
        
        // Main rounds
        for (int round = 1; round < AES128_ROUNDS; round++) {
            sub_bytes(state);
            shift_rows(state);
            mix_columns(state);
            add_round_key(state, &round_keys[round * 4]);
        }
        
        // Final round
        sub_bytes(state);
        shift_rows(state);
        add_round_key(state, &round_keys[AES128_ROUNDS * 4]);
        
        // Copy to output and update prev_block
        memcpy(&ciphertext[block * AES_BLOCK_SIZE], state, AES_BLOCK_SIZE);
        memcpy(prev_block, state, AES_BLOCK_SIZE);
    }
    
    return ERR_OK;
}

/**
 * AES-128 decrypt (CBC mode)
 */
error_code_t aes128_decrypt(const uint8_t* key, const uint8_t* iv,
                           const uint8_t* ciphertext, size_t ciphertext_len, uint8_t* plaintext) {
    if (!key || !ciphertext || !plaintext) {
        return ERR_INVALID_ARG;
    }
    
    if (ciphertext_len % AES_BLOCK_SIZE != 0) {
        return ERR_INVALID_ARG;  // Must be multiple of block size
    }
    
    // Expand key
    uint32_t round_keys[4 * (AES128_ROUNDS + 1)];
    aes128_key_expansion(key, round_keys);
    
    uint8_t prev_block[AES_BLOCK_SIZE];
    if (iv) {
        memcpy(prev_block, iv, AES_BLOCK_SIZE);
    } else {
        memset(prev_block, 0, AES_BLOCK_SIZE);
    }
    
    // Process each block
    for (size_t block = 0; block < ciphertext_len / AES_BLOCK_SIZE; block++) {
        uint8_t state[AES_BLOCK_SIZE];
        memcpy(state, &ciphertext[block * AES_BLOCK_SIZE], AES_BLOCK_SIZE);
        uint8_t saved_state[AES_BLOCK_SIZE];
        memcpy(saved_state, state, AES_BLOCK_SIZE);
        
        // Initial round
        add_round_key(state, &round_keys[AES128_ROUNDS * 4]);
        
        // Main rounds
        for (int round = AES128_ROUNDS - 1; round > 0; round--) {
            inv_shift_rows(state);
            inv_sub_bytes(state);
            add_round_key(state, &round_keys[round * 4]);
            inv_mix_columns(state);
        }
        
        // Final round
        inv_shift_rows(state);
        inv_sub_bytes(state);
        add_round_key(state, &round_keys[0]);
        
        // XOR with previous ciphertext block (CBC)
        for (int i = 0; i < AES_BLOCK_SIZE; i++) {
            plaintext[block * AES_BLOCK_SIZE + i] = state[i] ^ prev_block[i];
        }
        
        memcpy(prev_block, saved_state, AES_BLOCK_SIZE);
    }
    
    return ERR_OK;
}

