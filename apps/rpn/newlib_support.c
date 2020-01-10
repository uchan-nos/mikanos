#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>

int close(int fd) {
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

off_t lseek(int fd, off_t offset, int whence) {
  errno = EBADF;
  return -1;
}

ssize_t read(int fd, void* buf, size_t count) {
  errno = EBADF;
  return -1;
}

caddr_t sbrk(int incr) {
  errno = ENOMEM;
  return (caddr_t)-1;
}

struct SyscallResult {
  uint64_t value;
  int error;
};
struct SyscallResult SyscallPutString(uint64_t, uint64_t, uint64_t);

ssize_t write(int fd, const void* buf, size_t count) {
  struct SyscallResult res = SyscallPutString(fd, (uint64_t)buf, count);
  if (res.error == 0) {
    return res.value;
  }
  errno = res.error;
  return -1;
}
