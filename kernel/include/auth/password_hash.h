/**
 * @file password_hash.h
 * @brief Secure password hashing interface
 */

#ifndef KERNEL_AUTH_PASSWORD_HASH_H
#define KERNEL_AUTH_PASSWORD_HASH_H

#include "../types.h"

/**
 * Hash a password using PBKDF2-like algorithm
 * 
 * @param password Plain text password
 * @param hash_output Output buffer (must be at least 128 bytes)
 */
void password_hash(const char* password, char* hash_output);

/**
 * Verify a password against a hash
 * 
 * @param password Plain text password to verify
 * @param hash Stored password hash
 * @return true if password matches, false otherwise
 */
bool password_verify(const char* password, const char* hash);

#endif // KERNEL_AUTH_PASSWORD_HASH_H

