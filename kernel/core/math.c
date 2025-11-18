/**
 * @file math.c
 * @brief Basic math function implementations
 */

#include "../include/math.h"

/**
 * Absolute value
 */
int abs(int x) {
    return (x < 0) ? -x : x;
}

long labs(long x) {
    return (x < 0) ? -x : x;
}

long long llabs(long long x) {
    return (x < 0) ? -x : x;
}

/**
 * Power function (simple implementation)
 */
double pow(double x, double y) {
    if (y == 0.0) return 1.0;
    if (y == 1.0) return x;
    if (x == 0.0) return 0.0;
    
    // For integer powers, use repeated multiplication
    if (y == (int)y && y > 0 && y < 100) {
        double result = 1.0;
        for (int i = 0; i < (int)y; i++) {
            result *= x;
        }
        return result;
    }
    
    // For negative powers
    if (y < 0) {
        return 1.0 / pow(x, -y);
    }
    
    // For fractional powers, use approximation
    // This is a very basic implementation
    return exp(y * log(x));
}

float powf(float x, float y) {
    return (float)pow((double)x, (double)y);
}

/**
 * Square root (Newton's method)
 */
double sqrt(double x) {
    if (x < 0.0) return 0.0;  // Error case
    if (x == 0.0 || x == 1.0) return x;
    
    double guess = x / 2.0;
    double prev = 0.0;
    
    // Iterate until convergence
    for (int i = 0; i < 20; i++) {
        prev = guess;
        guess = (guess + x / guess) / 2.0;
        if (guess == prev) break;
    }
    
    return guess;
}

float sqrtf(float x) {
    return (float)sqrt((double)x);
}

/**
 * Sine function (Taylor series approximation)
 */
double sin(double x) {
    // Normalize to [-2π, 2π]
    while (x > 2.0 * M_PI) x -= 2.0 * M_PI;
    while (x < -2.0 * M_PI) x += 2.0 * M_PI;
    
    double result = 0.0;
    double term = x;
    double x_squared = x * x;
    
    // Taylor series: x - x³/3! + x⁵/5! - x⁷/7! + ...
    for (int i = 1; i <= 15; i += 2) {
        result += term;
        term = -term * x_squared / ((i + 1) * (i + 2));
    }
    
    return result;
}

/**
 * Cosine function
 */
double cos(double x) {
    // cos(x) = sin(x + π/2)
    return sin(x + M_PI / 2.0);
}

/**
 * Tangent function
 */
double tan(double x) {
    double c = cos(x);
    if (c == 0.0) return 0.0;  // Avoid division by zero
    return sin(x) / c;
}

/**
 * Exponential function (Taylor series)
 */
double exp(double x) {
    if (x == 0.0) return 1.0;
    
    double result = 1.0;
    double term = 1.0;
    
    // Taylor series: 1 + x + x²/2! + x³/3! + ...
    for (int i = 1; i < 20; i++) {
        term *= x / i;
        result += term;
        if (term < 1e-10) break;  // Convergence check
    }
    
    return result;
}

/**
 * Natural logarithm (Newton's method)
 */
double log(double x) {
    if (x <= 0.0) return 0.0;  // Error case
    if (x == 1.0) return 0.0;
    
    // Use approximation: log(x) ≈ 2 * (x-1)/(x+1) for x near 1
    // For better accuracy, use series expansion
    double result = 0.0;
    double y = (x - 1.0) / (x + 1.0);
    double y_squared = y * y;
    double term = y;
    
    // Series: 2 * (y + y³/3 + y⁵/5 + ...)
    for (int i = 1; i < 30; i += 2) {
        result += term / i;
        term *= y_squared;
        if (term < 1e-10) break;
    }
    
    return 2.0 * result;
}

/**
 * Base-10 logarithm
 */
double log10(double x) {
    return log(x) / log(10.0);
}

/**
 * Floor function
 */
double floor(double x) {
    if (x >= 0.0) {
        return (double)(long long)x;
    } else {
        long long i = (long long)x;
        if ((double)i == x) {
            return (double)i;
        } else {
            return (double)(i - 1);
        }
    }
}

/**
 * Ceiling function
 */
double ceil(double x) {
    if (x <= 0.0) {
        return (double)(long long)x;
    } else {
        long long i = (long long)x;
        if ((double)i == x) {
            return (double)i;
        } else {
            return (double)(i + 1);
        }
    }
}

/**
 * Round function
 */
double round(double x) {
    if (x >= 0.0) {
        return floor(x + 0.5);
    } else {
        return ceil(x - 0.5);
    }
}

