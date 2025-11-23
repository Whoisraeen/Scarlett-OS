/**
 * @file ecc.h
 * @brief Elliptic Curve Cryptography helper functions
 */

#ifndef KERNEL_CRYPTO_ECC_H
#define KERNEL_CRYPTO_ECC_H

#include "../include/crypto/crypto.h"
#include "bn.h"

// Initialize ECC parameters for specific curve
error_code_t ecc_init_curve(crypto_asym_type_t type, bn_t* p, bn_t* a, bn_t* b, bn_t* Gx, bn_t* Gy, bn_t* n);

// ECC Point Multiplication: R = k * P
error_code_t ecc_point_mul(bn_t* Rx, bn_t* Ry, const bn_t* k, const bn_t* Px, const bn_t* Py, const bn_t* p, const bn_t* a);

// ECC Point Addition: R = P + Q
error_code_t ecc_point_add(bn_t* Rx, bn_t* Ry, const bn_t* Px, const bn_t* Py, const bn_t* Qx, const bn_t* Qy, const bn_t* p, const bn_t* a);

// Check if point is on curve
bool ecc_is_on_curve(const bn_t* x, const bn_t* y, const bn_t* p, const bn_t* a, const bn_t* b);

#endif // KERNEL_CRYPTO_ECC_H
