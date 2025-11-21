/**
 * @file test_vfs.c
 * @brief VFS Unit Tests
 */

#include "../test_framework.h"

// Mock VFS functions
extern int vfs_open(const char* path, int flags);
extern int vfs_close(int fd);
extern ssize_t vfs_read(int fd, void* buf, size_t count);
extern ssize_t vfs_write(int fd, const void* buf, size_t count);
extern int vfs_mount(const char* source, const char* target, const char* fstype);
extern int vfs_unmount(const char* target);

TEST(vfs_open_close) {
    int fd = vfs_open("/test/file.txt", 0);
    ASSERT(fd >= 0);
    
    int ret = vfs_close(fd);
    ASSERT_EQ(ret, 0);
}

TEST(vfs_read_write) {
    int fd = vfs_open("/test/file.txt", 1); // O_RDWR
    ASSERT(fd >= 0);
    
    const char* data = "Hello, VFS!";
    ssize_t written = vfs_write(fd, data, 12);
    ASSERT_EQ(written, 12);
    
    char buffer[64];
    ssize_t read_bytes = vfs_read(fd, buffer, 64);
    ASSERT(read_bytes > 0);
    
    vfs_close(fd);
}

TEST(vfs_mount_unmount) {
    int ret = vfs_mount("/dev/sda1", "/mnt/test", "sfs");
    ASSERT_EQ(ret, 0);
    
    ret = vfs_unmount("/mnt/test");
    ASSERT_EQ(ret, 0);
}

TEST(vfs_invalid_fd) {
    char buffer[64];
    ssize_t ret = vfs_read(9999, buffer, 64);
    ASSERT(ret < 0);
}

int main(void) {
    test_init();
    
    printf("=== VFS Tests ===\n");
    RUN_TEST(vfs_open_close);
    RUN_TEST(vfs_read_write);
    RUN_TEST(vfs_mount_unmount);
    RUN_TEST(vfs_invalid_fd);
    
    test_print_results();
    return test_get_exit_code();
}
