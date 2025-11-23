/**
 * @file aes256.c
 * @brief AES-256 encryption/decryption implementation
 */

#include "../include/crypto/crypto.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

// AES S-box (exported for use by AES-128 and AES-192)
const uint8_t sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

// Inverse S-box (exported for use by AES-128 and AES-192)
const uint8_t inv_sbox[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

// Rcon table (exported for use by AES-128 and AES-192)
const uint8_t rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

// AES-256: 14 rounds
#define AES256_ROUNDS 14
#define AES256_KEY_WORDS 8

// Key expansion for AES-256
static void aes256_key_expansion(const uint8_t* key, uint32_t* round_keys) {
    // Copy initial key
    for (int i = 0; i < AES256_KEY_WORDS; i++) {
        round_keys[i] = ((uint32_t)key[i * 4] << 24) |
                        ((uint32_t)key[i * 4 + 1] << 16) |
                        ((uint32_t)key[i * 4 + 2] << 8) |
                        ((uint32_t)key[i * 4 + 3]);
    }
    
    // Generate remaining round keys
    for (int i = AES256_KEY_WORDS; i < 4 * (AES256_ROUNDS + 1); i++) {
        uint32_t temp = round_keys[i - 1];
        
        if (i % AES256_KEY_WORDS == 0) {
            // RotWord, SubWord, XOR with Rcon
            temp = ((temp << 8) | (temp >> 24));
            temp = ((uint32_t)sbox[(temp >> 24) & 0xFF] << 24) |
                   ((uint32_t)sbox[(temp >> 16) & 0xFF] << 16) |
                   ((uint32_t)sbox[(temp >> 8) & 0xFF] << 8) |
                   ((uint32_t)sbox[temp & 0xFF]);
            temp ^= ((uint32_t)rcon[i / AES256_KEY_WORDS] << 24);
        } else if (i % AES256_KEY_WORDS == 4) {
            // SubWord
            temp = ((uint32_t)sbox[(temp >> 24) & 0xFF] << 24) |
                   ((uint32_t)sbox[(temp >> 16) & 0xFF] << 16) |
                   ((uint32_t)sbox[(temp >> 8) & 0xFF] << 8) |
                   ((uint32_t)sbox[temp & 0xFF]);
        }
        
        round_keys[i] = round_keys[i - AES256_KEY_WORDS] ^ temp;
    }
}

// AddRoundKey (exported for use by AES-128 and AES-192)
void add_round_key(uint8_t* state, const uint32_t* round_key) {
    for (int i = 0; i < 4; i++) {
        uint32_t k = round_key[i];
        state[i * 4] ^= (k >> 24) & 0xFF;
        state[i * 4 + 1] ^= (k >> 16) & 0xFF;
        state[i * 4 + 2] ^= (k >> 8) & 0xFF;
        state[i * 4 + 3] ^= k & 0xFF;
    }
}

// SubBytes (exported for use by AES-128 and AES-192)
void sub_bytes(uint8_t* state) {
    for (int i = 0; i < 16; i++) {
        state[i] = sbox[state[i]];
    }
}

// InvSubBytes (exported for use by AES-128 and AES-192)
void inv_sub_bytes(uint8_t* state) {
    for (int i = 0; i < 16; i++) {
        state[i] = inv_sbox[state[i]];
    }
}

// ShiftRows (exported for use by AES-128 and AES-192)
void shift_rows(uint8_t* state) {
    uint8_t temp;
    
    // Row 1: shift left by 1
    temp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = temp;
    
    // Row 2: shift left by 2
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;
    
    // Row 3: shift left by 3
    temp = state[3];
    state[3] = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = temp;
}

// InvShiftRows (exported for use by AES-128 and AES-192)
void inv_shift_rows(uint8_t* state) {
    uint8_t temp;
    
    // Row 1: shift right by 1
    temp = state[13];
    state[13] = state[9];
    state[9] = state[5];
    state[5] = state[1];
    state[1] = temp;
    
    // Row 2: shift right by 2
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;
    
    // Row 3: shift right by 3
    temp = state[3];
    state[3] = state[7];
    state[7] = state[11];
    state[11] = state[15];
    state[15] = temp;
}

// Galois field multiplication
static inline uint8_t gmul(uint8_t a, uint8_t b) {
    uint8_t p = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) {
            p ^= a;
        }
        uint8_t hi_bit = a & 0x80;
        a <<= 1;
        if (hi_bit) {
            a ^= 0x1b;
        }
        b >>= 1;
    }
    return p;
}

// MixColumns (exported for use by AES-128 and AES-192)
void mix_columns(uint8_t* state) {
    for (int i = 0; i < 4; i++) {
        uint8_t s0 = state[i * 4];
        uint8_t s1 = state[i * 4 + 1];
        uint8_t s2 = state[i * 4 + 2];
        uint8_t s3 = state[i * 4 + 3];
        
        state[i * 4] = gmul(2, s0) ^ gmul(3, s1) ^ s2 ^ s3;
        state[i * 4 + 1] = s0 ^ gmul(2, s1) ^ gmul(3, s2) ^ s3;
        state[i * 4 + 2] = s0 ^ s1 ^ gmul(2, s2) ^ gmul(3, s3);
        state[i * 4 + 3] = gmul(3, s0) ^ s1 ^ s2 ^ gmul(2, s3);
    }
}

// InvMixColumns (exported for use by AES-128 and AES-192)
void inv_mix_columns(uint8_t* state) {
    for (int i = 0; i < 4; i++) {
        uint8_t s0 = state[i * 4];
        uint8_t s1 = state[i * 4 + 1];
        uint8_t s2 = state[i * 4 + 2];
        uint8_t s3 = state[i * 4 + 3];
        
        state[i * 4] = gmul(0x0e, s0) ^ gmul(0x0b, s1) ^ gmul(0x0d, s2) ^ gmul(0x09, s3);
        state[i * 4 + 1] = gmul(0x09, s0) ^ gmul(0x0e, s1) ^ gmul(0x0b, s2) ^ gmul(0x0d, s3);
        state[i * 4 + 2] = gmul(0x0d, s0) ^ gmul(0x09, s1) ^ gmul(0x0e, s2) ^ gmul(0x0b, s3);
        state[i * 4 + 3] = gmul(0x0b, s0) ^ gmul(0x0d, s1) ^ gmul(0x09, s2) ^ gmul(0x0e, s3);
    }
}

/**
 * AES-256 encrypt (CBC mode)
 */
error_code_t aes256_encrypt(const uint8_t* key, const uint8_t* iv,
                           const uint8_t* plaintext, size_t plaintext_len, uint8_t* ciphertext) {
    if (!key || !plaintext || !ciphertext) {
        return ERR_INVALID_ARG;
    }
    
    if (plaintext_len % AES_BLOCK_SIZE != 0) {
        return ERR_INVALID_ARG;  // Must be multiple of block size
    }
    
    // Expand key
    uint32_t round_keys[4 * (AES256_ROUNDS + 1)];
    aes256_key_expansion(key, round_keys);
    
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
        for (int round = 1; round < AES256_ROUNDS; round++) {
            sub_bytes(state);
            shift_rows(state);
            mix_columns(state);
            add_round_key(state, &round_keys[round * 4]);
        }
        
        // Final round
        sub_bytes(state);
        shift_rows(state);
        add_round_key(state, &round_keys[AES256_ROUNDS * 4]);
        
        // Copy to output and update prev_block
        memcpy(&ciphertext[block * AES_BLOCK_SIZE], state, AES_BLOCK_SIZE);
        memcpy(prev_block, state, AES_BLOCK_SIZE);
    }
    
    return ERR_OK;
}

/**
 * AES-256 decrypt (CBC mode)
 */
error_code_t aes256_decrypt(const uint8_t* key, const uint8_t* iv,
                            const uint8_t* ciphertext, size_t ciphertext_len, uint8_t* plaintext) {
    if (!key || !ciphertext || !plaintext) {
        return ERR_INVALID_ARG;
    }
    
    if (ciphertext_len % AES_BLOCK_SIZE != 0) {
        return ERR_INVALID_ARG;
    }
    
    // Expand key
    uint32_t round_keys[4 * (AES256_ROUNDS + 1)];
    aes256_key_expansion(key, round_keys);
    
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
        add_round_key(state, &round_keys[AES256_ROUNDS * 4]);
        
        // Main rounds
        for (int round = AES256_ROUNDS - 1; round > 0; round--) {
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

