#include <stdlib.h>

extern int main(int, char**);

void _start(int argc, char** argv) {
  exit(main(argc, argv));
}
