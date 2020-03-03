#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>

#include "syscall.h"

int close(int fd) {
  errno = EBADF;
  return -1;
}

int fstat(int fd, struct stat* buf) {
  errno = EBADF;
  return -1;
}

pid_t getpid(void) {
  return 0;
}

int isatty(int fd) {
  errno = EBADF;
  return -1;
}

int kill(pid_t pid, int sig) {
  errno = EPERM;
  return -1;
}

off_t lseek(int fd, off_t offset, int whence) {
  errno = EBADF;
  return -1;
}

int open(const char* path, int flags) {
  struct SyscallResult res = SyscallOpenFile(path, flags);
  if (res.error == 0) {
    return res.value;
  }
  errno = res.error;
  return -1;
}

// #@@range_begin(posix_memalign)
int posix_memalign(void** memptr, size_t alignment, size_t size) {
  void* p = malloc(size + alignment - 1);
  if (!p) {
    return ENOMEM;
  }
  uintptr_t addr = (uintptr_t)p;
  *memptr = (void*)((addr + alignment - 1) & ~(uintptr_t)(alignment - 1));
  return 0;
}
// #@@range_end(posix_memalign)

ssize_t read(int fd, void* buf, size_t count) {
  struct SyscallResult res = SyscallReadFile(fd, buf, count);
  if (res.error == 0) {
    return res.value;
  }
  errno = res.error;
  return -1;
}

caddr_t sbrk(int incr) {
  static uint8_t heap[4096];
  static int i = 0;
  int prev = i;
  i += incr;
  return (caddr_t)&heap[prev];
}

ssize_t write(int fd, const void* buf, size_t count) {
  struct SyscallResult res = SyscallPutString(fd, buf, count);
  if (res.error == 0) {
    return res.value;
  }
  errno = res.error;
  return -1;
}

void _exit(int status) {
  SyscallExit(status);
}
