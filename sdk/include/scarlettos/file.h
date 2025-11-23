#ifndef _SCARLETTOS_FILE_H
#define _SCARLETTOS_FILE_H

#include <scarlettos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// File access modes
#define O_RDONLY    0x0001
#define O_WRONLY    0x0002
#define O_RDWR      0x0003
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

// File types
#define DT_UNKNOWN  0
#define DT_FIFO     1
#define DT_CHR      2
#define DT_DIR      4
#define DT_BLK      6
#define DT_REG      8
#define DT_LNK      10
#define DT_SOCK     12

typedef struct {
    uint64_t inode;
    uint64_t size;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
} sc_stat_t;

typedef struct {
    uint64_t d_ino;
    uint64_t d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[256];
} sc_dirent_t;

// File operations
int sc_open(const char *path, int flags, int mode);
int sc_close(int fd);
ssize_t sc_read(int fd, void *buf, size_t count);
ssize_t sc_write(int fd, const void *buf, size_t count);
off_t sc_lseek(int fd, off_t offset, int whence);
int sc_stat(const char *path, sc_stat_t *buf);
int sc_fstat(int fd, sc_stat_t *buf);
int sc_unlink(const char *path);
int sc_mkdir(const char *path, int mode);
int sc_rmdir(const char *path);

// Directory operations
int sc_getdents(int fd, sc_dirent_t *dirp, unsigned int count);

#ifdef __cplusplus
}
#endif

#endif // _SCARLETTOS_FILE_H
