#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

extern "C" void main(int argc, char** argv) {
  FILE* fp = stdin;
  if (argc >= 2) {
    fp = fopen(argv[1], "r");
    if (fp == nullptr) {
      fprintf(stderr, "failed to open '%s'\n", argv[1]);
      exit(1);
    }
  }

  std::vector<std::string> lines;
  char line[1024];
  while (fgets(line, sizeof(line), fp)) {
    lines.push_back(line);
  }

  auto comp = [](const std::string& a, const std::string& b) {
    for (int i = 0; i < std::min(a.length(), b.length()); ++i) {
      if (a[i] < b[i]) {
        return true;
      } else if (a[i] > b[i]) {
        return false;
      }
    }
    return a.length() < b.length();
  };

  std::sort(lines.begin(), lines.end(), comp);
  for (auto& line : lines) {
    printf("%s", line.c_str());
  }
  exit(0);
}
