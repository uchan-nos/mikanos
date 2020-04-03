#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "../syscall.h"

AppEvent WaitKey() {
  AppEvent events[1];
  while (true) {
    auto [ n, err ] = SyscallReadEvent(events, 1);
    if (err) {
      fprintf(stderr, "ReadEvent failed: %s\n", strerror(err));
      exit(1);
    }

    if (events[0].type == AppEvent::kQuit) {
      exit(0);
    }
    if (events[0].type == AppEvent::kKeyPush &&
        events[0].arg.keypush.press) {
      return events[0];
    }
  }
}

extern "C" void main(int argc, char** argv) {
  int page_size = 10;
  int arg_file = 1;
  if (argc >= 2 && argv[1][0] == '-' && isdigit(argv[1][1])) {
    page_size = atoi(&argv[1][1]);
    ++arg_file;
  }

  FILE* fp = stdin;
  if (argc > arg_file) {
    fp = fopen(argv[arg_file], "r");
    if (fp == nullptr) {
      fprintf(stderr, "failed to open '%s'\n", argv[arg_file]);
      exit(1);
    }
  }

  std::vector<std::string> lines{};
  char line[256];
  while (fgets(line, sizeof(line), fp)) {
    lines.emplace_back(line);
  }

  for (int i = 0; i < lines.size(); ++i) {
    if (i > 0 && (i % page_size) == 0) {
      fputs("---more---\n", stderr);
      WaitKey();
    }
    fputs(lines[i].c_str(), stdout);
  }
  exit(0);
}
