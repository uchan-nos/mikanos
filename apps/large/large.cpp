#include <cstdlib>

char table[3 * 1024 * 1024];

extern "C" int main(int argc, char** argv) {
  return atoi(argv[1]);
}
