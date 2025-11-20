/**
 * @file unistd.h
 * @brief POSIX-like system calls
 */

#ifndef UNISTD_H
#define UNISTD_H

#include <stddef.h>
#include <stdint.h>

typedef int pid_t;
typedef int uid_t;
typedef long off_t;

pid_t getpid(void);
uid_t getuid(void);
void sleep(unsigned int seconds);
void usleep(unsigned int useconds);
void yield(void);

#endif // UNISTD_H

