#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <tuple>
#include <unistd.h>
#include <vector>
#include "../syscall.h"

std::tuple<int, char*, size_t> MapFile(const char* filepath) {
  SyscallResult res = SyscallOpenFile(filepath, O_RDONLY);
  if (res.error) {
    fprintf(stderr, "%s: %s\n", strerror(res.error), filepath);
    exit(1);
  }

  const int fd = res.value;
  size_t filesize;
  res = SyscallMapFile(fd, &filesize, 0);
  if (res.error) {
    fprintf(stderr, "%s\n", strerror(res.error));
    exit(1);
  }

  return {fd, reinterpret_cast<char*>(res.value), filesize};
}

uint64_t OpenTextWindow(int w, int h, const char* title) {
  SyscallResult res = SyscallOpenWindow(8 + 8*w, 28 + 16*h, 10, 10, title);
  if (res.error) {
    fprintf(stderr, "%s\n", strerror(res.error));
    exit(1);
  }
  const uint64_t layer_id = res.value;

  auto fill_rect = [layer_id](int x, int y, int w, int h, uint32_t c) {
    SyscallWinFillRectangle(layer_id, x, y, w, h, c);
  };
  fill_rect(3,       23,        1 + 8*w, 1,        0x666666);
  fill_rect(3,       24,        1,       1 + 16*h, 0x666666);
  fill_rect(4,       25 + 16*h, 1 + 8*w, 1,        0xcccccc);
  fill_rect(5 + 8*w, 24,        1,       1 + 16*h, 0xcccccc);

  return layer_id;
}

using LinesType = std::vector<std::pair<const char*, size_t>>;

LinesType FindLines(const char* p, size_t len) {
  LinesType lines;
  const char* end = p + len;

  auto next_lf = [end](const char* s) {
    while (s < end && *s != '\n') {
      ++s;
    }
    return s;
  };

  const char* lf = next_lf(p);
  while (lf < end) {
    lines.push_back({p, lf - p});
    p = lf + 1;
    lf = next_lf(p);
  }
  if (p < end) {
    lines.push_back({p, end - p});
  }

  return lines;
}

int CountUTF8Size(uint8_t c) {
  if (c < 0x80) {
    return 1;
  } else if (0xc0 <= c && c < 0xe0) {
    return 2;
  } else if (0xe0 <= c && c < 0xf0) {
    return 3;
  } else if (0xf0 <= c && c < 0xf8) {
    return 4;
  }
  return 0;
}

void CopyUTF8String(char* dst, size_t dst_size,
                    const char* src, size_t src_size,
                    int w, int tab) {
  int x = 0;

  const auto src_end = src + src_size;
  const auto dst_end = dst + dst_size;
  while (*src) {
    if (*src == '\t') {
      int spaces = tab - (x % tab);
      if (dst + spaces >= dst_end) {
        break;
      }
      memset(dst, ' ', spaces);
      ++src;
      dst += spaces;
      x += spaces;
      continue;
    }

    if (static_cast<uint8_t>(*src) < 0x80) {
      x += 1;
    } else {
      x += 2;
    }
    if (x >= w) {
      break;
    }

    int c = CountUTF8Size(*src);
    if (src + c > src_end || dst + c >= dst_end) {
      break;
    }
    memcpy(dst, src, c);
    src += c;
    dst += c;
  }

  *dst = '\0';
}

void DrawLines(const LinesType& lines, int start_line,
               uint64_t layer_id, int w, int h, int tab) {
  char buf[1024];
  SyscallWinFillRectangle(layer_id, 4, 24, 8*w, 16*h, 0xffffff);

  for (int i = 0; i < h; ++i) {
    int line_index = start_line + i;
    if (line_index < 0 || lines.size() <= line_index) {
      continue;
    }
    const auto [ line, line_len ] = lines[line_index];
    CopyUTF8String(buf, sizeof(buf), line, line_len, w, tab);
    SyscallWinWriteString(layer_id, 4, 24 + 16*i, 0x000000, buf);
  }
}

std::tuple<bool, int> WaitEvent(int h) {
  AppEvent events[1];
  while (true) {
    auto [ n, err ] = SyscallReadEvent(events, 1);
    if (err) {
      fprintf(stderr, "ReadEvent failed: %s\n", strerror(err));
      return {false, 0};
    }
    if (events[0].type == AppEvent::kQuit) {
      return {true, 0};
    } else if (events[0].type == AppEvent::kKeyPush &&
               events[0].arg.keypush.press) {
      return {false, events[0].arg.keypush.keycode};
    }
  }
}

bool UpdateStartLine(int* start_line, int height, size_t num_lines) {
  while (true) {
    const auto [ quit, keycode ] = WaitEvent(height);
    if (quit) {
      return quit;
    }
    if (num_lines < height) {
      continue;
    }

    int diff;
    switch (keycode) {
    case 75: diff = -height/2; break; // PageUp
    case 78: diff =  height/2; break; // PageDown
    case 81: diff =  1;        break; // DownArrow
    case 82: diff = -1;        break; // UpArrow
    default:
      continue;
    }

    if ((diff < 0 && *start_line == 0) ||
        (diff > 0 && *start_line == num_lines - height)) {
      continue;
    }

    *start_line += diff;
    if (*start_line < 0) {
      *start_line = 0;
    } else if (*start_line > num_lines - height) {
      *start_line = num_lines - height;
    }
    return false;
  }
}

extern "C" void main(int argc, char** argv) {
  auto print_help = [argv](){
    fprintf(stderr,
            "Usage: %s [-w WIDTH] [-h HEIGHT] [-t TAB] <file>\n",
            argv[0]);
  };

  int opt;
  int width = 80, height = 20, tab = 8;
  while ((opt = getopt(argc, argv, "w:h:t:")) != -1) {
    switch (opt) {
    case 'w': width = atoi(optarg); break;
    case 'h': height = atoi(optarg); break;
    case 't': tab = atoi(optarg); break;
    default:
      print_help();
      exit(1);
    }
  }
  if (optind >= argc) {
    print_help();
    exit(1);
  }

  const char* filepath = argv[optind];
  const auto [ fd, content, filesize ] = MapFile(filepath);

  const char* last_slash = strrchr(filepath, '/');
  const char* filename = last_slash ? &last_slash[1] : filepath;
  const auto layer_id = OpenTextWindow(width, height, filename);

  const auto lines = FindLines(content, filesize);

  int start_line = 0;
  while (true) {
    DrawLines(lines, start_line, layer_id, width, height, tab);
    if (UpdateStartLine(&start_line, height, lines.size())) {
      break;
    }
  }

  SyscallCloseWindow(layer_id);
  exit(0);
}
