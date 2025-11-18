#include "../include/string.h"
#include "../include/mm/heap.h"

/**
 * @brief Copy memory area
 */
void* memcpy(void* dest, const void* src, size_t n) {
    if (!dest || !src) return dest;
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

/**
 * @brief Move memory area (handles overlapping regions)
 */
void* memmove(void* dest, const void* src, size_t n) {
    if (!dest || !src) return dest;
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    
    if (d < s) {
        // Copy forward
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        // Copy backward
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

/**
 * @brief Fill memory with a constant byte
 */
void* memset(void* s, int c, size_t n) {
    if (!s) return s;
    unsigned char* p = (unsigned char*)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    return s;
}

/**
 * @brief Compare memory areas
 */
int memcmp(const void* s1, const void* s2, size_t n) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : 1;
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return (int)p1[i] - (int)p2[i];
        }
    }
    return 0;
}

/**
 * @brief Search for a byte in memory
 */
void* memchr(const void* s, int c, size_t n) {
    if (!s) return NULL;
    const unsigned char* p = (const unsigned char*)s;
    unsigned char ch = (unsigned char)c;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == ch) {
            return (void*)(p + i);
        }
    }
    return NULL;
}

/**
 * @brief Calculate the length of a string
 */
size_t strlen(const char* str) {
    if (!str) return 0;
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

/**
 * @brief Copy string
 */
char* strcpy(char* dest, const char* src) {
    if (!dest || !src) return dest;
    char* d = dest;
    while (*src) {
        *d++ = *src++;
    }
    *d = '\0';
    return dest;
}

/**
 * @brief Copy string with length limit
 */
char* strncpy(char* dest, const char* src, size_t n) {
    if (!dest || !src) return dest;
    char* d = dest;
    size_t i = 0;
    while (i < n && *src) {
        *d++ = *src++;
        i++;
    }
    while (i < n) {
        *d++ = '\0';
        i++;
    }
    return dest;
}

/**
 * @brief Concatenate strings
 */
char* strcat(char* dest, const char* src) {
    if (!dest || !src) return dest;
    char* d = dest;
    while (*d) {
        d++;
    }
    while (*src) {
        *d++ = *src++;
    }
    *d = '\0';
    return dest;
}

/**
 * @brief Concatenate strings with length limit
 */
char* strncat(char* dest, const char* src, size_t n) {
    if (!dest || !src) return dest;
    char* d = dest;
    while (*d) {
        d++;
    }
    size_t i = 0;
    while (i < n && *src) {
        *d++ = *src++;
        i++;
    }
    *d = '\0';
    return dest;
}

/**
 * @brief Compare strings
 */
int strcmp(const char* s1, const char* s2) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : 1;
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

/**
 * @brief Compare strings with length limit
 */
int strncmp(const char* s1, const char* s2, size_t n) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : 1;
    if (n == 0) return 0;
    size_t i = 0;
    while (i < n && *s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
        i++;
    }
    if (i == n) return 0;
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

/**
 * @brief Find first occurrence of character in string
 */
char* strchr(const char* s, int c) {
    if (!s) return NULL;
    char ch = (char)c;
    while (*s) {
        if (*s == ch) {
            return (char*)s;
        }
        s++;
    }
    if (ch == '\0') {
        return (char*)s;
    }
    return NULL;
}

/**
 * @brief Find last occurrence of character in string
 */
char* strrchr(const char* s, int c) {
    if (!s) return NULL;
    char ch = (char)c;
    const char* last = NULL;
    while (*s) {
        if (*s == ch) {
            last = s;
        }
        s++;
    }
    if (ch == '\0') {
        return (char*)s;
    }
    return (char*)last;
}

/**
 * @brief Find substring in string
 */
char* strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    if (*needle == '\0') return (char*)haystack;
    
    size_t needle_len = strlen(needle);
    while (*haystack) {
        if (strncmp(haystack, needle, needle_len) == 0) {
            return (char*)haystack;
        }
        haystack++;
    }
    return NULL;
}

/**
 * @brief Duplicate string (allocates memory)
 */
char* strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* dup = (char*)kmalloc(len);
    if (!dup) return NULL;
    memcpy(dup, s, len);
    return dup;
}
