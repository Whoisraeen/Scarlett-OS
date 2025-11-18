/**
 * @file math.h
 * @brief Basic math functions
 */

#ifndef KERNEL_MATH_H
#define KERNEL_MATH_H

#include "types.h"

// Absolute value
int abs(int x);
long labs(long x);
long long llabs(long long x);

// Minimum/Maximum
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

// Power functions
double pow(double x, double y);
float powf(float x, float y);

// Square root
double sqrt(double x);
float sqrtf(float x);

// Trigonometric functions (basic)
double sin(double x);
double cos(double x);
double tan(double x);

// Exponential and logarithmic
double exp(double x);
double log(double x);
double log10(double x);

// Rounding
double floor(double x);
double ceil(double x);
double round(double x);

// Constants
#define M_PI 3.14159265358979323846
#define M_E 2.71828182845904523536

#endif // KERNEL_MATH_H

