// Copyright (c) 2022 MikeCAT

/* 使い方メモ（将来的にアプリのヘルプ表示に組み込みたい）
 *
 * Ctrl-S で上書き保存できる
 *
 * 保存確認ダイアログの Save, Discard, Cancel を Alt-S/D/C で選択できる
 */

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <tuple>
#include <vector>

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
static const char DIALOG_CANCEL[] = "Cancel";
constexpr int DIALOG_CANCEL_X = CHAR_WIDTH * 22 + CHAR_WIDTH / 2;
constexpr int DIALOG_CANCEL_Y = CHAR_HEIGHT * 2 + CHAR_HEIGHT / 2;
constexpr int DIALOG_CANCEL_BX = CHAR_WIDTH * 21;
constexpr int DIALOG_CANCEL_BY = CHAR_HEIGHT * 2;
constexpr int DIALOG_CANCEL_BW = CHAR_WIDTH * 9;
constexpr int DIALOG_CANCEL_BH = CHAR_HEIGHT * 2;

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
std::vector<std::vector<uint32_t> > LoadFile(const char* file_name) {
  std::vector<std::vector<uint32_t> > res;
  FILE* fp = fopen(file_name, "rb");
  if (fp == NULL) {
    if (errno != ENOENT) {
      printf("failed to open %s: %s\n", file_name, strerror(errno));
    }
    res.push_back(std::vector<uint32_t>());
    return res;
  }
  int32_t c;
  std::vector<uint32_t> current_line;
  while ((c = getc(fp)) != EOF) {
    if (c == '\n') {
      res.push_back(current_line);
      current_line.clear();
      continue;
    }
    if (c < 0x80) {
      current_line.push_back(c);
    } else if ((c & 0xe0) == 0xc0) {
      int32_t c2 = getc(fp);
      if (c2 != EOF && (c2 & 0xc0) == 0x80) {
        current_line.push_back(((c & 0x1f) << 6) | (c2 & 0x3f));
      } else {
        current_line.push_back(INVALID_BYTE | c);
        if (c2 != EOF) current_line.push_back(INVALID_BYTE | c2);
      }
    } else if ((c & 0xf0) == 0xe0) {
      int32_t c2 = getc(fp);
      int32_t c3 = getc(fp);
      if (c2 != EOF && (c2 & 0xc0) == 0x80 &&
          c3 != EOF && (c3 & 0xc0) == 0x80) {
        auto dec = ((c & 0x0f) << 12) | ((c2 & 0x3f) << 6) | (c3 & 0x3f);
        current_line.push_back(dec);
      } else {
        current_line.push_back(INVALID_BYTE | c);
        if (c2 != EOF) current_line.push_back(INVALID_BYTE | c2);
        if (c3 != EOF) current_line.push_back(INVALID_BYTE | c3);
      }
    } else if ((c & 0xf8) == 0xf0) {
      int32_t c2 = getc(fp);
      int32_t c3 = getc(fp);
      int32_t c4 = getc(fp);
      if (c2 != EOF && (c2 & 0xc0) == 0x80 &&
          c3 != EOF && (c3 & 0xc0) == 0x80 &&
          c4 != EOF && (c4 & 0xc0) == 0x80) {
        auto dec = ((c & 0x07) << 18) | ((c2 & 0x3f) << 12) |
                   ((c3 & 0x3f) << 6) | (c4 & 0x3f);
        current_line.push_back(dec);
      } else {
        current_line.push_back(INVALID_BYTE | c);
        if (c2 != EOF) current_line.push_back(INVALID_BYTE | c2);
        if (c3 != EOF) current_line.push_back(INVALID_BYTE | c3);
        if (c4 != EOF) current_line.push_back(INVALID_BYTE | c4);
      }
    } else {
      current_line.push_back(INVALID_BYTE | c);
    }
  }
  res.push_back(current_line);
  fclose(fp);
  return res;
}

// 1文字をUTF-8にエンコードし、結果のバイト数を返す。
// UTF-8にエンコードできない値の場合は-1を返す。
int EncodeCharToUTF8(char* buf, uint32_t c) {
  if (c < 0x80) {
    buf[0] = (char)c;
    return 1;
  } else if (c < 0x800) {
    buf[0] = (char)(0xc0 | ((c >> 6) & 0x1f));
    buf[1] = (char)(0x80 | (c & 0x3f));
    return 2;
  } else if (c < 0x10000) {
    buf[0] = (char)(0xe0 | ((c >> 12) & 0x0f));
    buf[1] = (char)(0x80 | ((c >> 6) & 0x3f));
    buf[2] = (char)(0x80 | (c & 0x3f));
    return 3;
  } else if (c < 0x110000) {
    buf[0] = (char)(0xf0 | ((c >> 18) & 0x07));
    buf[1] = (char)(0x80 | ((c >> 12) & 0x3f));
    buf[2] = (char)(0x80 | ((c >> 6) & 0x3f));
    buf[3] = (char)(0x80 | (c & 0x3f));
    return 4;
  }
  return -1;
}

bool SaveFile(const char* file_name,
              const std::vector<std::vector<uint32_t> >& data) {
  if (file_name == NULL) {
    printf("cannot save because no file is opened\n");
    return false;
  }
  FILE* fp = fopen(file_name, "wb");
  if (fp == NULL) {
    printf("failed to open %s: %s\n", file_name, strerror(errno));
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
      char buf[8];
      int write_size = EncodeCharToUTF8(buf, c);
      if (write_size < 0) {
        printf("invalid character 0x%" PRIx32 "\n", c);
        fclose(fp);
        return false;
      }
      if (fwrite(buf, write_size, 1, fp) != 1) {
        printf("fwrite failed\n");
        fclose(fp);
        return false;
      }
    }
  }
#ifdef NO_FCLOSE_CHECK
  fclose(fp);
#else
  if (fclose(fp) == EOF) {
    printf("failed to close %s: %s\n", file_name, strerror(errno));
    return false;
  }
#endif
  return true;
}

// 中空の四角形を描画する
void DrawRect(uint64_t layer_id, int x, int y, int w, int h, uint32_t c) {
  SyscallWinDrawLine(layer_id, x, y, x + w - 1, y, c);
  SyscallWinDrawLine(layer_id, x + w - 1, y, x + w - 1, y + h - 1, c);
  SyscallWinDrawLine(layer_id, x + w - 1, y + h - 1, x, y + h - 1, c);
  SyscallWinDrawLine(layer_id, x, y + h - 1, x, y, c);
}

// 1文字を描画する。
// 横幅が枠何個分だったかを返す。
int DrawOneChar(uint64_t layer_id,
                int x, int y, uint32_t c, bool dry_run = false) {
  layer_id |= LAYER_NO_REDRAW;
  char buf[8] = "";
  if ((c & INVALID_BYTE) || (c < 0x20 && c != '\t')) {
    // 不正なバイトおよびタブ以外の制御文字 : バイトの値を枠で囲んで表現する
    if (!dry_run) {
      DrawRect(layer_id, x, y,
               CHAR_WIDTH * 2, CHAR_HEIGHT, SPECIAL_TEXT_COLOR);
      snprintf(buf, sizeof(buf), "%02x", (unsigned int)(c & 0xff));
      SyscallWinWriteString(layer_id, x, y, SPECIAL_TEXT_COLOR, buf);
    }
    return 2;
  }
  if (c == '\t') {
    // タブ
    if (!dry_run) {
      SyscallWinWriteString(layer_id, x, y, SPECIAL_TEXT_COLOR, ">");
    }
    return 1;
  }
  if (EncodeCharToUTF8(buf, c) < 0) {
    // UTF-8で表せない文字
    if (!dry_run) {
      SyscallWinWriteString(layer_id, x, y, SPECIAL_TEXT_COLOR, "?");
    }
    return 1;
  }
  // UTF-8で表せる文字
  if (!dry_run) {
    SyscallWinWriteString(layer_id, x, y, TEXT_COLOR, buf);
  }
  // TODO: 描画幅をより正確に判定する
  return c < 0x80 ? 1 : 2;
}

// [指定した文字の最初の位置, 指定した文字の幅] を返す
std::tuple<int, int> GetCharRange(std::vector<int>& char_idx, int pos) {
  if((unsigned int)pos >= char_idx.size() || char_idx[pos] < 0) {
    int last_valid = (int)char_idx.size() - 1;
    while (last_valid >= 0 && char_idx[last_valid] < 0) last_valid--;
    return std::tuple<int, int>(last_valid + 1, 0);
  }
  int start = pos;
  while (start > 0 && char_idx[start] == char_idx[start - 1]) start--;
  int end = start;
  while ((unsigned int)(end + 1) < char_idx.size() &&
         char_idx[end] == char_idx[end + 1]) end++;
  return std::tuple<int, int>(start, end - start + 1);
}

// 一行を描画する
// 各枠がどの文字に相当するかの情報をchar_idxに記録する
void DrawLine(std::vector<int>& char_idx, uint64_t layer_id,
              int x, int y, int width, int tab_size,
              const std::vector<uint32_t>& chars, bool is_last_line,
              bool dry_run = false) {
  layer_id |= LAYER_NO_REDRAW;
  char_idx.resize(width);
  if (!dry_run) {
    SyscallWinFillRectangle(layer_id, x, y,
                            CHAR_WIDTH * width, CHAR_HEIGHT, BACKGROUND_COLOR);
  }
  int idx = 0;
  for (size_t i = 0; idx < width && i < chars.size(); i++) {
    uint32_t c = chars[i];
    // まずは描画せずに、描画幅をチェックする
    int char_width = DrawOneChar(layer_id, x + CHAR_WIDTH * idx, y, c, true);
    if (!dry_run && idx + char_width <= width) {
      // 画面からはみ出さないなら、描画する
      DrawOneChar(layer_id, x + CHAR_WIDTH * idx, y, c);
    }
    for (int j = 0; j < char_width && idx + j < width; j++) {
      char_idx[idx + j] = (int)i;
    }
    idx += char_width;
    if (c == '\t') {
      while (idx % tab_size != 0 && idx < width) {
        char_idx[idx++] = (int)i;
      }
    }
  }
  if (!dry_run && is_last_line && idx < width) {
    char eof_text[] = "[EOF]";
    if (width - idx < (int)(sizeof(eof_text) / sizeof(*eof_text))) {
      eof_text[width - idx] = '\0';
    }
    SyscallWinWriteString(layer_id | LAYER_NO_REDRAW,
                          x + CHAR_WIDTH * idx, y,
                          SPECIAL_TEXT_COLOR, eof_text);
  }
  // 行のうち文字が無い部分
  for (; idx < width; idx++) {
    char_idx[idx] = -1;
  }
}

// 保存確認ダイアログを描画する
void DrawDialog(uint64_t layer_id, int x, int y) {
  layer_id |= LAYER_NO_REDRAW;
  // ダイアログの背景と枠線
  SyscallWinFillRectangle(layer_id, x, y,
                          DIALOG_WIDTH, DIALOG_HEIGHT, DIALOG_BACKGROUND_COLOR);
  DrawRect(layer_id, x, y, DIALOG_WIDTH, DIALOG_HEIGHT, DIALOG_BORDER_COLOR);

  // ダイアログのボタンの背景と枠線
  SyscallWinFillRectangle(layer_id, x + DIALOG_SAVE_BX, y + DIALOG_SAVE_BY,
                          DIALOG_SAVE_BW, DIALOG_SAVE_BH, DIALOG_BUTTON_COLOR);
  DrawRect(layer_id, x + DIALOG_SAVE_BX, y + DIALOG_SAVE_BY,
           DIALOG_SAVE_BW, DIALOG_SAVE_BH, DIALOG_BORDER_COLOR);
  SyscallWinFillRectangle(layer_id,
                          x + DIALOG_DISCARD_BX, y + DIALOG_DISCARD_BY,
                          DIALOG_DISCARD_BW, DIALOG_DISCARD_BH,
                          DIALOG_BUTTON_COLOR);
  DrawRect(layer_id, x + DIALOG_DISCARD_BX, y + DIALOG_DISCARD_BY,
           DIALOG_DISCARD_BW, DIALOG_DISCARD_BH, DIALOG_BORDER_COLOR);
  SyscallWinFillRectangle(layer_id, x + DIALOG_CANCEL_BX, y + DIALOG_CANCEL_BY,
                          DIALOG_CANCEL_BW, DIALOG_CANCEL_BH,
                          DIALOG_BUTTON_COLOR);
  DrawRect(layer_id, x + DIALOG_CANCEL_BX, y + DIALOG_CANCEL_BY,
           DIALOG_CANCEL_BW, DIALOG_CANCEL_BH, DIALOG_BORDER_COLOR);

  // ダイアログのテキスト
  SyscallWinWriteString(layer_id, x + DIALOG_MESSAGE_X, y + DIALOG_MESSAGE_Y,
                        DIALOG_TEXT_COLOR, DIALOG_MESSAGE);

  // ダイアログのボタンのテキスト
  SyscallWinWriteString(layer_id, x + DIALOG_SAVE_X, y + DIALOG_SAVE_Y,
                        DIALOG_TEXT_COLOR, DIALOG_SAVE);
  SyscallWinWriteString(layer_id, x + DIALOG_DISCARD_X, y + DIALOG_DISCARD_Y,
                        DIALOG_TEXT_COLOR, DIALOG_DISCARD);
  SyscallWinWriteString(layer_id, x + DIALOG_CANCEL_X, y + DIALOG_CANCEL_Y,
                        DIALOG_TEXT_COLOR, DIALOG_CANCEL);

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
                     x + DIALOG_CANCEL_X, y + DIALOG_CANCEL_Y + CHAR_HEIGHT + 1,
                     x + DIALOG_CANCEL_X + CHAR_WIDTH - 1,
                     y + DIALOG_CANCEL_Y + CHAR_HEIGHT + 1, DIALOG_TEXT_COLOR);
}

// 保存ダイアログのボタンの判定をする
enum DialogHitCheckResult {
  NONE, // どのボタンにも当たっていない
  SAVE, DISCARD, CANCEL
};
DialogHitCheckResult DialogHitCheck(int dx, int dy, int mx, int my) {
  auto hit_check = [&](int bx, int by, int bw, int bh) {
    return dx + bx <= mx && mx < dx + bx + bw &&
           dy + by <= my && my < dy + by + bh;
  };
  if (hit_check(DIALOG_SAVE_BX, DIALOG_SAVE_BY,
                DIALOG_SAVE_BW, DIALOG_SAVE_BH)) {
    return DialogHitCheckResult::SAVE;
  }
  if (hit_check(DIALOG_DISCARD_BX, DIALOG_DISCARD_BY,
                DIALOG_DISCARD_BW, DIALOG_DISCARD_BH)) {
    return DialogHitCheckResult::DISCARD;
  }
  if (hit_check(DIALOG_CANCEL_BX, DIALOG_CANCEL_BY,
                DIALOG_CANCEL_BW, DIALOG_CANCEL_BH)) {
    return DialogHitCheckResult::CANCEL;
  }
  return DialogHitCheckResult::NONE;
}

int main(int argc, char** argv) {
  int width = 80, height = 20, tab_size = 8;
  char* file_name = NULL;
  bool is_error = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-w") == 0) {
      if (++i < argc) width = atoi(argv[i]); else is_error = true;
    } else if (strcmp(argv[i], "-h") == 0) {
      if (++i < argc) height = atoi(argv[i]); else is_error = true;
    } else if (strcmp(argv[i], "-t") == 0) {
      if (++i < argc) tab_size = atoi(argv[i]); else is_error = true;
    } else {
      if (file_name == NULL) file_name = argv[i]; else is_error = true;
    }
  }
  if (is_error || width <= 0 || height <= 0 || tab_size <= 0) {
    printf("Usage: %s [options] [file]\n", argc > 0 ? argv[0] : "tedit");
    puts("options:");
    puts("  -w <width>  : set the width of the edit area");
    puts("  -h <height> : set the height of the edit area");
    puts("  -t <size>   : set the width of one tab");
    puts("file: the name of file to edit");
    exit(1);
  }
  if (width < MIN_WIDTH) width = MIN_WIDTH;
  if (height < MIN_HEIGHT) height = MIN_HEIGHT;
  int win_width = W_MARGIN + X_PADDING * 2 + CHAR_WIDTH * width;
  int win_height = H_MARGIN + Y_PADDING * 2 + CHAR_HEIGHT * height;
  int dialog_x = X_OFFSET + X_PADDING +
                 (CHAR_WIDTH * width - DIALOG_WIDTH) / 2;
  int dialog_y = Y_OFFSET + Y_PADDING +
                 (CHAR_HEIGHT * height - DIALOG_HEIGHT) / 2;
  auto [hwnd, err_open] = SyscallOpenWindow(win_width, win_height, WX, WY, "tedit");
  if (err_open) {
     printf("SyscallOpenWindow failed: %s\n", strerror(err_open));
     exit(err_open);
  }
  SyscallWinFillRectangle(hwnd | LAYER_NO_REDRAW, X_OFFSET, Y_OFFSET,
                          win_width - W_MARGIN, win_height - H_MARGIN, 0xffffff);
  std::vector<std::vector<uint32_t> > data;
  if (file_name != NULL) {
    data = LoadFile(file_name);
  } else {
    data.push_back(std::vector<uint32_t>());
  }
  // 画面に描画した文字の情報
  std::vector<std::vector<int> > char_idx(height, std::vector<int>(width));
  // 画面上に表示する最初の行の位置
  int scroll_y = 0;
  // 文章中のカーソルの位置
  int cursor_x = 0, cursor_y = 0;
  // カーソルが点灯中か
  bool cursor_on = false;
  // 保存していない変更があるか
  bool edited = false;
  // 保存確認ダイアログを表示中か
  bool dialog_shown = false;

  // 行を文章中の位置で指定して情報を取得する
  auto get_char_range_2 = [&](int y, int pos) {
    int screen_idx = y - scroll_y;
    if (0 <= screen_idx && (unsigned int)screen_idx < char_idx.size()) {
      // 画面内
      return GetCharRange(char_idx[screen_idx], pos);
    } else if (0 <= y && (unsigned int)y < data.size()) {
      // 画面外だが、文章中
      std::vector<int> ci;
      DrawLine(ci, 0, 0, 0, width, tab_size, data[y], false, true);
      return GetCharRange(ci, pos);
    } else {
      // 文章外
      return std::tuple<int, int>(0, 0);
    }
  };

  auto draw_cursor = [&, hwnd = hwnd](bool show, bool redraw) {
    int cx = X_OFFSET + X_PADDING + CHAR_WIDTH * cursor_x;
    int cy = Y_OFFSET + Y_PADDING + CHAR_HEIGHT * (cursor_y - scroll_y);
    if (show) {
      // キャレットを出す
      SyscallWinDrawLine(hwnd | LAYER_NO_REDRAW,
                         cx, cy, cx, cy + CHAR_HEIGHT - 1, CURSOR_COLOR);
    } else {
      // キャレットを消す
      auto [start, char_width] = get_char_range_2(cursor_y, cursor_x);
      SyscallWinFillRectangle(hwnd | LAYER_NO_REDRAW,
                              cx, cy, char_width == 0 ? 1 : CHAR_WIDTH * char_width,
                              CHAR_HEIGHT, BACKGROUND_COLOR);
      if (char_width > 0) {
        // 画面からはみ出さない場合のみ、文字を描画しなおす
        auto c = data[cursor_y][char_idx[cursor_y - scroll_y][cursor_x]];
        int draw_width = DrawOneChar(hwnd, cx, cy, c, true);
        if (cursor_x + draw_width <= width) {
          DrawOneChar(hwnd, cx, cy, c);
        }
      }
    }
    if (redraw) SyscallWinRedraw(hwnd);
  };

  // draw_from, draw_to : 画面における描画範囲
  auto draw_lines = [&, hwnd = hwnd](int draw_from, int draw_to) {
    if (draw_from >= 0) {
      for (size_t i = (size_t)draw_from;
           (draw_to < 0 || i <= (unsigned int)draw_to) && i < (unsigned int)height;
           i++) {
        size_t idx = scroll_y + i;
        if (idx < data.size()) {
          DrawLine(char_idx[i], hwnd, X_OFFSET + X_PADDING,
                   Y_OFFSET + Y_PADDING + CHAR_HEIGHT * i, width, tab_size,
                   data[idx], idx + 1 == data.size());
        } else {
          SyscallWinFillRectangle(hwnd | LAYER_NO_REDRAW, X_OFFSET + X_PADDING,
                                  Y_OFFSET + Y_PADDING + CHAR_HEIGHT * i,
                                  CHAR_WIDTH * width, CHAR_HEIGHT, BACKGROUND_COLOR);
        }
      }
    }
    if (cursor_on) draw_cursor(true, false);
  };
  draw_lines(0, -1);
  SyscallWinRedraw(hwnd);

  SyscallCreateTimer(TIMER_ONESHOT_REL, CURSOR_TIMER_VALUE, CURSOR_TIMER_INTERVAL_MS);
  int ret = 0;
  bool exit_flag = false;

  // ダイアログの各ボタンを押した時の動作
  auto dialog_save_pressed = [&, hwnd = hwnd]() {
    if (SaveFile(file_name, data)) {
      exit_flag = true;
    } else {
      // 保存に失敗したので、終了せずダイアログを閉じる
      draw_lines(0, -1);
      SyscallWinRedraw(hwnd);
      dialog_shown = false;
    }
  };
  auto dialog_discard_pressed = [&]() {
    exit_flag = true;
  };
  auto dialog_cancel_pressed = [&, hwnd = hwnd]() {
    draw_lines(0, -1);
    SyscallWinRedraw(hwnd);
    dialog_shown = false;
  };

  while (!exit_flag) {
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
        if (dialog_shown) {
          dialog_shown = false;
          draw_lines(0, -1);
          SyscallWinRedraw(hwnd);
        } else if (edited) {
          DrawDialog(hwnd, dialog_x, dialog_y);
          SyscallWinRedraw(hwnd);
          dialog_shown = true;
        } else {
          exit_flag = true;
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
          if (dialog_shown) {
            auto res = DialogHitCheck(dialog_x, dialog_y, mx, my);
            switch (res) {
              case DialogHitCheckResult::NONE:
                // 何もしない
                break;
              case DialogHitCheckResult::SAVE:
                dialog_save_pressed();
                break;
              case DialogHitCheckResult::DISCARD:
                dialog_discard_pressed();
                break;
              case DialogHitCheckResult::CANCEL:
                dialog_cancel_pressed();
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
              int new_cursor_y = sy + scroll_y;
              if ((unsigned int)new_cursor_y >= data.size()) {
                new_cursor_y = (int)data.size() - 1;
              }
              auto [start, char_width] = get_char_range_2(new_cursor_y, sx);
              if (cursor_on) draw_cursor(false, false);
              cursor_x = start;
              cursor_y = new_cursor_y;
              if (cursor_on) draw_cursor(true, true);
            }
          }
        }
        break;
      case AppEvent::kTimerTimeout:
        if (!dialog_shown && arg.timer.value == CURSOR_TIMER_VALUE) {
          cursor_on = !cursor_on;
          draw_cursor(cursor_on, true);
        }
        SyscallCreateTimer(TIMER_ONESHOT_REL,
                           CURSOR_TIMER_VALUE, CURSOR_TIMER_INTERVAL_MS);
        break;
      case AppEvent::kKeyPush:
        if (arg.keypush.press) {
          if (dialog_shown) {
            if (arg.keypush.keycode == KEY_ESC) {
              dialog_cancel_pressed();
            } else if (arg.keypush.modifier == KEY_MODIFIER_ALT) {
              switch (arg.keypush.keycode) {
                case KEY_S:
                  dialog_save_pressed();
                  break;
                case KEY_D:
                  dialog_discard_pressed();
                  break;
                case KEY_C:
                  dialog_cancel_pressed();
                  break;
              }
            }
            break;
          }
          int new_scroll_y = scroll_y;
          int new_cursor_x = cursor_x, new_cursor_y = cursor_y;
          int redraw_from = -1, redraw_to = -1;
          if (arg.keypush.modifier == KEY_MODIFIER_CTRL &&
              arg.keypush.keycode == KEY_S) {
            // Ctrl+S (上書き保存)
            if (SaveFile(file_name, data)) edited = false;
          } else if (arg.keypush.ascii > 0 && arg.keypush.keycode != KEY_ESC) {
            if (arg.keypush.ascii == '\b') {
              // Backspace
              if (cursor_x == 0) {
                if (cursor_y > 0) {
                  // 現在の行を前の行と接続し、前の行以降を再描画
                  auto [s, cw] = get_char_range_2(cursor_y - 1, width);
                  data[cursor_y - 1].insert(data[cursor_y - 1].end(),
                                            data[cursor_y].begin(),
                                            data[cursor_y].end());
                  data.erase(data.begin() + cursor_y);
                  new_cursor_y--;
                  new_cursor_x = s;
                  redraw_from = new_cursor_y - scroll_y;
                  edited = true;
                }
              } else {
                // カーソル前の1文字を削除し、現在の行のみを再描画
                auto [s, cw] = get_char_range_2(cursor_y, cursor_x - 1);
                int idx = char_idx[cursor_y - scroll_y][cursor_x - 1];
                data[cursor_y].erase(data[cursor_y].begin() + idx);
                new_cursor_x = s;
                redraw_from = redraw_to = cursor_y - scroll_y;
                edited = true;
              }
            } else if (arg.keypush.ascii == '\n') {
              // Enter
              int idx = cursor_x == 0 ? 0 :
                        char_idx[cursor_y - scroll_y][cursor_x - 1] + 1;
              std::vector<uint32_t> newLine(data[cursor_y].begin() + idx,
                                            data[cursor_y].end());
              data[cursor_y].erase(data[cursor_y].begin() + idx,
                                   data[cursor_y].end());
              data.insert(data.begin() + (cursor_y + 1), newLine);
              new_cursor_y++;
              new_cursor_x = 0;
              redraw_from = cursor_y - scroll_y;
              edited = true;
            } else {
              // 文字の挿入
              int idx = cursor_x == 0 ? 0 :
                        char_idx[cursor_y - scroll_y][cursor_x - 1] + 1;
              int c = arg.keypush.ascii;
              data[cursor_y].insert(data[cursor_y].begin() + idx, c);
              // 挿入後の行について、新しいカーソルの位置を求める
              // タブ(など)の処理があり得るので、単に「一文字分」ではダメ
              std::vector<int> new_char_idx;
              DrawLine(new_char_idx, 0, 0, 0, width, tab_size,
                       data[cursor_y], false, true);
              auto [s, cw] = GetCharRange(new_char_idx, cursor_x);
              if (cursor_x + cw <= width) new_cursor_x += cw;
              redraw_from = redraw_to = cursor_y - scroll_y;
              edited = true;
            }
          } else {
            switch (arg.keypush.keycode) {
              case KEY_DELETE:
                {
                  auto& line_char_idx = char_idx[cursor_y - scroll_y];
                  auto [s, cw] = get_char_range_2(cursor_y, cursor_x);
                  // 行末である
                  if (cw == 0 &&
                      (s == 0 || line_char_idx[s - 1] + 1 == (int)data[cursor_y].size())) {
                    // 次の行を現在の行にくっつけ、現在の行以降を再描画
                    if ((unsigned int)(cursor_y + 1) < data.size()) {
                      data[cursor_y].insert(data[cursor_y].end(),
                                            data[cursor_y + 1].begin(),
                                            data[cursor_y + 1].end());
                      data.erase(data.begin() + (cursor_y + 1));
                      redraw_from = cursor_y - scroll_y;
                      edited = true;
                    }
                  } else {
                    // 現在の行から1文字消し、現在の行のみを再描画
                    int idx = s == 0 ? line_char_idx[s] : line_char_idx[s - 1] + 1;
                    data[cursor_y].erase(data[cursor_y].begin() + idx);
                    redraw_from = redraw_to = cursor_y - scroll_y;
                    edited = true;
                  }
                }
                break;
              case KEY_LEFT:
                new_cursor_x--;
                if (new_cursor_x < 0) {
                  if (cursor_y == 0) {
                    new_cursor_x = 0;
                  } else {
                    new_cursor_y--;
                    new_cursor_x = width;
                  }
                }
                {
                  auto [s, cw] = get_char_range_2(new_cursor_y, new_cursor_x);
                  new_cursor_x = s;
                }
                break;
              case KEY_RIGHT:
                {
                  auto [s, cw] = get_char_range_2(cursor_y, cursor_x);
                  if (cw == 0) {
                    if (cursor_y + 1 < (int)data.size()) {
                      new_cursor_y++;
                      new_cursor_x = 0;
                    }
                  } else {
                    new_cursor_x += cw;
                  }
                }
                break;
              case KEY_UP:
                if (cursor_y > 0) {
                  new_cursor_y--;
                  auto [s, cw] = get_char_range_2(new_cursor_y, cursor_x);
                  new_cursor_x = s;
                }
                break;
              case KEY_DOWN:
                if (cursor_y + 1 < (int)data.size()) {
                  new_cursor_y++;
                  auto [s, cw] = get_char_range_2(new_cursor_y, cursor_x);
                  new_cursor_x = s;
                }
                break;
              case KEY_HOME:
                if (arg.keypush.modifier & KEY_MODIFIER_CTRL) {
                  new_cursor_y = 0;
                }
                new_cursor_x = 0;
                break;
              case KEY_END:
                {
                  if (arg.keypush.modifier & KEY_MODIFIER_CTRL) {
                    new_cursor_y = (int)data.size() - 1;
                  }
                  auto [s, cw] = get_char_range_2(new_cursor_y, width);
                  new_cursor_x = s;
                }
                break;
              case KEY_PGUP:
                new_scroll_y -= height / 2;
                new_cursor_y -= height / 2;
                if (new_scroll_y < 0) new_scroll_y = 0;
                if (new_cursor_y < 0) new_cursor_y = 0;
                {
                  auto [s, cw] = get_char_range_2(new_cursor_y, new_cursor_x);
                  new_cursor_x = s;
                }
                break;
              case KEY_PGDN:
                new_scroll_y += height / 2;
                new_cursor_y += height / 2;
                if (new_scroll_y >= (int)data.size()) {
                  new_scroll_y = (int)data.size() - 1;
                }
                if (new_cursor_y >= (int)data.size()) {
                  new_cursor_y = (int)data.size() - 1;
                }
                {
                  auto [s, cw] = get_char_range_2(new_cursor_y, new_cursor_x);
                  new_cursor_x = s;
                }
                break;
            }
          }
          // カーソルの位置に応じ、スクロール位置を補正する
          if (new_cursor_y < new_scroll_y) new_scroll_y = new_cursor_y;
          if (new_cursor_y >= new_scroll_y + height) {
            new_scroll_y = new_cursor_y - height + 1;
          }
          // 古いカーソルの表示を消す
          if (cursor_on) draw_cursor(false, false);
          if (scroll_y != new_scroll_y) {
            scroll_y = new_scroll_y;
            // スクロール位置が変わっていたら、全体を再描画する
            redraw_from = 0;
            redraw_to = -1;
          }
          cursor_x = new_cursor_x;
          cursor_y = new_cursor_y;
          draw_lines(redraw_from, redraw_to);
          SyscallWinRedraw(hwnd);
        }
        break;
    }
  }
  SyscallCloseWindow(hwnd);
  return ret;
}
