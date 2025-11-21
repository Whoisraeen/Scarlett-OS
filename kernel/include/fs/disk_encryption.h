/**
 * @file disk_encryption.h
 * @brief Disk encryption interface
 */

#ifndef KERNEL_FS_DISK_ENCRYPTION_H
#define KERNEL_FS_DISK_ENCRYPTION_H

#include "../types.h"
#include "../errors.h"
#include "block.h"
#include "../crypto/crypto.h"

// Encryption configuration
typedef struct {
    crypto_cipher_type_t cipher_type;  // AES-256
    uint8_t key[AES256_KEY_SIZE];      // Encryption key
    bool enabled;                       // Is encryption enabled?
} disk_encryption_config_t;

// Encrypted block device wrapper
typedef struct {
    block_device_t* underlying_dev;    // Underlying block device
    disk_encryption_config_t config;   // Encryption configuration
    uint64_t encrypted_block_offset;   // Offset to encrypted region (skip header)
} encrypted_block_device_t;

// Initialize disk encryption system
error_code_t disk_encryption_init(void);

// Create encrypted wrapper for a block device
error_code_t disk_encryption_wrap_device(block_device_t* dev, const uint8_t* key, 
                                         block_device_t** encrypted_dev);

// Unwrap encrypted device (get underlying device)
block_device_t* disk_encryption_unwrap_device(block_device_t* encrypted_dev);

// Set encryption key for a device
error_code_t disk_encryption_set_key(block_device_t* dev, const uint8_t* key);

// Enable/disable encryption for a device
error_code_t disk_encryption_enable(block_device_t* dev);
error_code_t disk_encryption_disable(block_device_t* dev);

// Check if device is encrypted
bool disk_encryption_is_encrypted(block_device_t* dev);

// Generate encryption key (uses RNG)
error_code_t disk_encryption_generate_key(uint8_t* key, size_t key_len);

// Derive encryption key from password (PBKDF2)
error_code_t disk_encryption_derive_key_from_password(const char* password, 
                                                       const uint8_t* salt, 
                                                       uint32_t iterations,
                                                       uint8_t* key);

#endif // KERNEL_FS_DISK_ENCRYPTION_H

