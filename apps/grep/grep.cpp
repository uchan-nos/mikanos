#include <cstdio>
#include <regex>
#include <unistd.h>

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <pattern> [<file>]\n", argv[0]);
    return 1;
  }

  std::regex pattern{argv[1]};

  FILE* fp = stdin;
  if (argc >= 3 && (fp = fopen(argv[2], "r")) == nullptr) {
    fprintf(stderr, "failed to open: %s\n", argv[2]);
    return 1;
  }

  char line[256];
  const bool is_term = isatty(STDOUT_FILENO);
  while (fgets(line, sizeof(line), fp)) {
    std::cmatch m;
    if (std::regex_search(line, m, pattern)) {
      if (is_term) {
        printf("%.*s\033[91m%.*s\033[0m%s",
               static_cast<int>(m.prefix().length()), m.prefix().first,
               static_cast<int>(m[0].length()), m[0].first,
               m.suffix().first);
      } else {
        printf("%s", line);
      }
    }
  }
}
