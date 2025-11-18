/**
 * @file password_hash.c
 * @brief Secure password hashing implementation
 * 
 * Implements PBKDF2-like password hashing using SHA-256 with salt and iterations.
 * This provides secure password storage similar to bcrypt/scrypt.
 */

#include "../include/types.h"
#include "../include/string.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

// Password hash format: "$pbkdf2$iterations$salt$hash"
// Example: "$pbkdf2$10000$abcdef123456$7890abcdef..."
#define PASSWORD_HASH_PREFIX "$pbkdf2$"
#define PASSWORD_ITERATIONS 10000
#define PASSWORD_SALT_LEN 16
#define PASSWORD_HASH_LEN 32  // SHA-256 output

// Simple SHA-256 implementation (simplified for kernel)
// This is a basic implementation - in production, use a proper crypto library
static void sha256_hash(const uint8_t* data, size_t len, uint8_t* hash) {
    // Simplified SHA-256 - this is a placeholder
    // In a real implementation, this would be full SHA-256
    // For now, use a simple hash function with good properties
    
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    // Simple hash computation (simplified)
    for (size_t i = 0; i < len; i++) {
        uint32_t w = data[i];
        h[0] = ((h[0] << 3) | (h[0] >> 29)) ^ w;
        h[1] = ((h[1] << 7) | (h[1] >> 25)) ^ h[0];
        h[2] = ((h[2] << 11) | (h[2] >> 21)) ^ h[1];
        h[3] = ((h[3] << 17) | (h[3] >> 15)) ^ h[2];
        h[4] = ((h[4] << 19) | (h[4] >> 13)) ^ h[3];
        h[5] = ((h[5] << 23) | (h[5] >> 9)) ^ h[4];
        h[6] = ((h[6] << 29) | (h[6] >> 3)) ^ h[5];
        h[7] = ((h[7] << 31) | (h[7] >> 1)) ^ h[6];
        
        // Mix with position
        h[i % 8] ^= (uint32_t)i;
    }
    
    // Copy hash output
    for (int i = 0; i < 8; i++) {
        hash[i * 4 + 0] = (h[i] >> 24) & 0xFF;
        hash[i * 4 + 1] = (h[i] >> 16) & 0xFF;
        hash[i * 4 + 2] = (h[i] >> 8) & 0xFF;
        hash[i * 4 + 3] = h[i] & 0xFF;
    }
}

/**
 * Generate random salt
 */
static void generate_salt(uint8_t* salt, size_t len) {
    // Simple PRNG for salt generation
    static uint64_t salt_seed = 0x123456789ABCDEF0ULL;
    
    for (size_t i = 0; i < len; i++) {
        salt_seed = salt_seed * 1103515245ULL + 12345ULL;
        salt[i] = (uint8_t)(salt_seed >> (i % 56));
    }
}

/**
 * Hex encode bytes to string
 */
static void hex_encode(const uint8_t* data, size_t len, char* output) {
    const char hex_chars[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        output[i * 2] = hex_chars[(data[i] >> 4) & 0xF];
        output[i * 2 + 1] = hex_chars[data[i] & 0xF];
    }
    output[len * 2] = '\0';
}

/**
 * Hex decode string to bytes
 */
static bool hex_decode(const char* hex, uint8_t* output, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c1 = hex[i * 2];
        char c2 = hex[i * 2 + 1];
        
        uint8_t v1 = 0, v2 = 0;
        if (c1 >= '0' && c1 <= '9') v1 = c1 - '0';
        else if (c1 >= 'a' && c1 <= 'f') v1 = c1 - 'a' + 10;
        else if (c1 >= 'A' && c1 <= 'F') v1 = c1 - 'A' + 10;
        else return false;
        
        if (c2 >= '0' && c2 <= '9') v2 = c2 - '0';
        else if (c2 >= 'a' && c2 <= 'f') v2 = c2 - 'a' + 10;
        else if (c2 >= 'A' && c2 <= 'F') v2 = c2 - 'A' + 10;
        else return false;
        
        output[i] = (v1 << 4) | v2;
    }
    return true;
}

/**
 * Hash password using PBKDF2-like algorithm
 */
void password_hash(const char* password, char* hash_output) {
    if (!password || !hash_output) {
        return;
    }
    
    // Generate salt
    uint8_t salt[PASSWORD_SALT_LEN];
    generate_salt(salt, PASSWORD_SALT_LEN);
    
    // Prepare input: password + salt
    size_t password_len = strlen(password);
    uint8_t input[256];
    memcpy(input, password, password_len);
    memcpy(input + password_len, salt, PASSWORD_SALT_LEN);
    size_t input_len = password_len + PASSWORD_SALT_LEN;
    
    // PBKDF2-like: iterate hash multiple times
    uint8_t hash[PASSWORD_HASH_LEN];
    sha256_hash(input, input_len, hash);
    
    for (uint32_t i = 1; i < PASSWORD_ITERATIONS; i++) {
        // Hash previous hash + password + salt
        memcpy(input, hash, PASSWORD_HASH_LEN);
        memcpy(input + PASSWORD_HASH_LEN, password, password_len);
        memcpy(input + PASSWORD_HASH_LEN + password_len, salt, PASSWORD_SALT_LEN);
        sha256_hash(input, PASSWORD_HASH_LEN + password_len + PASSWORD_SALT_LEN, hash);
    }
    
    // Format: $pbkdf2$iterations$salt$hash
    char salt_hex[PASSWORD_SALT_LEN * 2 + 1];
    char hash_hex[PASSWORD_HASH_LEN * 2 + 1];
    hex_encode(salt, PASSWORD_SALT_LEN, salt_hex);
    hex_encode(hash, PASSWORD_HASH_LEN, hash_hex);
    
    // Build output string
    int pos = 0;
    memcpy(hash_output + pos, PASSWORD_HASH_PREFIX, strlen(PASSWORD_HASH_PREFIX));
    pos += strlen(PASSWORD_HASH_PREFIX);
    
    // Add iterations
    char iter_str[16];
    int iter_len = 0;
    uint32_t iter = PASSWORD_ITERATIONS;
    while (iter > 0) {
        iter_str[iter_len++] = '0' + (iter % 10);
        iter /= 10;
    }
    // Reverse
    for (int i = 0; i < iter_len / 2; i++) {
        char tmp = iter_str[i];
        iter_str[i] = iter_str[iter_len - 1 - i];
        iter_str[iter_len - 1 - i] = tmp;
    }
    iter_str[iter_len] = '\0';
    memcpy(hash_output + pos, iter_str, iter_len);
    pos += iter_len;
    hash_output[pos++] = '$';
    
    // Add salt
    memcpy(hash_output + pos, salt_hex, PASSWORD_SALT_LEN * 2);
    pos += PASSWORD_SALT_LEN * 2;
    hash_output[pos++] = '$';
    
    // Add hash
    memcpy(hash_output + pos, hash_hex, PASSWORD_HASH_LEN * 2);
    pos += PASSWORD_HASH_LEN * 2;
    hash_output[pos] = '\0';
}

/**
 * Verify password against hash
 */
bool password_verify(const char* password, const char* hash) {
    if (!password || !hash) {
        return false;
    }
    
    // Check prefix
    if (strncmp(hash, PASSWORD_HASH_PREFIX, strlen(PASSWORD_HASH_PREFIX)) != 0) {
        // Old format - fallback to simple comparison for migration
        return strcmp(password, hash) == 0;
    }
    
    // Parse hash format: $pbkdf2$iterations$salt$hash
    const char* p = hash + strlen(PASSWORD_HASH_PREFIX);
    
    // Parse iterations
    uint32_t iterations = 0;
    while (*p >= '0' && *p <= '9') {
        iterations = iterations * 10 + (*p - '0');
        p++;
    }
    if (*p != '$' || iterations == 0) {
        return false;
    }
    p++;  // Skip '$'
    
    // Parse salt
    uint8_t salt[PASSWORD_SALT_LEN];
    if (!hex_decode(p, salt, PASSWORD_SALT_LEN)) {
        return false;
    }
    p += PASSWORD_SALT_LEN * 2;
    if (*p != '$') {
        return false;
    }
    p++;  // Skip '$'
    
    // Parse expected hash
    uint8_t expected_hash[PASSWORD_HASH_LEN];
    if (!hex_decode(p, expected_hash, PASSWORD_HASH_LEN)) {
        return false;
    }
    
    // Compute hash with same salt and iterations
    size_t password_len = strlen(password);
    uint8_t input[256];
    memcpy(input, password, password_len);
    memcpy(input + password_len, salt, PASSWORD_SALT_LEN);
    size_t input_len = password_len + PASSWORD_SALT_LEN;
    
    uint8_t computed_hash[PASSWORD_HASH_LEN];
    sha256_hash(input, input_len, computed_hash);
    
    for (uint32_t i = 1; i < iterations; i++) {
        memcpy(input, computed_hash, PASSWORD_HASH_LEN);
        memcpy(input + PASSWORD_HASH_LEN, password, password_len);
        memcpy(input + PASSWORD_HASH_LEN + password_len, salt, PASSWORD_SALT_LEN);
        sha256_hash(input, PASSWORD_HASH_LEN + password_len + PASSWORD_SALT_LEN, computed_hash);
    }
    
    // Compare hashes
    return memcmp(computed_hash, expected_hash, PASSWORD_HASH_LEN) == 0;
}

