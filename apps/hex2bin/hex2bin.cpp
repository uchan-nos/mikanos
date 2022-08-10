#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

bool reverse = false;
int size = 1;
bool little_endian = false;

// オプションを解析し、最初に見つけた非オプションの位置を返す
int ParseArgs(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
      printf("Usage: %s [options] [in-file]\n", argv[0]);
      printf("16 進数値をバイナリに変換し、標準出力へ表示する\n"
             "  -h: このヘルプを表示する\n"
             "  -s <size>: 処理の単位となるバイト数\n"
             "      size = 1 (default), 2, 4, 8\n"
             "  -l: バイナリをリトルエンディアンとして扱う\n"
             "  -r: 入出力を反転し、バイナリを 16 進ダンプする\n"
             );
      return -1;
    }

    if (strcmp(argv[i], "-s") == 0) {
      i++;
      size = atoi(argv[i]);
      if (size != 1 && size != 2 && size != 4 && size != 8) {
        fprintf(stderr, "size が不正: %d\n", size);
        return -1;
      }
    } else if (strcmp(argv[i], "-l") == 0) {
      little_endian = true;
    } else if (strcmp(argv[i], "-r") == 0) {
      reverse = true;
    } else {
      return i;
    }
  }
  return 0;
}

// stdin から最大 n バイトを v に読み込み、読んだバイト数を返す
int ReadBytes(uint8_t *v, int n, FILE* in) {
  for (int i = 0; i < n; i++) {
    int c = getc(in);
    if (c == EOF) {
      return i;
    }
    v[i] = c;
  }
  return n;
}

int main(int argc, char** argv) {
  FILE* in_file;

  int non_opt_index = ParseArgs(argc, argv);
  if (non_opt_index < 0) {
    return -non_opt_index;
  } else if (non_opt_index == 0) {
    in_file = stdin;
  } else {
    in_file = fopen(argv[non_opt_index], "rb");
    if (in_file == NULL) {
      perror("failed to open in-file");
      return 1;
    }
  }

  if (reverse) {
    int line_bytes = 0;
    for (;;) {
      uint8_t v[8] = {};
      if (ReadBytes(v, size, in_file) == 0) {
        break;
      }
      if (line_bytes > 0) {
        putchar(' ');
      }
      for (int i = 0; i < size; i++) {
        printf("%02x", v[little_endian ? size - 1 - i : i]);
      }
      line_bytes += size;
      if (line_bytes >= 8) {
        putchar('\n');
        line_bytes = 0;
      }
    }
  } else {
    uint64_t v;
    while (fscanf(in_file, "%lx", &v) == 1) {
      for (int i = 0; i < size; i++) {
        if (little_endian) {
          putchar(v & 0xff);
          v >>= 8;
        } else {
          putchar((v >> 8*(size - 1)) & 0xff);
          v <<= 8;
        }
      }
    }
  }

  return 0;
}
