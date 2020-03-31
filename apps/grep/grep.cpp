#include <cstdio>
#include <cstdlib>
#include <regex>

extern "C" void main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <pattern> [<file>]\n", argv[0]);
    exit(1);
  }

  std::regex pattern{argv[1]};

  FILE* fp = stdin;
  if (argc >= 3 && (fp = fopen(argv[2], "r")) == nullptr) {
    fprintf(stderr, "failed to open: %s\n", argv[2]);
    exit(1);
  }

  char line[256];
  while (fgets(line, sizeof(line), fp)) {
    std::cmatch m;
    if (std::regex_search(line, m, pattern)) {
      printf("%s", line);
    }
  }
  exit(0);
}
