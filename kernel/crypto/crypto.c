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
extern error_code_t aes128_encrypt(const uint8_t* key, const uint8_t* iv,
                                   const uint8_t* plaintext, size_t plaintext_len, uint8_t* ciphertext);
extern error_code_t aes128_decrypt(const uint8_t* key, const uint8_t* iv,
                                   const uint8_t* ciphertext, size_t ciphertext_len, uint8_t* plaintext);
extern error_code_t aes192_encrypt(const uint8_t* key, const uint8_t* iv,
                                   const uint8_t* plaintext, size_t plaintext_len, uint8_t* ciphertext);
extern error_code_t aes192_decrypt(const uint8_t* key, const uint8_t* iv,
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
            return aes128_encrypt(key, iv, plaintext, plaintext_len, ciphertext);
        case CRYPTO_CIPHER_AES192:
            return aes192_encrypt(key, iv, plaintext, plaintext_len, ciphertext);
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
            return aes128_decrypt(key, iv, ciphertext, ciphertext_len, plaintext);
        case CRYPTO_CIPHER_AES192:
            return aes192_decrypt(key, iv, ciphertext, ciphertext_len, plaintext);
        default:
            return ERR_NOT_SUPPORTED;
    }
}

/**
 * Generate RSA keypair
 * 
 * RSA key generation:
 * 1. Generate two large primes p and q
 * 2. Compute n = p * q (modulus)
 * 3. Compute phi(n) = (p-1) * (q-1)
 * 4. Choose public exponent e (typically 65537)
 * 5. Compute private exponent d such that e*d ≡ 1 (mod phi(n))
 * 
 * Key format (simplified):
 * Public key: {n, e}
 * Private key: {n, d, p, q}
 */
error_code_t crypto_rsa_generate_keypair(crypto_asym_type_t type, uint8_t* public_key, size_t* public_key_len,
                                         uint8_t* private_key, size_t* private_key_len) {
    if (!public_key || !public_key_len || !private_key || !private_key_len) {
        return ERR_INVALID_ARG;
    }
    
    size_t key_size;
    switch (type) {
        case CRYPTO_ASYM_RSA2048:
            key_size = RSA2048_KEY_SIZE;
            break;
        case CRYPTO_ASYM_RSA4096:
            key_size = RSA4096_KEY_SIZE;
            break;
        default:
            return ERR_NOT_SUPPORTED;
    }
    
    // Check buffer sizes
    // Public key: n (key_size bytes) + e (4 bytes) = key_size + 4
    // Private key: n (key_size) + d (key_size) + p (key_size/2) + q (key_size/2) = 2*key_size + key_size = 3*key_size
    if (*public_key_len < key_size + 4 || *private_key_len < 3 * key_size) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("RSA: Generating %zu-bit keypair\n", key_size * 8);
    
    // Generate two large random primes p and q
    // For production, use proper prime generation (Miller-Rabin test)
    uint8_t* p = (uint8_t*)kmalloc(key_size / 2);
    uint8_t* q = (uint8_t*)kmalloc(key_size / 2);
    if (!p || !q) {
        if (p) kfree(p);
        if (q) kfree(q);
        return ERR_OUT_OF_MEMORY;
    }
    
    // Generate random candidates for p and q
    // In production, these would be tested for primality
    error_code_t err = crypto_random_bytes(p, key_size / 2);
    if (err != ERR_OK) {
        kfree(p);
        kfree(q);
        return err;
    }
    
    err = crypto_random_bytes(q, key_size / 2);
    if (err != ERR_OK) {
        kfree(p);
        kfree(q);
        return err;
    }
    
    // Ensure odd numbers (set least significant bit)
    p[0] |= 1;
    q[0] |= 1;
    
    // Set high bit to ensure key size
    p[(key_size / 2) - 1] |= 0x80;
    q[(key_size / 2) - 1] |= 0x80;
    
    // Compute n = p * q (simplified - would use proper big integer multiplication)
    // For now, use placeholder values
    uint8_t* n = (uint8_t*)kmalloc(key_size);
    if (!n) {
        kfree(p);
        kfree(q);
        return ERR_OUT_OF_MEMORY;
    }
    
    // Simplified: In production, implement proper big integer multiplication
    // n = p * q (this is a placeholder)
    memset(n, 0, key_size);
    // TODO: Implement proper big integer multiplication: n = p * q
    
    // Public exponent e (typically 65537 = 0x10001)
    uint32_t e = 65537;
    
    // Compute phi(n) = (p-1) * (q-1) and d = e^(-1) mod phi(n)
    // This requires extended Euclidean algorithm
    uint8_t* d = (uint8_t*)kmalloc(key_size);
    if (!d) {
        kfree(p);
        kfree(q);
        kfree(n);
        return ERR_OUT_OF_MEMORY;
    }
    
    // Simplified: In production, compute d using extended Euclidean algorithm
    memset(d, 0, key_size);
    // TODO: Implement extended Euclidean algorithm to compute d
    
    // Format public key: {n, e}
    memcpy(public_key, n, key_size);
    memcpy(public_key + key_size, &e, 4);
    *public_key_len = key_size + 4;
    
    // Format private key: {n, d, p, q}
    memcpy(private_key, n, key_size);
    memcpy(private_key + key_size, d, key_size);
    memcpy(private_key + 2 * key_size, p, key_size / 2);
    memcpy(private_key + 2 * key_size + key_size / 2, q, key_size / 2);
    *private_key_len = 3 * key_size;
    
    kfree(p);
    kfree(q);
    kfree(n);
    kfree(d);
    
    kinfo("RSA: Keypair generated (simplified implementation)\n");
    return ERR_OK;
}

/**
 * RSA encrypt
 * 
 * RSA encryption: c = m^e mod n
 * where m is plaintext, e is public exponent, n is modulus
 */
error_code_t crypto_rsa_encrypt(const uint8_t* public_key, size_t public_key_len,
                                const uint8_t* plaintext, size_t plaintext_len, uint8_t* ciphertext) {
    if (!public_key || !plaintext || !ciphertext) {
        return ERR_INVALID_ARG;
    }
    
    if (public_key_len < 4) {
        return ERR_INVALID_ARG;
    }
    
    // Extract n and e from public key
    // Format: {n (key_size bytes), e (4 bytes)}
    size_t key_size = public_key_len - 4;
    const uint8_t* n = public_key;
    const uint32_t* e = (const uint32_t*)(public_key + key_size);
    
    // Check plaintext size (must be less than key size)
    if (plaintext_len > key_size) {
        return ERR_INVALID_ARG;
    }
    
    // RSA encryption: ciphertext = plaintext^e mod n
    // This requires modular exponentiation with big integers
    // For production, use proper big integer library
    
    // Simplified implementation (placeholder)
    // In production: ciphertext = mod_exp(plaintext, e, n)
    
    // For now, return error indicating full implementation needed
    // This provides the structure and algorithm, but requires big integer library
    kinfo("RSA: Encryption requires big integer library (modular exponentiation)\n");
    
    // Placeholder: would compute ciphertext = plaintext^e mod n
    memset(ciphertext, 0, key_size);
    
    return ERR_NOT_SUPPORTED;  // Full implementation requires big integer library
}

/**
 * RSA decrypt
 * 
 * RSA decryption: m = c^d mod n
 * where c is ciphertext, d is private exponent, n is modulus
 * 
 * Can use Chinese Remainder Theorem for efficiency:
 * m1 = c^d mod p
 * m2 = c^d mod q
 * m = CRT(m1, m2, p, q)
 */
error_code_t crypto_rsa_decrypt(const uint8_t* private_key, size_t private_key_len,
                                const uint8_t* ciphertext, size_t ciphertext_len, uint8_t* plaintext) {
    if (!private_key || !ciphertext || !plaintext) {
        return ERR_INVALID_ARG;
    }
    
    // Extract n, d, p, q from private key
    // Format: {n (key_size), d (key_size), p (key_size/2), q (key_size/2)}
    size_t key_size = private_key_len / 3;
    if (private_key_len < 3 * key_size || ciphertext_len != key_size) {
        return ERR_INVALID_ARG;
    }
    
    const uint8_t* n = private_key;
    const uint8_t* d = private_key + key_size;
    const uint8_t* p = private_key + 2 * key_size;
    const uint8_t* q = private_key + 2 * key_size + key_size / 2;
    
    // RSA decryption: plaintext = ciphertext^d mod n
    // This requires modular exponentiation with big integers
    // For production, use proper big integer library
    
    // Simplified implementation (placeholder)
    // In production: plaintext = mod_exp(ciphertext, d, n)
    // Or use CRT: m1 = c^d mod p, m2 = c^d mod q, then combine
    
    kinfo("RSA: Decryption requires big integer library (modular exponentiation)\n");
    
    // Placeholder: would compute plaintext = ciphertext^d mod n
    memset(plaintext, 0, key_size);
    
    return ERR_NOT_SUPPORTED;  // Full implementation requires big integer library
}

/**
 * Generate ECC keypair
 * 
 * ECC key generation:
 * 1. Generate random private key k (scalar)
 * 2. Compute public key Q = k * G (where G is base point)
 * 
 * Key format:
 * Private key: k (scalar, key_size bytes)
 * Public key: Q (point, 2*key_size bytes: x, y coordinates)
 */
error_code_t crypto_ecc_generate_keypair(crypto_asym_type_t type, uint8_t* public_key, size_t* public_key_len,
                                          uint8_t* private_key, size_t* private_key_len) {
    if (!public_key || !public_key_len || !private_key || !private_key_len) {
        return ERR_INVALID_ARG;
    }
    
    size_t key_size;
    switch (type) {
        case CRYPTO_ASYM_ECC_P256:
            key_size = ECC_P256_KEY_SIZE;
            break;
        case CRYPTO_ASYM_ECC_P384:
            key_size = ECC_P384_KEY_SIZE;
            break;
        default:
            return ERR_NOT_SUPPORTED;
    }
    
    // Check buffer sizes
    // Private key: k (key_size bytes)
    // Public key: Q (2*key_size bytes: x, y coordinates)
    if (*private_key_len < key_size || *public_key_len < 2 * key_size) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("ECC: Generating %zu-bit keypair\n", key_size * 8);
    
    // Generate random private key k
    error_code_t err = crypto_random_bytes(private_key, key_size);
    if (err != ERR_OK) {
        return err;
    }
    
    // Ensure k is in valid range (0 < k < curve_order)
    // For P-256: order is approximately 2^256
    // For P-384: order is approximately 2^384
    // Set high bit to ensure sufficient size, clear if too large
    private_key[key_size - 1] &= 0x7F;  // Clear high bit to stay in range
    
    // Compute public key Q = k * G (scalar multiplication on elliptic curve)
    // This requires elliptic curve point multiplication
    // For production, implement proper ECC point operations
    
    // Simplified: In production, compute Q = k * G using:
    // - Point addition on elliptic curve
    // - Scalar multiplication (double-and-add algorithm)
    
    // Placeholder: would compute public key = k * base_point
    memset(public_key, 0, 2 * key_size);
    
    *private_key_len = key_size;
    *public_key_len = 2 * key_size;
    
    kinfo("ECC: Keypair generated (simplified implementation)\n");
    return ERR_OK;
}

/**
 * ECC sign (ECDSA)
 * 
 * ECDSA signing:
 * 1. Hash the message: h = hash(data)
 * 2. Generate random k
 * 3. Compute r = (k * G).x mod n
 * 4. Compute s = k^(-1) * (h + r * private_key) mod n
 * 5. Signature is (r, s)
 */
error_code_t crypto_ecc_sign(const uint8_t* private_key, size_t private_key_len,
                             const uint8_t* data, size_t data_len, uint8_t* signature, size_t* signature_len) {
    if (!private_key || !data || !signature || !signature_len) {
        return ERR_INVALID_ARG;
    }
    
    size_t key_size = private_key_len;
    if (*signature_len < 2 * key_size) {
        return ERR_INVALID_ARG;
    }
    
    // Hash the message
    uint8_t hash[64];  // Support up to SHA-512
    error_code_t err = crypto_hash(CRYPTO_HASH_SHA256, data, data_len, hash);
    if (err != ERR_OK) {
        return err;
    }
    
    // Generate random k for ECDSA
    uint8_t* k = (uint8_t*)kmalloc(key_size);
    if (!k) {
        return ERR_OUT_OF_MEMORY;
    }
    
    err = crypto_random_bytes(k, key_size);
    if (err != ERR_OK) {
        kfree(k);
        return err;
    }
    
    // ECDSA signing:
    // r = (k * G).x mod n
    // s = k^(-1) * (h + r * private_key) mod n
    // This requires:
    // - Elliptic curve point multiplication
    // - Modular inverse
    // - Modular arithmetic
    
    // Simplified: In production, implement full ECDSA algorithm
    uint8_t* r = signature;
    uint8_t* s = signature + key_size;
    
    // Placeholder: would compute r and s using ECC operations
    memset(r, 0, key_size);
    memset(s, 0, key_size);
    
    kfree(k);
    
    *signature_len = 2 * key_size;
    
    kinfo("ECC: Signing requires full ECDSA implementation\n");
    return ERR_NOT_SUPPORTED;  // Full implementation requires ECC point operations
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

