/**
 * @file disk_encryption.c
 * @brief Disk encryption implementation
 */

#include "../include/fs/disk_encryption.h"
#include "../include/fs/block.h"
#include "../include/crypto/crypto.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"

// Header size for encrypted volumes (stores metadata)
#define ENCRYPTION_HEADER_SIZE 512  // One block
#define ENCRYPTION_MAGIC "SCARLETT_ENCRYPTED_V1"
#define ENCRYPTION_MAGIC_LEN 24

// Encryption header structure
typedef struct {
    char magic[ENCRYPTION_MAGIC_LEN];  // Magic string
    uint32_t version;                   // Format version
    uint32_t cipher_type;               // Cipher type
    uint8_t salt[16];                  // Salt for key derivation
    uint32_t iterations;                // PBKDF2 iterations
    uint8_t reserved[464];             // Reserved for future use
} encryption_header_t;

// List of encrypted devices
static encrypted_block_device_t* encrypted_devices = NULL;

/**
 * Generate IV for a block (using block number)
 */
static void generate_iv(uint64_t block_num, uint8_t* iv) {
    // Use block number as IV (XTS-like approach)
    // In production, use a proper IV scheme
    memset(iv, 0, AES_BLOCK_SIZE);
    for (int i = 0; i < 8; i++) {
        iv[i] = (uint8_t)(block_num >> (i * 8));
    }
    // XOR with a constant to ensure non-zero
    iv[0] ^= 0x5A;
}

/**
 * Encrypt block device read operation
 */
static error_code_t encrypted_read_block(block_device_t* dev, uint64_t block_num, void* buffer) {
    encrypted_block_device_t* enc_dev = (encrypted_block_device_t*)dev->private_data;
    if (!enc_dev || !enc_dev->underlying_dev) {
        return ERR_INVALID_ARG;
    }
    
    // Read encrypted block from underlying device
    uint64_t physical_block = enc_dev->encrypted_block_offset + block_num;
    error_code_t err = block_device_read(enc_dev->underlying_dev, physical_block, buffer);
    if (err != ERR_OK) {
        return err;
    }
    
    // Decrypt block
    if (enc_dev->config.enabled) {
        uint8_t iv[AES_BLOCK_SIZE];
        generate_iv(block_num, iv);
        
        uint8_t decrypted[BLOCK_SIZE];
        err = crypto_decrypt(CRYPTO_CIPHER_AES256, enc_dev->config.key, iv, 
                              (uint8_t*)buffer, BLOCK_SIZE, decrypted);
        if (err != ERR_OK) {
            return err;
        }
        
        memcpy(buffer, decrypted, BLOCK_SIZE);
    }
    
    return ERR_OK;
}

/**
 * Encrypt block device write operation
 */
static error_code_t encrypted_write_block(block_device_t* dev, uint64_t block_num, const void* buffer) {
    encrypted_block_device_t* enc_dev = (encrypted_block_device_t*)dev->private_data;
    if (!enc_dev || !enc_dev->underlying_dev) {
        return ERR_INVALID_ARG;
    }
    
    uint8_t encrypted[BLOCK_SIZE];
    
    // Encrypt block
    if (enc_dev->config.enabled) {
        uint8_t iv[AES_BLOCK_SIZE];
        generate_iv(block_num, iv);
        
        error_code_t err = crypto_encrypt(CRYPTO_CIPHER_AES256, enc_dev->config.key, iv,
                                          (const uint8_t*)buffer, BLOCK_SIZE, encrypted);
        if (err != ERR_OK) {
            return err;
        }
    } else {
        memcpy(encrypted, buffer, BLOCK_SIZE);
    }
    
    // Write encrypted block to underlying device
    uint64_t physical_block = enc_dev->encrypted_block_offset + block_num;
    return block_device_write(enc_dev->underlying_dev, physical_block, encrypted);
}

/**
 * Encrypt block device read_blocks operation
 */
static error_code_t encrypted_read_blocks(block_device_t* dev, uint64_t start_block, 
                                          uint64_t count, void* buffer) {
    encrypted_block_device_t* enc_dev = (encrypted_block_device_t*)dev->private_data;
    if (!enc_dev || !enc_dev->underlying_dev) {
        return ERR_INVALID_ARG;
    }
    
    // Read encrypted blocks from underlying device
    uint64_t physical_start = enc_dev->encrypted_block_offset + start_block;
    error_code_t err = block_device_read_blocks(enc_dev->underlying_dev, physical_start, count, buffer);
    if (err != ERR_OK) {
        return err;
    }
    
    // Decrypt each block
    if (enc_dev->config.enabled) {
        uint8_t* buf = (uint8_t*)buffer;
        for (uint64_t i = 0; i < count; i++) {
            uint8_t iv[AES_BLOCK_SIZE];
            generate_iv(start_block + i, iv);
            
            uint8_t decrypted[BLOCK_SIZE];
            err = crypto_decrypt(CRYPTO_CIPHER_AES256, enc_dev->config.key, iv,
                                buf + (i * BLOCK_SIZE), BLOCK_SIZE, decrypted);
            if (err != ERR_OK) {
                return err;
            }
            
            memcpy(buf + (i * BLOCK_SIZE), decrypted, BLOCK_SIZE);
        }
    }
    
    return ERR_OK;
}

/**
 * Encrypt block device write_blocks operation
 */
static error_code_t encrypted_write_blocks(block_device_t* dev, uint64_t start_block,
                                          uint64_t count, const void* buffer) {
    encrypted_block_device_t* enc_dev = (encrypted_block_device_t*)dev->private_data;
    if (!enc_dev || !enc_dev->underlying_dev) {
        return ERR_INVALID_ARG;
    }
    
    uint8_t* encrypted_buf = (uint8_t*)kmalloc(count * BLOCK_SIZE);
    if (!encrypted_buf) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Encrypt each block
    const uint8_t* buf = (const uint8_t*)buffer;
    if (enc_dev->config.enabled) {
        for (uint64_t i = 0; i < count; i++) {
            uint8_t iv[AES_BLOCK_SIZE];
            generate_iv(start_block + i, iv);
            
            error_code_t err = crypto_encrypt(CRYPTO_CIPHER_AES256, enc_dev->config.key, iv,
                                             buf + (i * BLOCK_SIZE), BLOCK_SIZE,
                                             encrypted_buf + (i * BLOCK_SIZE));
            if (err != ERR_OK) {
                kfree(encrypted_buf);
                return err;
            }
        }
    } else {
        memcpy(encrypted_buf, buffer, count * BLOCK_SIZE);
    }
    
    // Write encrypted blocks to underlying device
    uint64_t physical_start = enc_dev->encrypted_block_offset + start_block;
    error_code_t err = block_device_write_blocks(enc_dev->underlying_dev, physical_start, 
                                                 count, encrypted_buf);
    kfree(encrypted_buf);
    return err;
}

/**
 * Initialize disk encryption system
 */
error_code_t disk_encryption_init(void) {
    kinfo("Initializing disk encryption system...\n");
    
    encrypted_devices = NULL;
    
    // Ensure crypto library is initialized
    crypto_init();
    
    kinfo("Disk encryption system initialized\n");
    return ERR_OK;
}

/**
 * Create encrypted wrapper for a block device
 */
error_code_t disk_encryption_wrap_device(block_device_t* dev, const uint8_t* key,
                                         block_device_t** encrypted_dev) {
    if (!dev || !key || !encrypted_dev) {
        return ERR_INVALID_ARG;
    }
    
    // Allocate encrypted device structure
    encrypted_block_device_t* enc_dev = (encrypted_block_device_t*)kmalloc(sizeof(encrypted_block_device_t));
    if (!enc_dev) {
        return ERR_OUT_OF_MEMORY;
    }
    
    memset(enc_dev, 0, sizeof(encrypted_block_device_t));
    enc_dev->underlying_dev = dev;
    enc_dev->config.cipher_type = CRYPTO_CIPHER_AES256;
    memcpy(enc_dev->config.key, key, AES256_KEY_SIZE);
    enc_dev->config.enabled = true;
    enc_dev->encrypted_block_offset = 1;  // Skip header block
    
    // Allocate wrapper block device
    block_device_t* wrapper = (block_device_t*)kmalloc(sizeof(block_device_t));
    if (!wrapper) {
        kfree(enc_dev);
        return ERR_OUT_OF_MEMORY;
    }
    
    memset(wrapper, 0, sizeof(block_device_t));
    
    // Set up wrapper device
    size_t name_len = strlen(dev->name) + 5;  // "enc_" + original name + null
    char* name = (char*)kmalloc(name_len);
    if (!name) {
        kfree(wrapper);
        kfree(enc_dev);
        return ERR_OUT_OF_MEMORY;
    }
    strcpy(name, "enc_");
    strcat(name, dev->name);
    wrapper->name = (const char*)name;
    
    wrapper->block_count = dev->block_count - 1;  // Reserve one block for header
    wrapper->block_size = dev->block_size;
    wrapper->read_block = encrypted_read_block;
    wrapper->write_block = encrypted_write_block;
    wrapper->read_blocks = encrypted_read_blocks;
    wrapper->write_blocks = encrypted_write_blocks;
    wrapper->private_data = enc_dev;
    
    // Add to encrypted devices list
    enc_dev->next = encrypted_devices;
    encrypted_devices = enc_dev;
    
    *encrypted_dev = wrapper;
    
    kinfo("Wrapped block device %s with encryption\n", dev->name);
    return ERR_OK;
}

/**
 * Unwrap encrypted device
 */
block_device_t* disk_encryption_unwrap_device(block_device_t* encrypted_dev) {
    if (!encrypted_dev) {
        return NULL;
    }
    
    encrypted_block_device_t* enc_dev = (encrypted_block_device_t*)encrypted_dev->private_data;
    if (!enc_dev) {
        return NULL;
    }
    
    return enc_dev->underlying_dev;
}

/**
 * Set encryption key for a device
 */
error_code_t disk_encryption_set_key(block_device_t* dev, const uint8_t* key) {
    if (!dev || !key) {
        return ERR_INVALID_ARG;
    }
    
    encrypted_block_device_t* enc_dev = (encrypted_block_device_t*)dev->private_data;
    if (!enc_dev) {
        return ERR_INVALID_ARG;  // Not an encrypted device
    }
    
    memcpy(enc_dev->config.key, key, AES256_KEY_SIZE);
    return ERR_OK;
}

/**
 * Enable encryption for a device
 */
error_code_t disk_encryption_enable(block_device_t* dev) {
    if (!dev) {
        return ERR_INVALID_ARG;
    }
    
    encrypted_block_device_t* enc_dev = (encrypted_block_device_t*)dev->private_data;
    if (!enc_dev) {
        return ERR_INVALID_ARG;
    }
    
    enc_dev->config.enabled = true;
    return ERR_OK;
}

/**
 * Disable encryption for a device
 */
error_code_t disk_encryption_disable(block_device_t* dev) {
    if (!dev) {
        return ERR_INVALID_ARG;
    }
    
    encrypted_block_device_t* enc_dev = (encrypted_block_device_t*)dev->private_data;
    if (!enc_dev) {
        return ERR_INVALID_ARG;
    }
    
    enc_dev->config.enabled = false;
    return ERR_OK;
}

/**
 * Check if device is encrypted
 */
bool disk_encryption_is_encrypted(block_device_t* dev) {
    if (!dev) {
        return false;
    }
    
    encrypted_block_device_t* enc_dev = (encrypted_block_device_t*)dev->private_data;
    return (enc_dev != NULL);
}

/**
 * Generate encryption key
 */
error_code_t disk_encryption_generate_key(uint8_t* key, size_t key_len) {
    if (!key) {
        return ERR_INVALID_ARG;
    }
    
    if (key_len != AES256_KEY_SIZE) {
        return ERR_INVALID_ARG;
    }
    
    return crypto_random_bytes(key, key_len);
}

/**
 * Derive encryption key from password
 */
error_code_t disk_encryption_derive_key_from_password(const char* password, 
                                                      const uint8_t* salt,
                                                      uint32_t iterations,
                                                      uint8_t* key) {
    if (!password || !salt || !key) {
        return ERR_INVALID_ARG;
    }
    
    return crypto_pbkdf2(CRYPTO_HASH_SHA256, (const uint8_t*)password, strlen(password),
                        salt, 16, iterations, key, AES256_KEY_SIZE);
}

