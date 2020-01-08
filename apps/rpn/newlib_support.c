#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>

#define SBRK_BUF_SIZE (2 * 1024)
unsigned char sbrk_buf[SBRK_BUF_SIZE];
off_t sbrk_index;

int64_t SyscallPutString(const char*);

caddr_t sbrk(int incr) {
  if (sbrk_index + incr >= SBRK_BUF_SIZE) {
    errno = ENOMEM;
    return (caddr_t)-1;
  }

  off_t prev_index = sbrk_index;
  sbrk_index += incr;
  return (caddr_t)&sbrk_buf[prev_index];
}

int close(int fd) {
  errno = EBADF;
  return -1;
}

off_t lseek(int fd, off_t offset, int whence) {
  errno = EBADF;
  return -1;
}

ssize_t read(int fd, void* buf, size_t count) {
  errno = EBADF;
  return -1;
}

ssize_t write(int fd, const void* buf, size_t count) {
  if (fd == 1) {
    SyscallPutString((const char*)buf);
    return count;
  }
  errno = EBADF;
  return -1;
}

int fstat(int fd, struct stat* buf) {
  errno = EBADF;
  return -1;
}

int isatty(int fd) {
  errno = EBADF;
  return -1;
}
