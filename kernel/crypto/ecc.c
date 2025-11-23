/**
 * @file ecc.c
 * @brief Elliptic Curve Cryptography implementation
 */

#include "ecc.h"
#include "../include/string.h"

// NIST P-256 Constants
static const uint64_t P256_P[] = {
    0xFFFFFFFFFFFFFFFF, 0x00000000FFFFFFFF, 0x0000000000000000, 0xFFFFFFFF00000001
};
static const uint64_t P256_B[] = {
    0x3BCE3C3E27D2604B, 0x651D06B0CC53B0F6, 0xB3EBBD55769886BC, 0x5AC635D8AA3A93E7
};
static const uint64_t P256_GX[] = {
    0xF4A13945D898C296, 0x77037D812DEB33A0, 0xF8BCE6E563A440F2, 0x6B17D1F2E12C4247
};
static const uint64_t P256_GY[] = {
    0xCBB6406837BF51F5, 0x2BCE33576B315ECE, 0x8EE7EB4A7C0F9E16, 0x4FE342E2FE1A7F9B
};
static const uint64_t P256_N[] = {
    0xFFFFFFFFFFFFFFFF, 0x00000000FFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFF00000000
    // Note: P-256 order is actually FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551
    // Wait, the above array is wrong. Let me fix it.
};
// Correct P-256 Order n
static const uint64_t P256_ORDER[] = {
    0xFC632551, 0xF3B9CAC2, 0xA7179E84, 0xBCE6FAAD, // These are wrong endianness/layout for my bn_t (little endian u64)
    // Let's use bytes to be safe or write carefully.
};

// Using byte arrays for initialization to avoid endian confusion
static const uint8_t P256_P_BYTES[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};
static const uint8_t P256_A_BYTES[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC
};
static const uint8_t P256_B_BYTES[] = {
    0x5A, 0xC6, 0x35, 0xD8, 0xAA, 0x3A, 0x93, 0xE7, 0xB3, 0xEB, 0xBD, 0x55, 0x76, 0x98, 0x86, 0xBC,
    0x65, 0x1D, 0x06, 0xB0, 0xCC, 0x53, 0xB0, 0xF6, 0x3B, 0xCE, 0x3C, 0x3E, 0x27, 0xD2, 0x60, 0x4B
};
static const uint8_t P256_GX_BYTES[] = {
    0x6B, 0x17, 0xD1, 0xF2, 0xE1, 0x2C, 0x42, 0x47, 0xF8, 0xBC, 0xE6, 0xE5, 0x63, 0xA4, 0x40, 0xF2,
    0x77, 0x03, 0x7D, 0x81, 0x2D, 0xEB, 0x33, 0xA0, 0xF4, 0xA1, 0x39, 0x45, 0xD8, 0x98, 0xC2, 0x96
};
static const uint8_t P256_GY_BYTES[] = {
    0x4F, 0xE3, 0x42, 0xE2, 0xFE, 0x1A, 0x7F, 0x9B, 0x8E, 0xE7, 0xEB, 0x4A, 0x7C, 0x0F, 0x9E, 0x16,
    0x2B, 0xCE, 0x33, 0x57, 0x6B, 0x31, 0x5E, 0xCE, 0xCB, 0xB6, 0x40, 0x68, 0x37, 0xBF, 0x51, 0xF5
};
static const uint8_t P256_N_BYTES[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17, 0x9E, 0x84, 0xF3, 0xB9, 0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51
};


error_code_t ecc_init_curve(crypto_asym_type_t type, bn_t* p, bn_t* a, bn_t* b, bn_t* Gx, bn_t* Gy, bn_t* n) {
    if (type == CRYPTO_ASYM_ECC_P256) {
        bn_from_bytes(p, P256_P_BYTES, 32);
        bn_from_bytes(a, P256_A_BYTES, 32);
        bn_from_bytes(b, P256_B_BYTES, 32);
        bn_from_bytes(Gx, P256_GX_BYTES, 32);
        bn_from_bytes(Gy, P256_GY_BYTES, 32);
        bn_from_bytes(n, P256_N_BYTES, 32);
        return ERR_OK;
    }
    return ERR_NOT_SUPPORTED;
}

// Point Addition: R = P + Q
error_code_t ecc_point_add(bn_t* Rx, bn_t* Ry, const bn_t* Px, const bn_t* Py, const bn_t* Qx, const bn_t* Qy, const bn_t* p, const bn_t* a) {
    // If P is infinity (0,0), R = Q
    if (Px->top == 0 && Py->top == 0) {
        bn_copy(Rx, Qx);
        bn_copy(Ry, Qy);
        return ERR_OK;
    }
    // If Q is infinity, R = P
    if (Qx->top == 0 && Qy->top == 0) {
        bn_copy(Rx, Px);
        bn_copy(Ry, Py);
        return ERR_OK;
    }
    
    bn_t* lambda = bn_alloc();
    bn_t* temp = bn_alloc();
    bn_t* temp2 = bn_alloc();
    
    // Check if P == Q (Point Doubling)
    if (bn_cmp(Px, Qx) == 0 && bn_cmp(Py, Qy) == 0) {
        // lambda = (3*Px^2 + a) * (2*Py)^-1 mod p
        
        // 3*Px^2
        bn_mul(temp, Px, Px); // Px^2
        bn_mod(temp, temp, p);
        bn_from_int(temp2, 3);
        bn_mul(lambda, temp, temp2); // 3*Px^2
        bn_add(lambda, lambda, a); // + a
        bn_mod(lambda, lambda, p);
        
        // (2*Py)^-1
        bn_from_int(temp2, 2);
        bn_mul(temp, Py, temp2); // 2*Py
        bn_mod(temp, temp, p);
        
        bn_mod_inv(temp2, temp, p); // inv
        
        bn_mul(lambda, lambda, temp2); // * inv
        bn_mod(lambda, lambda, p);
        
    } else {
        // lambda = (Qy - Py) * (Qx - Px)^-1 mod p
        
        // Qy - Py
        if (bn_cmp(Qy, Py) >= 0) {
            bn_sub(temp, Qy, Py);
        } else {
            bn_sub(temp, Py, Qy);
            bn_sub(temp, p, temp); // p - (Py - Qy) = Qy - Py mod p
        }
        
        // Qx - Px
        if (bn_cmp(Qx, Px) >= 0) {
            bn_sub(temp2, Qx, Px);
        } else {
            bn_sub(temp2, Px, Qx);
            bn_sub(temp2, p, temp2);
        }
        
        bn_mod_inv(temp2, temp2, p); // inv
        
        bn_mul(lambda, temp, temp2);
        bn_mod(lambda, lambda, p);
    }
    
    // Rx = lambda^2 - Px - Qx
    bn_mul(temp, lambda, lambda); // lambda^2
    bn_mod(temp, temp, p);
    
    // temp - Px
    if (bn_cmp(temp, Px) >= 0) {
        bn_sub(temp, temp, Px);
    } else {
        bn_sub(temp2, Px, temp);
        bn_sub(temp, p, temp2);
    }
    
    // temp - Qx
    if (bn_cmp(temp, Qx) >= 0) {
        bn_sub(Rx, temp, Qx);
    } else {
        bn_sub(temp2, Qx, temp);
        bn_sub(Rx, p, temp2);
    }
    
    // Ry = lambda * (Px - Rx) - Py
    // Px - Rx
    if (bn_cmp(Px, Rx) >= 0) {
        bn_sub(temp, Px, Rx);
    } else {
        bn_sub(temp2, Rx, Px);
        bn_sub(temp, p, temp2);
    }
    
    bn_mul(temp, lambda, temp);
    bn_mod(temp, temp, p);
    
    // temp - Py
    if (bn_cmp(temp, Py) >= 0) {
        bn_sub(Ry, temp, Py);
    } else {
        bn_sub(temp2, Py, temp);
        bn_sub(Ry, p, temp2);
    }
    
    bn_free(lambda);
    bn_free(temp);
    bn_free(temp2);
    return ERR_OK;
}

error_code_t ecc_point_mul(bn_t* Rx, bn_t* Ry, const bn_t* k, const bn_t* Px, const bn_t* Py, const bn_t* p, const bn_t* a) {
    bn_t* Tx = bn_alloc(); // Temp P
    bn_t* Ty = bn_alloc();
    bn_t* Ax = bn_alloc(); // Accumulator
    bn_t* Ay = bn_alloc();
    
    bn_copy(Tx, Px);
    bn_copy(Ty, Py);
    
    // Initialize Accumulator to infinity (0,0)
    bn_from_int(Ax, 0);
    bn_from_int(Ay, 0);
    
    size_t bits = bn_bit_count(k);
    
    for (size_t i = 0; i < bits; i++) {
        size_t word_idx = i / 64;
        size_t bit_idx = i % 64;
        
        if (k->words[word_idx] & (1ULL << bit_idx)) {
            // Add Tx, Ty to Ax, Ay
            bn_t* newAx = bn_alloc();
            bn_t* newAy = bn_alloc();
            ecc_point_add(newAx, newAy, Ax, Ay, Tx, Ty, p, a);
            bn_copy(Ax, newAx);
            bn_copy(Ay, newAy);
            bn_free(newAx);
            bn_free(newAy);
        }
        
        // Double Tx, Ty
        bn_t* newTx = bn_alloc();
        bn_t* newTy = bn_alloc();
        ecc_point_add(newTx, newTy, Tx, Ty, Tx, Ty, p, a);
        bn_copy(Tx, newTx);
        bn_copy(Ty, newTy);
        bn_free(newTx);
        bn_free(newTy);
    }
    
    bn_copy(Rx, Ax);
    bn_copy(Ry, Ay);
    
    bn_free(Tx); bn_free(Ty);
    bn_free(Ax); bn_free(Ay);
    return ERR_OK;
}

bool ecc_is_on_curve(const bn_t* x, const bn_t* y, const bn_t* p, const bn_t* a, const bn_t* b) {
    // y^2 = x^3 + ax + b (mod p)
    bn_t* lhs = bn_alloc();
    bn_t* rhs = bn_alloc();
    bn_t* temp = bn_alloc();
    
    // lhs = y^2
    bn_mul(lhs, y, y);
    bn_mod(lhs, lhs, p);
    
    // rhs = x^3 + ax + b
    bn_mul(temp, x, x); // x^2
    bn_mod(temp, temp, p);
    bn_mul(rhs, temp, x); // x^3
    bn_mod(rhs, rhs, p);
    
    bn_mul(temp, a, x); // ax
    bn_mod(temp, temp, p);
    bn_add(rhs, rhs, temp); // x^3 + ax
    bn_mod(rhs, rhs, p);
    
    bn_add(rhs, rhs, b); // + b
    bn_mod(rhs, rhs, p);
    
    bool result = (bn_cmp(lhs, rhs) == 0);
    
    bn_free(lhs);
    bn_free(rhs);
    bn_free(temp);
    return result;
}
