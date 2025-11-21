/**
 * @file crypto.h
 * @brief Cryptographic library interface
 */

#ifndef KERNEL_CRYPTO_H
#define KERNEL_CRYPTO_H

#include "../types.h"
#include "../errors.h"

// Hash function types
typedef enum {
    CRYPTO_HASH_SHA256,
    CRYPTO_HASH_SHA512,
    CRYPTO_HASH_MD5  // For compatibility only, not recommended
} crypto_hash_type_t;

// Hash output sizes
#define SHA256_HASH_SIZE 32
#define SHA512_HASH_SIZE 64
#define MD5_HASH_SIZE 16

// Symmetric encryption types
typedef enum {
    CRYPTO_CIPHER_AES256,
    CRYPTO_CIPHER_AES128,
    CRYPTO_CIPHER_AES192
} crypto_cipher_type_t;

// AES key sizes
#define AES128_KEY_SIZE 16
#define AES192_KEY_SIZE 24
#define AES256_KEY_SIZE 32
#define AES_BLOCK_SIZE 16

// Asymmetric encryption types
typedef enum {
    CRYPTO_ASYM_RSA2048,
    CRYPTO_ASYM_RSA4096,
    CRYPTO_ASYM_ECC_P256,
    CRYPTO_ASYM_ECC_P384
} crypto_asym_type_t;

// RSA key sizes
#define RSA2048_KEY_SIZE 256
#define RSA4096_KEY_SIZE 512

// ECC key sizes
#define ECC_P256_KEY_SIZE 32
#define ECC_P384_KEY_SIZE 48

// Initialization
error_code_t crypto_init(void);

// Hash functions
error_code_t crypto_hash(crypto_hash_type_t type, const uint8_t* data, size_t data_len, uint8_t* hash_output);
error_code_t crypto_hash_init(crypto_hash_type_t type, void** context);
error_code_t crypto_hash_update(void* context, const uint8_t* data, size_t data_len);
error_code_t crypto_hash_final(void* context, uint8_t* hash_output);
void crypto_hash_free(void* context);

// Symmetric encryption
error_code_t crypto_encrypt(crypto_cipher_type_t type, const uint8_t* key, const uint8_t* iv,
                            const uint8_t* plaintext, size_t plaintext_len, uint8_t* ciphertext);
error_code_t crypto_decrypt(crypto_cipher_type_t type, const uint8_t* key, const uint8_t* iv,
                            const uint8_t* ciphertext, size_t ciphertext_len, uint8_t* plaintext);

// Asymmetric encryption (RSA)
error_code_t crypto_rsa_generate_keypair(crypto_asym_type_t type, uint8_t* public_key, size_t* public_key_len,
                                         uint8_t* private_key, size_t* private_key_len);
error_code_t crypto_rsa_encrypt(const uint8_t* public_key, size_t public_key_len,
                                const uint8_t* plaintext, size_t plaintext_len, uint8_t* ciphertext);
error_code_t crypto_rsa_decrypt(const uint8_t* private_key, size_t private_key_len,
                                const uint8_t* ciphertext, size_t ciphertext_len, uint8_t* plaintext);

// Asymmetric encryption (ECC)
error_code_t crypto_ecc_generate_keypair(crypto_asym_type_t type, uint8_t* public_key, size_t* public_key_len,
                                          uint8_t* private_key, size_t* private_key_len);
error_code_t crypto_ecc_sign(const uint8_t* private_key, size_t private_key_len,
                             const uint8_t* data, size_t data_len, uint8_t* signature, size_t* signature_len);
error_code_t crypto_ecc_verify(const uint8_t* public_key, size_t public_key_len,
                                const uint8_t* data, size_t data_len, const uint8_t* signature, size_t signature_len);

// Random number generation
error_code_t crypto_random_bytes(uint8_t* buffer, size_t len);
error_code_t crypto_random_init(void);
uint32_t crypto_random_u32(void);
uint64_t crypto_random_u64(void);

// Key derivation (PBKDF2)
error_code_t crypto_pbkdf2(crypto_hash_type_t hash_type, const uint8_t* password, size_t password_len,
                           const uint8_t* salt, size_t salt_len, uint32_t iterations,
                           uint8_t* key, size_t key_len);

#endif // KERNEL_CRYPTO_H

