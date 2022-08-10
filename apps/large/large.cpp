#include <cstdlib>
#include "../syscall.h"

char table[3 * 1024 * 1024];

int main(int argc, char** argv) {
  return atoi(argv[1]);
}
