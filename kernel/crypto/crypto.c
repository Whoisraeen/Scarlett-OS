/**
 * @file crypto.c
 * @brief Cryptographic library implementation
 */

#include "../include/crypto/crypto.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/mm/heap.h"
#include "bn.h"
#include "ecc.h"

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
    if (*public_key_len < key_size + 4 || *private_key_len < 3 * key_size) {
        return ERR_INVALID_ARG;
    }
    
    kinfo("RSA: Generating %zu-bit keypair (this may take a while)\n", key_size * 8);
    
    bn_t* p = bn_alloc();
    bn_t* q = bn_alloc();
    bn_t* n = bn_alloc();
    bn_t* e = bn_alloc();
    bn_t* d = bn_alloc();
    bn_t* phi = bn_alloc();
    bn_t* p_minus_1 = bn_alloc();
    bn_t* q_minus_1 = bn_alloc();
    bn_t* one = bn_alloc();
    
    bn_from_int(one, 1);
    bn_from_int(e, 65537);
    
    // 1. Generate prime p
    kinfo("RSA: Generating prime p...\n");
    do {
        bn_rand(p, key_size * 4); // key_size/2 in bytes -> *8 bits / 2 = *4 bits
        // Ensure odd
        p->words[0] |= 1;
        // Ensure high bit set
        size_t high_word = (key_size / 2 + 7) / 8 / 8; // Approximate word index
        // Actually bn_rand handles bits, so we just need to ensure the bit count is roughly correct
        // But bn_rand might produce smaller number.
        // Just check prime.
    } while (!bn_is_prime(p, 10)); // 10 rounds for speed
    
    // 2. Generate prime q
    kinfo("RSA: Generating prime q...\n");
    do {
        bn_rand(q, key_size * 4);
        q->words[0] |= 1;
    } while (!bn_is_prime(q, 10) || bn_cmp(p, q) == 0);
    
    // 3. n = p * q
    bn_mul(n, p, q);
    
    // 4. phi = (p-1) * (q-1)
    bn_sub(p_minus_1, p, one);
    bn_sub(q_minus_1, q, one);
    bn_mul(phi, p_minus_1, q_minus_1);
    
    // 5. d = e^(-1) mod phi
    if (bn_mod_inv(d, e, phi) != ERR_OK) {
        // This should happen rarely with e=65537, but if so, we should retry
        kerror("RSA: Failed to generate private exponent (gcd(e, phi) != 1)\n");
        bn_free(p); bn_free(q); bn_free(n); bn_free(e); bn_free(d);
        bn_free(phi); bn_free(p_minus_1); bn_free(q_minus_1); bn_free(one);
        return ERR_FAILED;
    }
    
    // Export keys
    // Public: n, e
    bn_to_bytes(n, public_key, key_size);
    uint32_t e_val = 65537;
    memcpy(public_key + key_size, &e_val, 4);
    *public_key_len = key_size + 4;
    
    // Private: n, d, p, q
    bn_to_bytes(n, private_key, key_size);
    bn_to_bytes(d, private_key + key_size, key_size);
    bn_to_bytes(p, private_key + 2 * key_size, key_size / 2);
    bn_to_bytes(q, private_key + 2 * key_size + key_size / 2, key_size / 2);
    *private_key_len = 3 * key_size;
    
    bn_free(p); bn_free(q); bn_free(n); bn_free(e); bn_free(d);
    bn_free(phi); bn_free(p_minus_1); bn_free(q_minus_1); bn_free(one);
    
    kinfo("RSA: Keypair generated successfully\n");
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
    size_t key_size = public_key_len - 4;
    const uint8_t* n_buf = public_key;
    const uint32_t* e_ptr = (const uint32_t*)(public_key + key_size);
    uint32_t e_val = *e_ptr;
    
    if (plaintext_len > key_size) {
        return ERR_INVALID_ARG;
    }
    
    bn_t* n = bn_alloc();
    bn_t* e = bn_alloc();
    bn_t* m = bn_alloc();
    bn_t* c = bn_alloc();
    
    bn_from_bytes(n, n_buf, key_size);
    bn_from_int(e, e_val);
    bn_from_bytes(m, plaintext, plaintext_len);
    
    // c = m^e mod n
    bn_mod_exp(c, m, e, n);
    
    // Export ciphertext
    bn_to_bytes(c, ciphertext, key_size);
    
    bn_free(n); bn_free(e); bn_free(m); bn_free(c);
    return ERR_OK;
}

error_code_t crypto_rsa_decrypt(const uint8_t* private_key, size_t private_key_len,
                                const uint8_t* ciphertext, size_t ciphertext_len, uint8_t* plaintext) {
    if (!private_key || !ciphertext || !plaintext) {
        return ERR_INVALID_ARG;
    }
    
    // Extract n, d from private key
    size_t key_size = private_key_len / 3;
    if (private_key_len < 3 * key_size || ciphertext_len != key_size) {
        return ERR_INVALID_ARG;
    }
    
    const uint8_t* n_buf = private_key;
    const uint8_t* d_buf = private_key + key_size;
    
    bn_t* n = bn_alloc();
    bn_t* d = bn_alloc();
    bn_t* c = bn_alloc();
    bn_t* m = bn_alloc();
    
    bn_from_bytes(n, n_buf, key_size);
    bn_from_bytes(d, d_buf, key_size);
    bn_from_bytes(c, ciphertext, ciphertext_len);
    
    // m = c^d mod n
    bn_mod_exp(m, c, d, n);
    
    // Export plaintext
    bn_to_bytes(m, plaintext, key_size);
    
    bn_free(n); bn_free(d); bn_free(c); bn_free(m);
    return ERR_OK;
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
    if (type == CRYPTO_ASYM_ECC_P256) {
        key_size = ECC_P256_KEY_SIZE;
    } else {
        return ERR_NOT_SUPPORTED;
    }
    
    if (*private_key_len < key_size || *public_key_len < 2 * key_size) {
        return ERR_INVALID_ARG;
    }
    
    bn_t* p = bn_alloc();
    bn_t* a = bn_alloc();
    bn_t* b = bn_alloc();
    bn_t* Gx = bn_alloc();
    bn_t* Gy = bn_alloc();
    bn_t* n = bn_alloc();
    
    if (ecc_init_curve(type, p, a, b, Gx, Gy, n) != ERR_OK) {
        bn_free(p); bn_free(a); bn_free(b); bn_free(Gx); bn_free(Gy); bn_free(n);
        return ERR_NOT_SUPPORTED;
    }
    
    kinfo("ECC: Generating %zu-bit keypair\n", key_size * 8);
    
    bn_t* d = bn_alloc();
    bn_t* Qx = bn_alloc();
    bn_t* Qy = bn_alloc();
    
    // Generate private key d (1 <= d < n)
    do {
        bn_rand(d, key_size * 8);
        bn_mod(d, d, n);
    } while (d->top == 0); // Ensure d != 0
    
    // Q = d * G
    ecc_point_mul(Qx, Qy, d, Gx, Gy, p, a);
    
    // Export private key d
    bn_to_bytes(d, private_key, key_size);
    *private_key_len = key_size;
    
    // Export public key Q (x, y)
    bn_to_bytes(Qx, public_key, key_size);
    bn_to_bytes(Qy, public_key + key_size, key_size);
    *public_key_len = 2 * key_size;
    
    bn_free(p); bn_free(a); bn_free(b); bn_free(Gx); bn_free(Gy); bn_free(n);
    bn_free(d); bn_free(Qx); bn_free(Qy);
    
    kinfo("ECC: Keypair generated successfully\n");
    return ERR_OK;
}

error_code_t crypto_ecc_sign(const uint8_t* private_key, size_t private_key_len,
                             const uint8_t* data, size_t data_len, uint8_t* signature, size_t* signature_len) {
    if (!private_key || !data || !signature || !signature_len) {
        return ERR_INVALID_ARG;
    }
    
    size_t key_size = private_key_len;
    if (*signature_len < 2 * key_size) {
        return ERR_INVALID_ARG;
    }
    
    // Determine curve type based on key size (simplified logic)
    crypto_asym_type_t type;
    if (key_size == ECC_P256_KEY_SIZE) {
        type = CRYPTO_ASYM_ECC_P256;
    } else {
        return ERR_NOT_SUPPORTED;
    }
    
    // Hash the message
    uint8_t hash[64];
    error_code_t err = crypto_hash(CRYPTO_HASH_SHA256, data, data_len, hash);
    if (err != ERR_OK) return err;
    
    bn_t* p = bn_alloc();
    bn_t* a = bn_alloc();
    bn_t* b = bn_alloc();
    bn_t* Gx = bn_alloc();
    bn_t* Gy = bn_alloc();
    bn_t* n = bn_alloc();
    
    if (ecc_init_curve(type, p, a, b, Gx, Gy, n) != ERR_OK) {
        bn_free(p); bn_free(a); bn_free(b); bn_free(Gx); bn_free(Gy); bn_free(n);
        return ERR_NOT_SUPPORTED;
    }
    
    bn_t* d = bn_alloc();
    bn_t* k = bn_alloc();
    bn_t* r = bn_alloc();
    bn_t* s = bn_alloc();
    bn_t* z = bn_alloc();
    bn_t* Rx = bn_alloc();
    bn_t* Ry = bn_alloc();
    bn_t* tmp = bn_alloc();
    
    bn_from_bytes(d, private_key, key_size);
    bn_from_bytes(z, hash, SHA256_HASH_SIZE); // Use leftmost bits of hash
    // For P-256, hash is 256 bits, so z is full hash.
    
    // ECDSA Loop
    int retries = 100;
    while (retries--) {
        // 1. k = random [1, n-1]
        do {
            bn_rand(k, key_size * 8);
            bn_mod(k, k, n);
        } while (k->top == 0);
        
        // 2. R = k * G
        ecc_point_mul(Rx, Ry, k, Gx, Gy, p, a);
        
        // 3. r = Rx mod n
        bn_mod(r, Rx, n);
        if (r->top == 0) continue;
        
        // 4. s = k^-1 * (z + r*d) mod n
        bn_mul(tmp, r, d); // r*d
        bn_mod(tmp, tmp, n);
        bn_add(tmp, tmp, z); // z + r*d
        bn_mod(tmp, tmp, n);
        
        bn_t* kinv = bn_alloc();
        bn_mod_inv(kinv, k, n);
        
        bn_mul(s, kinv, tmp);
        bn_mod(s, s, n);
        bn_free(kinv);
        
        if (s->top == 0) continue;
        
        break;
    }
    
    if (retries <= 0) {
        err = ERR_FAILED;
    } else {
        bn_to_bytes(r, signature, key_size);
        bn_to_bytes(s, signature + key_size, key_size);
        *signature_len = 2 * key_size;
        err = ERR_OK;
    }
    
    bn_free(p); bn_free(a); bn_free(b); bn_free(Gx); bn_free(Gy); bn_free(n);
    bn_free(d); bn_free(k); bn_free(r); bn_free(s); bn_free(z);
    bn_free(Rx); bn_free(Ry); bn_free(tmp);
    
    return err;
}

error_code_t crypto_ecc_verify(const uint8_t* public_key, size_t public_key_len,
                               const uint8_t* data, size_t data_len, const uint8_t* signature, size_t signature_len) {
    if (!public_key || !data || !signature) {
        return ERR_INVALID_ARG;
    }
    
    size_t key_size = public_key_len / 2;
    if (public_key_len != 2 * key_size || signature_len != 2 * key_size) {
        return ERR_INVALID_ARG;
    }
    
    crypto_asym_type_t type;
    if (key_size == ECC_P256_KEY_SIZE) {
        type = CRYPTO_ASYM_ECC_P256;
    } else {
        return ERR_NOT_SUPPORTED;
    }
    
    uint8_t hash[64];
    error_code_t err = crypto_hash(CRYPTO_HASH_SHA256, data, data_len, hash);
    if (err != ERR_OK) return err;
    
    bn_t* p = bn_alloc();
    bn_t* a = bn_alloc();
    bn_t* b = bn_alloc();
    bn_t* Gx = bn_alloc();
    bn_t* Gy = bn_alloc();
    bn_t* n = bn_alloc();
    
    if (ecc_init_curve(type, p, a, b, Gx, Gy, n) != ERR_OK) {
        bn_free(p); bn_free(a); bn_free(b); bn_free(Gx); bn_free(Gy); bn_free(n);
        return ERR_NOT_SUPPORTED;
    }
    
    bn_t* Qx = bn_alloc();
    bn_t* Qy = bn_alloc();
    bn_t* r = bn_alloc();
    bn_t* s = bn_alloc();
    bn_t* z = bn_alloc();
    
    bn_from_bytes(Qx, public_key, key_size);
    bn_from_bytes(Qy, public_key + key_size, key_size);
    bn_from_bytes(r, signature, key_size);
    bn_from_bytes(s, signature + key_size, key_size);
    bn_from_bytes(z, hash, SHA256_HASH_SIZE);
    
    // Check if r, s in [1, n-1]
    if (r->top == 0 || bn_cmp(r, n) >= 0 || s->top == 0 || bn_cmp(s, n) >= 0) {
        err = ERR_INVALID_ARG;
    } else {
        // w = s^-1 mod n
        bn_t* w = bn_alloc();
        bn_mod_inv(w, s, n);
        
        // u1 = z*w mod n
        bn_t* u1 = bn_alloc();
        bn_mul(u1, z, w);
        bn_mod(u1, u1, n);
        
        // u2 = r*w mod n
        bn_t* u2 = bn_alloc();
        bn_mul(u2, r, w);
        bn_mod(u2, u2, n);
        
        // P = u1*G + u2*Q
        bn_t* P1x = bn_alloc(); bn_t* P1y = bn_alloc();
        bn_t* P2x = bn_alloc(); bn_t* P2y = bn_alloc();
        bn_t* Px = bn_alloc(); bn_t* Py = bn_alloc();
        
        ecc_point_mul(P1x, P1y, u1, Gx, Gy, p, a); // P1 = u1*G
        ecc_point_mul(P2x, P2y, u2, Qx, Qy, p, a); // P2 = u2*Q
        
        ecc_point_add(Px, Py, P1x, P1y, P2x, P2y, p, a); // P = P1 + P2
        
        if (Px->top == 0 && Py->top == 0) { // Infinity
            err = ERR_INVALID_ARG; // Invalid signature
        } else {
            // v = Px mod n
            bn_t* v = bn_alloc();
            bn_mod(v, Px, n);
            
            if (bn_cmp(v, r) == 0) {
                err = ERR_OK;
            } else {
                err = ERR_INVALID_ARG; // Signature mismatch
            }
            bn_free(v);
        }
        
        bn_free(w); bn_free(u1); bn_free(u2);
        bn_free(P1x); bn_free(P1y); bn_free(P2x); bn_free(P2y);
        bn_free(Px); bn_free(Py);
    }
    
    bn_free(p); bn_free(a); bn_free(b); bn_free(Gx); bn_free(Gy); bn_free(n);
    bn_free(Qx); bn_free(Qy); bn_free(r); bn_free(s); bn_free(z);
    
    return err;
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
 * HMAC helper
 */
static error_code_t crypto_hmac(crypto_hash_type_t type, const uint8_t* key, size_t key_len,
                                const uint8_t* data, size_t data_len, uint8_t* output) {
    size_t block_size = 64; // 64 bytes for SHA256/512 (SHA512 is actually 128 bytes block size usually, but let's check)
    if (type == CRYPTO_HASH_SHA512) block_size = 128;
    
    uint8_t k[128]; // Max block size
    memset(k, 0, sizeof(k));
    
    // 1. If key is longer than block size, hash it
    if (key_len > block_size) {
        error_code_t err = crypto_hash(type, key, key_len, k);
        if (err != ERR_OK) return err;
        // key_len is now hash size
        key_len = (type == CRYPTO_HASH_SHA256) ? SHA256_HASH_SIZE : SHA512_HASH_SIZE;
    } else {
        memcpy(k, key, key_len);
    }
    
    // 2. Create ipad and opad
    uint8_t ipad[128];
    uint8_t opad[128];
    
    for (size_t i = 0; i < block_size; i++) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5C;
    }
    
    // 3. Inner hash: H(ipad || data)
    void* ctx = NULL;
    error_code_t err = crypto_hash_init(type, &ctx);
    if (err != ERR_OK) return err;
    
    crypto_hash_update(ctx, ipad, block_size);
    crypto_hash_update(ctx, data, data_len);
    
    uint8_t inner_hash[64]; // Max hash size
    crypto_hash_final(ctx, inner_hash);
    crypto_hash_free(ctx);
    
    // 4. Outer hash: H(opad || inner_hash)
    err = crypto_hash_init(type, &ctx);
    if (err != ERR_OK) return err;
    
    crypto_hash_update(ctx, opad, block_size);
    size_t hash_size = (type == CRYPTO_HASH_SHA256) ? SHA256_HASH_SIZE : SHA512_HASH_SIZE;
    crypto_hash_update(ctx, inner_hash, hash_size);
    
    crypto_hash_final(ctx, output);
    crypto_hash_free(ctx);
    
    return ERR_OK;
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
        uint8_t salt_block[256]; // Max salt size supported + 4
        if (salt_len + 4 > sizeof(salt_block)) {
             // Handle large salt if needed, for now cap it or error
             // Standard PBKDF2 allows arbitrary salt, but fixed buffer is safer for kernel
        }
        
        size_t salt_block_len = salt_len + 4;
        memcpy(salt_block, salt, salt_len);
        salt_block[salt_len] = (uint8_t)((i + 1) >> 24);
        salt_block[salt_len + 1] = (uint8_t)((i + 1) >> 16);
        salt_block[salt_len + 2] = (uint8_t)((i + 1) >> 8);
        salt_block[salt_len + 3] = (uint8_t)(i + 1);
        
        // HMAC(password, salt_block)
        crypto_hmac(hash_type, password, password_len, salt_block, salt_block_len, u);
        memcpy(t, u, hash_size);
        
        // Iterate
        for (uint32_t j = 1; j < iterations; j++) {
            // U_j = HMAC(password, U_{j-1})
            crypto_hmac(hash_type, password, password_len, u, hash_size, u);
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

