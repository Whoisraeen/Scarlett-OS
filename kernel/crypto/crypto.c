/**
 * @file crypto.c
 * @brief Cryptographic library implementation
 */

#include "../include/crypto/crypto.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/mm/heap.h"

// Forward declarations
extern error_code_t sha256_init(void** context);
extern error_code_t sha256_update(void* context, const uint8_t* data, size_t data_len);
extern error_code_t sha256_final(void* context, uint8_t* hash_output);
extern void sha256_free(void* context);
extern error_code_t sha256_hash(const uint8_t* data, size_t data_len, uint8_t* hash_output);

extern error_code_t sha512_init(void** context);
extern error_code_t sha512_update(void* context, const uint8_t* data, size_t data_len);
extern error_code_t sha512_final(void* context, uint8_t* hash_output);
extern void sha512_free(void* context);
extern error_code_t sha512_hash(const uint8_t* data, size_t data_len, uint8_t* hash_output);

extern error_code_t aes256_encrypt(const uint8_t* key, const uint8_t* iv,
                                   const uint8_t* plaintext, size_t plaintext_len, uint8_t* ciphertext);
extern error_code_t aes256_decrypt(const uint8_t* key, const uint8_t* iv,
                                   const uint8_t* ciphertext, size_t ciphertext_len, uint8_t* plaintext);

extern error_code_t rng_init(void);
extern error_code_t rng_get_bytes(uint8_t* buffer, size_t len);

static bool crypto_initialized = false;

/**
 * Initialize crypto library
 */
error_code_t crypto_init(void) {
    if (crypto_initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing crypto library...\n");
    
    // Initialize RNG
    error_code_t err = rng_init();
    if (err != ERR_OK) {
        kerror("Failed to initialize RNG: %d\n", err);
        return err;
    }
    
    crypto_initialized = true;
    kinfo("Crypto library initialized\n");
    return ERR_OK;
}

/**
 * Hash data using specified hash function
 */
error_code_t crypto_hash(crypto_hash_type_t type, const uint8_t* data, size_t data_len, uint8_t* hash_output) {
    if (!data || !hash_output) {
        return ERR_INVALID_ARG;
    }
    
    switch (type) {
        case CRYPTO_HASH_SHA256:
            return sha256_hash(data, data_len, hash_output);
        case CRYPTO_HASH_SHA512:
            return sha512_hash(data, data_len, hash_output);
        default:
            return ERR_NOT_SUPPORTED;
    }
}

/**
 * Initialize hash context
 */
error_code_t crypto_hash_init(crypto_hash_type_t type, void** context) {
    if (!context) {
        return ERR_INVALID_ARG;
    }
    
    switch (type) {
        case CRYPTO_HASH_SHA256:
            return sha256_init(context);
        case CRYPTO_HASH_SHA512:
            return sha512_init(context);
        default:
            return ERR_NOT_SUPPORTED;
    }
}

/**
 * Update hash with more data
 */
error_code_t crypto_hash_update(void* context, const uint8_t* data, size_t data_len) {
    if (!context || !data) {
        return ERR_INVALID_ARG;
    }
    
    // Determine hash type from context (simplified - in real implementation, store type in context)
    // For now, try SHA256 first
    error_code_t err = sha256_update(context, data, data_len);
    if (err == ERR_OK) {
        return ERR_OK;
    }
    
    // Try SHA512
    return sha512_update(context, data, data_len);
}

/**
 * Finalize hash and get output
 */
error_code_t crypto_hash_final(void* context, uint8_t* hash_output) {
    if (!context || !hash_output) {
        return ERR_INVALID_ARG;
    }
    
    // Try SHA256 first
    error_code_t err = sha256_final(context, hash_output);
    if (err == ERR_OK) {
        return ERR_OK;
    }
    
    // Try SHA512
    return sha512_final(context, hash_output);
}

/**
 * Free hash context
 */
void crypto_hash_free(void* context) {
    if (!context) {
        return;
    }
    
    // Try both (one will succeed)
    sha256_free(context);
    sha512_free(context);
}

/**
 * Encrypt data using symmetric cipher
 */
error_code_t crypto_encrypt(crypto_cipher_type_t type, const uint8_t* key, const uint8_t* iv,
                            const uint8_t* plaintext, size_t plaintext_len, uint8_t* ciphertext) {
    if (!key || !plaintext || !ciphertext) {
        return ERR_INVALID_ARG;
    }
    
    switch (type) {
        case CRYPTO_CIPHER_AES256:
            return aes256_encrypt(key, iv, plaintext, plaintext_len, ciphertext);
        case CRYPTO_CIPHER_AES128:
        case CRYPTO_CIPHER_AES192:
            // TODO: Implement AES128 and AES192
            return ERR_NOT_SUPPORTED;
        default:
            return ERR_NOT_SUPPORTED;
    }
}

/**
 * Decrypt data using symmetric cipher
 */
error_code_t crypto_decrypt(crypto_cipher_type_t type, const uint8_t* key, const uint8_t* iv,
                            const uint8_t* ciphertext, size_t ciphertext_len, uint8_t* plaintext) {
    if (!key || !ciphertext || !plaintext) {
        return ERR_INVALID_ARG;
    }
    
    switch (type) {
        case CRYPTO_CIPHER_AES256:
            return aes256_decrypt(key, iv, ciphertext, ciphertext_len, plaintext);
        case CRYPTO_CIPHER_AES128:
        case CRYPTO_CIPHER_AES192:
            // TODO: Implement AES128 and AES192
            return ERR_NOT_SUPPORTED;
        default:
            return ERR_NOT_SUPPORTED;
    }
}

/**
 * Generate RSA keypair
 */
error_code_t crypto_rsa_generate_keypair(crypto_asym_type_t type, uint8_t* public_key, size_t* public_key_len,
                                         uint8_t* private_key, size_t* private_key_len) {
    // TODO: Implement RSA key generation
    return ERR_NOT_SUPPORTED;
}

/**
 * RSA encrypt
 */
error_code_t crypto_rsa_encrypt(const uint8_t* public_key, size_t public_key_len,
                                const uint8_t* plaintext, size_t plaintext_len, uint8_t* ciphertext) {
    // TODO: Implement RSA encryption
    return ERR_NOT_SUPPORTED;
}

/**
 * RSA decrypt
 */
error_code_t crypto_rsa_decrypt(const uint8_t* private_key, size_t private_key_len,
                                const uint8_t* ciphertext, size_t ciphertext_len, uint8_t* plaintext) {
    // TODO: Implement RSA decryption
    return ERR_NOT_SUPPORTED;
}

/**
 * Generate ECC keypair
 */
error_code_t crypto_ecc_generate_keypair(crypto_asym_type_t type, uint8_t* public_key, size_t* public_key_len,
                                          uint8_t* private_key, size_t* private_key_len) {
    // TODO: Implement ECC key generation
    return ERR_NOT_SUPPORTED;
}

/**
 * ECC sign
 */
error_code_t crypto_ecc_sign(const uint8_t* private_key, size_t private_key_len,
                             const uint8_t* data, size_t data_len, uint8_t* signature, size_t* signature_len) {
    // TODO: Implement ECC signing
    return ERR_NOT_SUPPORTED;
}

/**
 * ECC verify
 */
error_code_t crypto_ecc_verify(const uint8_t* public_key, size_t public_key_len,
                               const uint8_t* data, size_t data_len, const uint8_t* signature, size_t signature_len) {
    // TODO: Implement ECC verification
    return ERR_NOT_SUPPORTED;
}

/**
 * Generate random bytes
 */
error_code_t crypto_random_bytes(uint8_t* buffer, size_t len) {
    if (!buffer) {
        return ERR_INVALID_ARG;
    }
    
    return rng_get_bytes(buffer, len);
}

/**
 * Initialize random number generator
 */
error_code_t crypto_random_init(void) {
    return rng_init();
}

/**
 * Generate random 32-bit unsigned integer
 */
uint32_t crypto_random_u32(void) {
    uint32_t value;
    if (crypto_random_bytes((uint8_t*)&value, sizeof(value)) == ERR_OK) {
        return value;
    }
    return 0;
}

/**
 * Generate random 64-bit unsigned integer
 */
uint64_t crypto_random_u64(void) {
    uint64_t value;
    if (crypto_random_bytes((uint8_t*)&value, sizeof(value)) == ERR_OK) {
        return value;
    }
    return 0;
}

/**
 * PBKDF2 key derivation
 */
error_code_t crypto_pbkdf2(crypto_hash_type_t hash_type, const uint8_t* password, size_t password_len,
                          const uint8_t* salt, size_t salt_len, uint32_t iterations,
                          uint8_t* key, size_t key_len) {
    if (!password || !salt || !key) {
        return ERR_INVALID_ARG;
    }
    
    // Hash output size
    size_t hash_size = (hash_type == CRYPTO_HASH_SHA256) ? SHA256_HASH_SIZE : SHA512_HASH_SIZE;
    
    // Allocate working buffer
    uint8_t* u = (uint8_t*)kmalloc(hash_size);
    uint8_t* t = (uint8_t*)kmalloc(hash_size);
    if (!u || !t) {
        if (u) kfree(u);
        if (t) kfree(t);
        return ERR_OUT_OF_MEMORY;
    }
    
    // Calculate number of blocks needed
    size_t blocks = (key_len + hash_size - 1) / hash_size;
    
    for (size_t i = 0; i < blocks; i++) {
        // U1 = HMAC(password, salt || i)
        uint8_t salt_block[256];
        size_t salt_block_len = salt_len + 4;
        memcpy(salt_block, salt, salt_len);
        salt_block[salt_len] = (uint8_t)((i + 1) >> 24);
        salt_block[salt_len + 1] = (uint8_t)((i + 1) >> 16);
        salt_block[salt_len + 2] = (uint8_t)((i + 1) >> 8);
        salt_block[salt_len + 3] = (uint8_t)(i + 1);
        
        // HMAC(password, salt_block) - simplified (use hash directly for now)
        uint8_t input[512];
        size_t input_len = password_len + salt_block_len;
        memcpy(input, password, password_len);
        memcpy(input + password_len, salt_block, salt_block_len);
        
        crypto_hash(hash_type, input, input_len, u);
        memcpy(t, u, hash_size);
        
        // Iterate
        for (uint32_t j = 1; j < iterations; j++) {
            crypto_hash(hash_type, u, hash_size, u);
            for (size_t k = 0; k < hash_size; k++) {
                t[k] ^= u[k];
            }
        }
        
        // Copy to output
        size_t copy_len = (key_len > (i + 1) * hash_size) ? hash_size : (key_len - i * hash_size);
        memcpy(key + i * hash_size, t, copy_len);
    }
    
    kfree(u);
    kfree(t);
    return ERR_OK;
}

