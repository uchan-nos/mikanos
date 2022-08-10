#include <cstdio>
#include <cstdlib>
#include "../syscall.h"

int main(int argc, char** argv) {
  const char* filename = "/memmap";
  int ch = '\n';
  if (argc >= 3) {
    filename = argv[1];
    ch = atoi(argv[2]);
  }
  FILE* fp = fopen(filename, "r");
  if (!fp) {
    printf("failed to open %s\n", filename);
    return 1;
  }

  SyscallResult res = SyscallDemandPages(1, 0);
  if (res.error) {
    return 1;
  }
  char* buf = reinterpret_cast<char*>(res.value);
  char* buf0 = buf;

  size_t total = 0;
  size_t n;
  while ((n = fread(buf, 1, 4096, fp)) == 4096) {
    total += n;
    if (res = SyscallDemandPages(1, 0); res.error) {
      return 1;
    }
    buf += 4096;
  }
  total += n;
  printf("size of %s = %lu bytes\n", filename, total);

  size_t num = 0;
  for (int i = 0; i < total; ++i) {
    if (buf0[i] == ch) {
      ++num;
    }
  }
  printf("the number of '%c' (0x%02x) = %lu\n", ch, ch, num);
  return 0;
}
