#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <cerrno>
#include <vector>
#include <string>
#include <tuple>
#include "../syscall.h"

// ウィンドウ枠の大きさ
constexpr int H_MARGIN = 28, W_MARGIN = 8, Y_OFFSET = 24, X_OFFSET = 4;
// ウィンドウの初期位置
constexpr int WX = 10, WY = 10;

// 描画領域のまわりに確保するスペースの大きさ
constexpr int X_PADDING = 2, Y_PADDING = 2;
// 描画に用いる文字の大きさ
constexpr int CHAR_WIDTH = 8, CHAR_HEIGHT = 16;

// 描画に用いる色
constexpr uint32_t BACKGROUND_COLOR = 0xffffff;
constexpr uint32_t TEXT_COLOR = 0x000000;
constexpr uint32_t SPECIAL_TEXT_COLOR = 0xcccccc;
constexpr uint32_t CURSOR_COLOR = 0x000000;

// カーソルを点滅させるタイマーの情報
constexpr int CURSOR_TIMER_VALUE = 1, CURSOR_TIMER_INTERVAL_MS = 500;

// キーコード
constexpr int KEY_ESC = 0x29, KEY_DELETE = 0x4c;
constexpr int KEY_LEFT = 0x50, KEY_RIGHT = 0x4f;
constexpr int KEY_UP = 0x52, KEY_DOWN = 0x51;
constexpr int KEY_HOME = 0x4a, KEY_END = 0x4d;
constexpr int KEY_PGUP = 0x4b, KEY_PGDN = 0x4e;
constexpr int KEY_S = 0x16, KEY_D = 0x07, KEY_C = 0x06;
constexpr int KEY_MODIFIER_CTRL = 0x01, KEY_MODIFIER_ALT = 0x04;

// 保存確認ダイアログの情報 (座標はダイアログの左上からの相対)
/*
      Discard changes?

   Save     Discard   Cancel
 --------- --------- ---------
*/
constexpr int DIALOG_WIDTH = CHAR_WIDTH * 31;
constexpr int DIALOG_HEIGHT = CHAR_HEIGHT * 4 + CHAR_HEIGHT / 2;
static const char DIALOG_MESSAGE[] = "Discard changes?";
constexpr int DIALOG_MESSAGE_X = CHAR_WIDTH * 6;
constexpr int DIALOG_MESSAGE_Y = CHAR_HEIGHT / 2;
static const char DIALOG_SAVE[] = "Save";
constexpr int DIALOG_SAVE_X = CHAR_WIDTH * 3 + CHAR_WIDTH / 2;
constexpr int DIALOG_SAVE_Y = CHAR_HEIGHT * 2 + CHAR_HEIGHT / 2;
constexpr int DIALOG_SAVE_BX = CHAR_WIDTH * 1;
constexpr int DIALOG_SAVE_BY = CHAR_HEIGHT * 2;
constexpr int DIALOG_SAVE_BW = CHAR_WIDTH * 9;
constexpr int DIALOG_SAVE_BH = CHAR_HEIGHT * 2;
static const char DIALOG_DISCARD[] = "Discard";
constexpr int DIALOG_DISCARD_X = CHAR_WIDTH * 12;
constexpr int DIALOG_DISCARD_Y = CHAR_HEIGHT * 2 + CHAR_HEIGHT / 2;
constexpr int DIALOG_DISCARD_BX = CHAR_WIDTH * 11;
constexpr int DIALOG_DISCARD_BY = CHAR_HEIGHT * 2;
constexpr int DIALOG_DISCARD_BW = CHAR_WIDTH * 9;
constexpr int DIALOG_DISCARD_BH = CHAR_HEIGHT * 2;
static const char DIALOG_CANDEL[] = "Cancel";
constexpr int DIALOG_CANDEL_X = CHAR_WIDTH * 22 + CHAR_WIDTH / 2;
constexpr int DIALOG_CANDEL_Y = CHAR_HEIGHT * 2 + CHAR_HEIGHT / 2;
constexpr int DIALOG_CANDEL_BX = CHAR_WIDTH * 21;
constexpr int DIALOG_CANDEL_BY = CHAR_HEIGHT * 2;
constexpr int DIALOG_CANDEL_BW = CHAR_WIDTH * 9;
constexpr int DIALOG_CANDEL_BH = CHAR_HEIGHT * 2;

constexpr uint32_t DIALOG_TEXT_COLOR = 0x000000;
constexpr uint32_t DIALOG_BORDER_COLOR = 0x000000;
constexpr uint32_t DIALOG_BACKGROUND_COLOR = 0xcccccc;
constexpr uint32_t DIALOG_BUTTON_COLOR = 0xeeeeee;

// ダイアログを表示するために最低限必要なエリアの大きさ
constexpr int MIN_WIDTH = (DIALOG_WIDTH + CHAR_WIDTH - 1) / CHAR_WIDTH;
constexpr int MIN_HEIGHT = (DIALOG_HEIGHT + CHAR_HEIGHT - 1) / CHAR_HEIGHT;

// ファイルをUTF-8と仮定して読み込み、行ごとに文字の配列として返す。
// 文字にデコードできなかったバイトは、INVALID_BYTEフラグを立てて表す。
constexpr uint32_t INVALID_BYTE = UINT32_C(0x80000000);
std::vector<std::vector<uint32_t> > loadFile(const char* fileName) {
  std::vector<std::vector<uint32_t> > res;
  FILE* fp = fopen(fileName, "rb");
  if (fp == NULL) {
    printf("failed to open %s: %s\n", fileName, strerror(errno));
    res.push_back(std::vector<uint32_t>());
    return res;
  }
  int32_t c;
  std::vector<uint32_t> currentLine;
  while ((c = getc(fp)) != EOF) {
    if (c == '\n') {
      res.push_back(currentLine);
      currentLine.clear();
    } else {
      if (c < 0x80) {
        currentLine.push_back(c);
      } else if ((c & 0xe0) == 0xc0) {
        int32_t c2 = getc(fp);
        if (c2 != EOF && (c2 & 0xc0) == 0x80) {
          currentLine.push_back(((c & 0x1f) << 6) | (c2 & 0x3f));
        } else {
          currentLine.push_back(INVALID_BYTE | c);
          if (c2 != EOF) currentLine.push_back(INVALID_BYTE | c2);
        }
      } else if ((c & 0xf0) == 0xe0) {
        int32_t c2 = getc(fp);
        int32_t c3 = getc(fp);
        if (c2 != EOF && (c2 & 0xc0) == 0x80 &&
        c3 != EOF && (c3 & 0xc0) == 0x80) {
          currentLine.push_back(
            ((c & 0x0f) << 12) | ((c2 & 0x3f) << 6) | (c3 & 0x3f));
        } else {
          currentLine.push_back(INVALID_BYTE | c);
          if (c2 != EOF) currentLine.push_back(INVALID_BYTE | c2);
          if (c3 != EOF) currentLine.push_back(INVALID_BYTE | c3);
        }
      } else if ((c & 0xf8) == 0xf0) {
        int32_t c2 = getc(fp);
        int32_t c3 = getc(fp);
        int32_t c4 = getc(fp);
        if (c2 != EOF && (c2 & 0xc0) == 0x80 &&
        c3 != EOF && (c3 & 0xc0) == 0x80 &&
        c4 != EOF && (c4 & 0xc0) == 0x80) {
          currentLine.push_back(
            ((c & 0x07) << 18) | ((c2 & 0x3f) << 12) |
            ((c3 & 0x3f) << 6) | (c4 & 0x3f));
        } else {
          currentLine.push_back(INVALID_BYTE | c);
          if (c2 != EOF) currentLine.push_back(INVALID_BYTE | c2);
          if (c3 != EOF) currentLine.push_back(INVALID_BYTE | c3);
          if (c4 != EOF) currentLine.push_back(INVALID_BYTE | c4);
        }
      } else {
        currentLine.push_back(INVALID_BYTE | c);
      }
    }
  }
  res.push_back(currentLine);
  fclose(fp);
  return res;
}

bool saveFile(const char* fileName,
const std::vector<std::vector<uint32_t> >& data) {
  if (fileName == NULL) {
    printf("cannot save because no file is opened\n");
    return false;
  }
  FILE* fp = fopen(fileName, "wb");
  if (fp == NULL) {
    printf("failed to open %s: %s\n", fileName, strerror(errno));
    return false;
  }
  for (size_t i = 0; i < data.size(); i++) {
    if (i > 0) {
      if (fputc('\n', fp) == EOF) {
        printf("fputc failed\n");
        fclose(fp);
        return false;
      }
    }
    for (auto c : data[i]) {
      uint8_t buf[4];
      size_t writeSize;
      if ((c & INVALID_BYTE) || c < 0x80) {
        buf[0] = c;
        writeSize = 1;
      } else if (c < 0x800) {
        buf[0] = 0xc0 | ((c >> 6) & 0x1f);
        buf[1] = 0x80 | (c & 0x3f);
        writeSize = 2;
      } else if (c < 0x10000) {
        buf[0] = 0xe0 | ((c >> 12) & 0x0f);
        buf[1] = 0x80 | ((c >> 6) & 0x3f);
        buf[2] = 0x80 | (c & 0x3f);
        writeSize = 3;
      } else if (c < 0x110000) {
        buf[0] = 0xf0 | ((c >> 18) & 0x07);
        buf[1] = 0x80 | ((c >> 12) & 0x3f);
        buf[2] = 0x80 | ((c >> 6) & 0x3f);
        buf[3] = 0x80 | (c & 0x3f);
        writeSize = 4;
      } else {
        printf("invalid character 0x%" PRIx32 "\n", c);
        fclose(fp);
        return false;
      }
      if (fwrite(buf, writeSize, 1, fp) != 1) {
        printf("fwrite failed\n");
        fclose(fp);
        return false;
      }
    }
  }
  if (fclose(fp) == EOF) {
    printf("failed to close %s: %s\n", fileName, strerror(errno));
    return false;
  }
  return true;
}

// 中空の四角形を描画する
void drawRect(uint64_t layer_id, int x, int y, int w, int h, uint32_t c) {
  SyscallWinDrawLine(layer_id, x, y, x + w - 1, y, c);
  SyscallWinDrawLine(layer_id, x + w - 1, y, x + w - 1, y + h - 1, c);
  SyscallWinDrawLine(layer_id, x + w - 1, y + h - 1, x, y + h - 1, c);
  SyscallWinDrawLine(layer_id, x, y + h - 1, x, y, c);
}

// 1文字を描画する。
// 横幅が枠何個分だったかを返す。
int drawOneChar(uint64_t layer_id,
int x, int y, uint32_t c, bool dry_run = false) {
  layer_id |= LAYER_NO_REDRAW;
  char buf[8] = "";
  int ret = 0;
  if ((c & INVALID_BYTE) || (c < 0x20 && c != '\t')) {
    // 不正なバイトおよびタブ以外の制御文字 : バイトの値を枠で囲んで表現する
    if (!dry_run) {
      drawRect(layer_id, x, y,
        CHAR_WIDTH * 2, CHAR_HEIGHT, SPECIAL_TEXT_COLOR);
      snprintf(buf, sizeof(buf), "%02x", (unsigned int)(c & 0xff));
      SyscallWinWriteString(layer_id, x, y, SPECIAL_TEXT_COLOR, buf);
    }
    return 2;
  } else if (c == '\t') {
    if (!dry_run) {
      SyscallWinWriteString(layer_id, x, y, SPECIAL_TEXT_COLOR, ">");
    }
    return 1;
  } else if (c < 0x80) {
    buf[0] = (char)c;
    ret = 1;
  } else if (c < 0x800) {
    buf[0] = (char)(((c >> 6) & 0x1f) | 0xc0);
    buf[1] = (char)((c & 0x3f) | 0x80);
    // TODO: 描画幅をより正確に判定する
    ret = 2;
  } else if (c < 0x10000) {
    buf[0] = (char)(((c >> 12) & 0x0f) | 0xe0);
    buf[1] = (char)(((c >> 6) & 0x3f) | 0x80);
    buf[2] = (char)((c & 0x3f) | 0x80);
    // TODO: 描画幅をより正確に判定する
    ret = 2;
  } else if (c < 0x110000) {
    buf[0] = (char)(((c >> 18) & 0x07) | 0xf0);
    buf[1] = (char)(((c >> 12) & 0x3f) | 0x80);
    buf[2] = (char)(((c >> 6) & 0x3f) | 0x80);
    buf[3] = (char)((c & 0x3f) | 0x80);
    // TODO: 描画幅をより正確に判定する
    ret = 2;
  } else {
    if (!dry_run) {
      SyscallWinWriteString(layer_id, x, y, SPECIAL_TEXT_COLOR, "?");
    }
    return 1;
  }
  if (!dry_run) {
    SyscallWinWriteString(layer_id, x, y, TEXT_COLOR, buf);
  }
  return ret;
}

// [指定した文字の最初の位置, 指定した文字の幅] を返す
std::tuple<int, int> getCharRange(std::vector<int>& charIdx, int pos) {
  if((unsigned int)pos >= charIdx.size() || charIdx[pos] < 0) {
    int lastValid = (int)charIdx.size() - 1;
    while (lastValid >= 0 && charIdx[lastValid] < 0) lastValid--;
    return std::tuple<int, int>(lastValid + 1, 0);
  }
  int start = pos;
  while (start > 0 && charIdx[start] == charIdx[start - 1]) start--;
  int end = start;
  while ((unsigned int)(end + 1) < charIdx.size() &&
  charIdx[end] == charIdx[end + 1]) end++;
  return std::tuple<int, int>(start, end - start + 1);
}

// 一行を描画する
// 各枠がどの文字に相当するかの情報をcharIdxに記録する
void drawLine(std::vector<int>& charIdx, uint64_t layer_id, int x, int y,
int width, int tabSize, const std::vector<uint32_t>& chars, bool isLastLine,
bool dry_run = false) {
  layer_id |= LAYER_NO_REDRAW;
  charIdx.resize(width);
  if (!dry_run) {
    SyscallWinFillRectangle(layer_id, x, y,
      CHAR_WIDTH * width, CHAR_HEIGHT, BACKGROUND_COLOR);
  }
  int idx = 0;
  for (size_t i = 0; idx < width && i < chars.size(); i++) {
    uint32_t c = chars[i];
    // まずは描画せずに、描画幅をチェックする
    int cWidth = drawOneChar(layer_id, x + CHAR_WIDTH * idx, y, c, true);
    if (!dry_run && idx + cWidth <= width) {
      // 画面からはみ出さないなら、描画する
      drawOneChar(layer_id, x + CHAR_WIDTH * idx, y, c);
    }
    for (int j = 0; j < cWidth && idx + j < width; j++) {
      charIdx[idx + j] = (int)i;
    }
    idx += cWidth;
    if (c == '\t') {
      while (idx % tabSize != 0 && idx < width) {
        charIdx[idx++] = (int)i;
      }
    }
  }
  if (!dry_run && isLastLine && idx < width) {
    char eofText[] = "[EOF]";
    if (width - idx < (int)(sizeof(eofText) / sizeof(*eofText))) {
      eofText[width - idx] = '\0';
    }
    SyscallWinWriteString(layer_id | LAYER_NO_REDRAW,
      x + CHAR_WIDTH * idx, y, SPECIAL_TEXT_COLOR, eofText);
  }
  // 行のうち文字が無い部分
  for (; idx < width; idx++) {
    charIdx[idx] = -1;
  }
}

// 保存確認ダイアログを描画する
void drawDialog(uint64_t layer_id, int x, int y) {
  layer_id |= LAYER_NO_REDRAW;
  // ダイアログの背景と枠線
  SyscallWinFillRectangle(layer_id, x, y, DIALOG_WIDTH, DIALOG_HEIGHT,
    DIALOG_BACKGROUND_COLOR);
  drawRect(layer_id, x, y, DIALOG_WIDTH, DIALOG_HEIGHT, DIALOG_BORDER_COLOR);
  // ダイアログのボタンの背景と枠線
  SyscallWinFillRectangle(layer_id, x + DIALOG_SAVE_BX, y + DIALOG_SAVE_BY,
    DIALOG_SAVE_BW, DIALOG_SAVE_BH, DIALOG_BUTTON_COLOR);
  drawRect(layer_id, x + DIALOG_SAVE_BX, y + DIALOG_SAVE_BY,
    DIALOG_SAVE_BW, DIALOG_SAVE_BH, DIALOG_BORDER_COLOR);
  SyscallWinFillRectangle(layer_id,
    x + DIALOG_DISCARD_BX, y + DIALOG_DISCARD_BY,
    DIALOG_DISCARD_BW, DIALOG_DISCARD_BH, DIALOG_BUTTON_COLOR);
  drawRect(layer_id, x + DIALOG_DISCARD_BX, y + DIALOG_DISCARD_BY,
    DIALOG_DISCARD_BW, DIALOG_DISCARD_BH, DIALOG_BORDER_COLOR);
  SyscallWinFillRectangle(layer_id, x + DIALOG_CANDEL_BX, y + DIALOG_CANDEL_BY,
    DIALOG_CANDEL_BW, DIALOG_CANDEL_BH, DIALOG_BUTTON_COLOR);
  drawRect(layer_id, x + DIALOG_CANDEL_BX, y + DIALOG_CANDEL_BY,
    DIALOG_CANDEL_BW, DIALOG_CANDEL_BH, DIALOG_BORDER_COLOR);
  // ダイアログのテキスト
  SyscallWinWriteString(layer_id, x + DIALOG_MESSAGE_X, y + DIALOG_MESSAGE_Y,
    DIALOG_TEXT_COLOR, DIALOG_MESSAGE);
  // ダイアログのボタンのテキスト
  SyscallWinWriteString(layer_id, x + DIALOG_SAVE_X, y + DIALOG_SAVE_Y,
    DIALOG_TEXT_COLOR, DIALOG_SAVE);
  SyscallWinWriteString(layer_id, x + DIALOG_DISCARD_X, y + DIALOG_DISCARD_Y,
    DIALOG_TEXT_COLOR, DIALOG_DISCARD);
  SyscallWinWriteString(layer_id, x + DIALOG_CANDEL_X, y + DIALOG_CANDEL_Y,
    DIALOG_TEXT_COLOR, DIALOG_CANDEL);
  // ボタンのテキストの各1文字目に下線をつける (Alt+その文字で操作できる印)
  SyscallWinDrawLine(layer_id,
    x + DIALOG_SAVE_X, y + DIALOG_SAVE_Y + CHAR_HEIGHT + 1,
    x + DIALOG_SAVE_X + CHAR_WIDTH - 1,
    y + DIALOG_SAVE_Y + CHAR_HEIGHT + 1, DIALOG_TEXT_COLOR);
  SyscallWinDrawLine(layer_id,
    x + DIALOG_DISCARD_X, y + DIALOG_DISCARD_Y + CHAR_HEIGHT + 1,
    x + DIALOG_DISCARD_X + CHAR_WIDTH - 1,
    y + DIALOG_DISCARD_Y + CHAR_HEIGHT + 1, DIALOG_TEXT_COLOR);
  SyscallWinDrawLine(layer_id,
    x + DIALOG_CANDEL_X, y + DIALOG_CANDEL_Y + CHAR_HEIGHT + 1,
    x + DIALOG_CANDEL_X + CHAR_WIDTH - 1,
    y + DIALOG_CANDEL_Y + CHAR_HEIGHT + 1, DIALOG_TEXT_COLOR);
}

// 保存ダイアログのボタンの判定をする
enum DialogHitCheckResult {
  NONE, // どのボタンにも当たっていない
  SAVE, DISCARD, CANCEL
};
DialogHitCheckResult dialogHitCheck(int dx, int dy, int mx, int my) {
  auto hitCheck = [&](int bx, int by, int bw, int bh) {
    return dx + bx <= mx && mx < dx + bx + bw &&
      dy + by <= my && my < dy + by + bh;
  };
  if (hitCheck(DIALOG_SAVE_BX, DIALOG_SAVE_BY,
  DIALOG_SAVE_BW, DIALOG_SAVE_BH)) return DialogHitCheckResult::SAVE;
  if (hitCheck(DIALOG_DISCARD_BX, DIALOG_DISCARD_BY,
  DIALOG_DISCARD_BW, DIALOG_DISCARD_BH)) return DialogHitCheckResult::DISCARD;
  if (hitCheck(DIALOG_CANDEL_BX, DIALOG_CANDEL_BY,
  DIALOG_CANDEL_BW, DIALOG_CANDEL_BH)) return DialogHitCheckResult::CANCEL;
  return DialogHitCheckResult::NONE;
}

extern "C" void main(int argc, char** argv) {
  int width = 80, height = 20, tabSize = 8;
  char* fileName = NULL;
  bool isError = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-w") == 0) {
      if (++i < argc) width = atoi(argv[i]); else isError = true;
    } else if (strcmp(argv[i], "-h") == 0) {
      if (++i < argc) height = atoi(argv[i]); else isError = true;
    } else if (strcmp(argv[i], "-t") == 0) {
      if (++i < argc) tabSize = atoi(argv[i]); else isError = true;
    } else {
      if (fileName == NULL) fileName = argv[i]; else isError = true;
    }
  }
  if (isError || width <= 0 || height <= 0 || tabSize <= 0) {
    printf("Usage: %s [options] [file]\n",
      argc > 0 ? argv[0] : "tedit");
    puts("options:");
    puts("  -w <width>  : set the width of the edit area");
    puts("  -h <height> : set the height of the edit area");
    puts("  -t <size>   : set the width of one tab");
    puts("file: the name of file to edit");
    exit(1);
  }
  if (width < MIN_WIDTH) width = MIN_WIDTH;
  if (height < MIN_HEIGHT) height = MIN_HEIGHT;
  int winWidth = W_MARGIN + X_PADDING * 2 + CHAR_WIDTH * width;
  int winHeight = H_MARGIN + Y_PADDING * 2 + CHAR_HEIGHT * height;
  int dialogX = X_OFFSET + X_PADDING +
    (CHAR_WIDTH * width - DIALOG_WIDTH) / 2;
  int dialogY = Y_OFFSET + Y_PADDING +
    (CHAR_HEIGHT * height - DIALOG_HEIGHT) / 2;
  auto [hwnd, errOpen] = SyscallOpenWindow(
    winWidth, winHeight, WX, WY, "tedit");
  if (errOpen) {
     printf("SyscallOpenWindow failed: %s\n", strerror(errOpen));
     exit(errOpen);
  }
  SyscallWinFillRectangle(hwnd | LAYER_NO_REDRAW, X_OFFSET, Y_OFFSET,
    winWidth - W_MARGIN, winHeight - H_MARGIN, 0xffffff);
  std::vector<std::vector<uint32_t> > data;
  if (fileName != NULL) {
    data = loadFile(fileName);
  } else {
    data.push_back(std::vector<uint32_t>());
  }
  // 画面に描画した文字の情報
  std::vector<std::vector<int> > charIdx(height, std::vector<int>(width));
  // 画面上に表示する最初の行の位置
  int scrollY = 0;
  // 文章中のカーソルの位置
  int cursorX = 0, cursorY = 0;
  // カーソルが点灯中か
  bool cursorOn = false;
  // 保存していない変更があるか
  bool edited = false;
  // 保存確認ダイアログを表示中か
  bool dialogShown = false;

  // 行を文章中の位置で指定して情報を取得する
  auto getCharRange2 = [&](int y, int pos) {
    int screenIdx = y - scrollY;
    if(0 <= screenIdx && (unsigned int)screenIdx < charIdx.size()) {
      // 画面内
      return getCharRange(charIdx[screenIdx], pos);
    } else  if (0 <= y && (unsigned int)y < data.size()) {
      // 画面外だが、文章中
      std::vector<int> ci;
      drawLine(ci, 0, 0, 0, width, tabSize, data[y], false, true);
      return getCharRange(ci, pos);
    } else {
      // 文章外
      return std::tuple<int, int>(0, 0);
    }
  };

  auto drawCursor = [&, hwnd = hwnd](bool show, bool redraw){
    int cx = X_OFFSET + X_PADDING + CHAR_WIDTH * cursorX;
    int cy = Y_OFFSET + Y_PADDING + CHAR_HEIGHT * (cursorY - scrollY);
    if (show) {
      // キャレットを出す
      SyscallWinDrawLine(hwnd | LAYER_NO_REDRAW,
        cx, cy, cx, cy + CHAR_HEIGHT - 1, CURSOR_COLOR);
    } else {
      // キャレットを消す
      auto [start, cWidth] = getCharRange2(cursorY, cursorX);
      SyscallWinFillRectangle(hwnd | LAYER_NO_REDRAW,
        cx, cy, cWidth == 0 ? 1 : CHAR_WIDTH * cWidth,
        CHAR_HEIGHT, BACKGROUND_COLOR);
      if (cWidth > 0) {
        // 画面からはみ出さない場合のみ、文字を描画しなおす
        auto c = data[cursorY][charIdx[cursorY - scrollY][cursorX]];
        int drawWidth = drawOneChar(hwnd, cx, cy, c, true);
        if (cursorX + drawWidth <= width) {
          drawOneChar(hwnd, cx, cy, c);
        }
      }
    }
    if (redraw) SyscallWinRedraw(hwnd);
  };

  // drawFrom, drawTo : 画面における描画範囲
  auto drawLines = [&, hwnd = hwnd](int drawFrom, int drawTo) {
    if (drawFrom >= 0) {
      for (size_t i = (size_t)drawFrom;
      (drawTo < 0 || i <= (unsigned int)drawTo) &&
      i < (unsigned int)height; i++) {
        size_t idx = scrollY + i;
        if (idx < data.size()) {
          drawLine(charIdx[i], hwnd, X_OFFSET + X_PADDING,
            Y_OFFSET + Y_PADDING + CHAR_HEIGHT * i, width, tabSize, data[idx],
            idx + 1 == data.size());
        } else {
          SyscallWinFillRectangle(hwnd | LAYER_NO_REDRAW, X_OFFSET + X_PADDING,
            Y_OFFSET + Y_PADDING + CHAR_HEIGHT * i,
            CHAR_WIDTH * width, CHAR_HEIGHT, BACKGROUND_COLOR);
        }
      }
    }
    if (cursorOn) drawCursor(true, false);
  };
  drawLines(0, -1);
  SyscallWinRedraw(hwnd);

  SyscallCreateTimer(TIMER_ONESHOT_REL,
    CURSOR_TIMER_VALUE, CURSOR_TIMER_INTERVAL_MS);
  int ret = 0;
  bool exitFlag = false;

  // ダイアログの各ボタンを押した時の動作
  auto dialogSavePressed = [&, hwnd = hwnd]() {
    if (saveFile(fileName, data)) {
      exitFlag = true;
    } else {
      // 保存に失敗したので、終了せずダイアログを閉じる
      drawLines(0, -1);
      SyscallWinRedraw(hwnd);
      dialogShown = false;
    }
  };
  auto dialogDiscardPressed = [&]() {
   exitFlag = true;
  };
  auto dialogCancelPressed = [&, hwnd = hwnd]() {
    drawLines(0, -1);
    SyscallWinRedraw(hwnd);
    dialogShown = false;
  };

  while (!exitFlag) {
    AppEvent event;
    auto [n, err] = SyscallReadEvent(&event, 1);
    if (err) {
      printf("SysCallReadEvent failed: %s\n", strerror(err));
      ret = err;
      break;
    }
    if (n == 0) continue;
    auto& arg = event.arg;
    switch (event.type) {
      case AppEvent::kQuit:
        if (dialogShown) {
          dialogShown = false;
          drawLines(0, -1);
          SyscallWinRedraw(hwnd);
        } else if (edited) {
          drawDialog(hwnd, dialogX, dialogY);
          SyscallWinRedraw(hwnd);
          dialogShown = true;
        } else {
          exitFlag = true;
        }
        break;
      case AppEvent::kMouseMove:
        // 何もしない
        break;
      case AppEvent::kMouseButton:
        // 左クリックが押された
        if (arg.mouse_button.press == 1 && arg.mouse_button.button == 0) {
          // マウスの座標
          int mx = arg.mouse_button.x, my = arg.mouse_button.y;
          if (dialogShown) {
            auto res = dialogHitCheck(dialogX, dialogY, mx, my);
            switch (res) {
              case DialogHitCheckResult::NONE:
                // 何もしない
                break;
              case DialogHitCheckResult::SAVE:
                dialogSavePressed();
                break;
              case DialogHitCheckResult::DISCARD:
                dialogDiscardPressed();
                break;
              case DialogHitCheckResult::CANCEL:
                dialogCancelPressed();
                break;
            }
            break;
          }
          // 描画エリアの左上の座標
          int ox = X_OFFSET + X_PADDING, oy = Y_OFFSET + Y_PADDING;
          if (ox <= mx && oy <= my) {
            // クリックされた文字の位置
            int sx = (mx - ox) / CHAR_WIDTH, sy = (my - oy) / CHAR_HEIGHT;
            if (sx < width && sy < height) {
              int newCursorY = sy + scrollY;
              if ((unsigned int)newCursorY >= data.size()) {
                newCursorY = (int)data.size() - 1;
              }
              auto [start, cWidth] = getCharRange2(newCursorY, sx);
              if (cursorOn) drawCursor(false, false);
              cursorX = start;
              cursorY = newCursorY;
              if (cursorOn) drawCursor(true, true);
            }
          }
        }
        break;
      case AppEvent::kTimerTimeout:
        if (!dialogShown && arg.timer.value == CURSOR_TIMER_VALUE) {
          cursorOn = !cursorOn;
          drawCursor(cursorOn, true);
        }
        SyscallCreateTimer(TIMER_ONESHOT_REL,
          CURSOR_TIMER_VALUE, CURSOR_TIMER_INTERVAL_MS);
        break;
      case AppEvent::kKeyPush:
        if (arg.keypush.press) {
          if (dialogShown) {
            if (arg.keypush.keycode == KEY_ESC) {
              dialogCancelPressed();
            } else if (arg.keypush.modifier == KEY_MODIFIER_ALT) {
              switch (arg.keypush.keycode) {
                case KEY_S:
                  dialogSavePressed();
                  break;
                case KEY_D:
                  dialogDiscardPressed();
                  break;
                case KEY_C:
                  dialogCancelPressed();
                  break;
              }
            }
            break;
          }
          int newScrollY = scrollY, newCursorX = cursorX, newCursorY = cursorY;
          int redrawFrom = -1, redrawTo = -1;
          if (arg.keypush.modifier == KEY_MODIFIER_CTRL &&
          arg.keypush.keycode == KEY_S) {
            // Ctrl+S (上書き保存)
            if (saveFile(fileName, data)) edited = false;
          } else if (arg.keypush.ascii > 0 && arg.keypush.keycode != KEY_ESC) {
            if (arg.keypush.ascii == '\b') {
              // Backspace
              if (cursorX == 0) {
                if (cursorY > 0) {
                  // 現在の行を前の行と接続し、前の行以降を再描画
                  auto [s, cw] = getCharRange2(cursorY - 1, width);
                  data[cursorY - 1].insert(data[cursorY - 1].end(),
                    data[cursorY].begin(), data[cursorY].end());
                  data.erase(data.begin() + cursorY);
                  newCursorY--;
                  newCursorX = s;
                  redrawFrom = newCursorY - scrollY;
                  edited = true;
                }
              } else {
                // カーソル前の1文字を削除し、現在の行のみを再描画
                auto [s, cw] = getCharRange2(cursorY, cursorX - 1);
                int idx = charIdx[cursorY - scrollY][cursorX - 1];
                data[cursorY].erase(data[cursorY].begin() + idx);
                newCursorX = s;
                redrawFrom = redrawTo = cursorY - scrollY;
                edited = true;
              }
            } else if (arg.keypush.ascii == '\n') {
              // Enter
              int idx = cursorX == 0 ? 0 :
                charIdx[cursorY - scrollY][cursorX - 1] + 1;
              std::vector<uint32_t> newLine(
                data[cursorY].begin() + idx, data[cursorY].end());
              data[cursorY].erase(
                data[cursorY].begin() + idx, data[cursorY].end());
              data.insert(data.begin() + (cursorY + 1), newLine);
              newCursorY++;
              newCursorX = 0;
              redrawFrom = cursorY - scrollY;
              edited = true;
            } else {
              // 文字の挿入
              int idx = cursorX == 0 ? 0 :
                charIdx[cursorY - scrollY][cursorX - 1] + 1;
              int c = arg.keypush.ascii;
              data[cursorY].insert(data[cursorY].begin() + idx, c);
              // 挿入後の行について、新しいカーソルの位置を求める
              // タブ(など)の処理があり得るので、単に「一文字分」ではダメ
              std::vector<int> newCharIdx;
              drawLine(newCharIdx, 0, 0, 0, width, tabSize,
                data[cursorY], false, true);
              auto [s, cw] = getCharRange(newCharIdx, cursorX);
              if (cursorX + cw <= width) newCursorX += cw;
              redrawFrom = redrawTo = cursorY - scrollY;
              edited = true;
            }
          } else {
            switch (arg.keypush.keycode) {
              case KEY_DELETE:
                {
                  auto& lineCharIdx = charIdx[cursorY - scrollY];
                  auto [s, cw] = getCharRange2(cursorY, cursorX);
                  // 行末である
                  if (cw == 0 && (s == 0 ||
                  lineCharIdx[s - 1] + 1 == (int)data[cursorY].size())) {
                    // 次の行を現在の行にくっつけ、現在の行以降を再描画
                    if ((unsigned int)(cursorY + 1) < data.size()) {
                      data[cursorY].insert(data[cursorY].end(),
                        data[cursorY + 1].begin(), data[cursorY + 1].end());
                      data.erase(data.begin() + (cursorY + 1));
                      redrawFrom = cursorY - scrollY;
                      edited = true;
                    }
                  } else {
                    // 現在の行から1文字消し、現在の行のみを再描画
                    int idx = s == 0 ? lineCharIdx[s] : lineCharIdx[s - 1] + 1;
                    data[cursorY].erase(data[cursorY].begin() + idx);
                    redrawFrom = redrawTo = cursorY - scrollY;
                    edited = true;
                  }
                }
                break;
              case KEY_LEFT:
                newCursorX--;
                if (newCursorX < 0) {
                  if (cursorY == 0) {
                    newCursorX = 0;
                  } else {
                    newCursorY--;
                    newCursorX = width;
                  }
                }
                {
                  auto [s, cw] = getCharRange2(newCursorY, newCursorX);
                  newCursorX = s;
                }
                break;
              case KEY_RIGHT:
                {
                  auto [s, cw] = getCharRange2(cursorY, cursorX);
                  if (cw == 0) {
                    if (cursorY + 1 < (int)data.size()) {
                      newCursorY++;
                      newCursorX = 0;
                    }
                  } else {
                    newCursorX += cw;
                  }
                }
                break;
              case KEY_UP:
                if (cursorY > 0) {
                  newCursorY--;
                  auto [s, cw] = getCharRange2(newCursorY, cursorX);
                  newCursorX = s;
                }
                break;
              case KEY_DOWN:
                if (cursorY + 1 < (int)data.size()) {
                  newCursorY++;
                  auto [s, cw] = getCharRange2(newCursorY, cursorX);
                  newCursorX = s;
                }
                break;
              case KEY_HOME:
                if (arg.keypush.modifier & KEY_MODIFIER_CTRL) {
                  newCursorY = 0;
                }
                newCursorX = 0;
                break;
              case KEY_END:
                {
                  if (arg.keypush.modifier & KEY_MODIFIER_CTRL) {
                    newCursorY = (int)data.size() - 1;
                  }
                  auto [s, cw] = getCharRange2(newCursorY, width);
                  newCursorX = s;
                }
                break;
              case KEY_PGUP:
                newScrollY -= height / 2;
                newCursorY -= height / 2;
                if (newScrollY < 0) newScrollY = 0;
                if (newCursorY < 0) newCursorY = 0;
                {
                  auto [s, cw] = getCharRange2(newCursorY, newCursorX);
                  newCursorX = s;
                }
                break;
              case KEY_PGDN:
                newScrollY += height / 2;
                newCursorY += height / 2;
                if (newScrollY >= (int)data.size()) {
                  newScrollY = (int)data.size() - 1;
                }
                if (newCursorY >= (int)data.size()) {
                  newCursorY = (int)data.size() - 1;
                }
                {
                  auto [s, cw] = getCharRange2(newCursorY, newCursorX);
                  newCursorX = s;
                }
                break;
            }
          }
          // カーソルの位置に応じ、スクロール位置を補正する
          if (newCursorY < newScrollY) newScrollY = newCursorY;
          if (newCursorY >= newScrollY + height) {
            newScrollY = newCursorY - height + 1;
          }
          // 古いカーソルの表示を消す
          if (cursorOn) drawCursor(false, false);
          if (scrollY != newScrollY) {
            scrollY = newScrollY;
            // スクロール位置が変わっていたら、全体を再描画する
            redrawFrom = 0;
            redrawTo = -1;
          }
          cursorX = newCursorX;
          cursorY = newCursorY;
          drawLines(redrawFrom, redrawTo);
          SyscallWinRedraw(hwnd);
        }
        break;
    }
  }
  SyscallCloseWindow(hwnd);
  exit(ret);
}
