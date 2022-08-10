#include <cstdio>
#include <fcntl.h>
#include "../syscall.h"

int main(int argc, char** argv) {
  SyscallResult res = SyscallOpenFile("/memmap", O_RDONLY);
  if (res.error) {
    return res.error;
  }
  const int fd = res.value;
  size_t file_size;
  res = SyscallMapFile(fd, &file_size, 0);
  if (res.error) {
    return res.error;
  }

  char* p = reinterpret_cast<char*>(res.value);

  for (size_t i = 0; i < file_size; ++i) {
    printf("%c", p[i]);
  }
  printf("\nread from mapped file (%lu bytes)\n", file_size);

  return 0;
}
