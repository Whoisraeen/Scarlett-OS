/**
 * @file new.cpp
 * @brief C++ new/delete operators
 */

#include <stddef.h>
#include <stdint.h>

// Simple memory allocation (will use syscalls later)
extern "C" void* malloc(size_t size);
extern "C" void free(void* ptr);

void* operator new(size_t size) {
    return malloc(size);
}

void* operator new[](size_t size) {
    return malloc(size);
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete[](void* ptr) noexcept {
    free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    free(ptr);
}

