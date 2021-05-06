/**
 * @file font.cpp
 *
 * フォント描画のプログラムを集めたファイル.
 */

#include "font.hpp"

#include <cstdlib>
#include <vector>

#include "fat.hpp"
#include "logger.hpp"

extern const uint8_t _binary_hankaku_bin_start;
extern const uint8_t _binary_hankaku_bin_end;
extern const uint8_t _binary_hankaku_bin_size;

namespace {

const uint8_t* GetFont(char c) {
  auto index = 16 * static_cast<unsigned int>(c);
  if (index >= reinterpret_cast<uintptr_t>(&_binary_hankaku_bin_size)) {
    return nullptr;
  }
  return &_binary_hankaku_bin_start + index;
}

FT_Library ft_library;
std::vector<uint8_t>* nihongo_buf;

Error RenderUnicode(char32_t c, FT_Face face) {
  const auto glyph_index = FT_Get_Char_Index(face, c);
  if (glyph_index == 0) {
    return MAKE_ERROR(Error::kFreeTypeError);
  }

  if (int err = FT_Load_Glyph(face, glyph_index,
                              FT_LOAD_RENDER | FT_LOAD_TARGET_MONO)) {
    return MAKE_ERROR(Error::kFreeTypeError);
  }
  return MAKE_ERROR(Error::kSuccess);
}

} // namespace

void WriteAscii(PixelWriter& writer, Vector2D<int> pos, char c, const PixelColor& color) {
  const uint8_t* font = GetFont(c);
  if (font == nullptr) {
    return;
  }
  for (int dy = 0; dy < 16; ++dy) {
    for (int dx = 0; dx < 8; ++dx) {
      if ((font[dy] << dx) & 0x80u) {
        writer.Write(pos + Vector2D<int>{dx, dy}, color);
      }
    }
  }
}

void WriteString(PixelWriter& writer, Vector2D<int> pos, const char* s, const PixelColor& color) {
  int x = 0;
  while (*s) {
    const auto [ u32, bytes ] = ConvertUTF8To32(s);
    WriteUnicode(writer, pos + Vector2D<int>{8 * x, 0}, u32, color);
    s += bytes;
    x += IsHankaku(u32) ? 1 : 2;
  }
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

std::pair<char32_t, int> ConvertUTF8To32(const char* u8) {
  switch (CountUTF8Size(u8[0])) {
  case 1:
    return {
      static_cast<char32_t>(u8[0]),
      1
    };
  case 2:
    return {
      (static_cast<char32_t>(u8[0]) & 0b0001'1111) << 6 |
      (static_cast<char32_t>(u8[1]) & 0b0011'1111) << 0,
      2
    };
  case 3:
    return {
      (static_cast<char32_t>(u8[0]) & 0b0000'1111) << 12 |
      (static_cast<char32_t>(u8[1]) & 0b0011'1111) << 6 |
      (static_cast<char32_t>(u8[2]) & 0b0011'1111) << 0,
      3
    };
  case 4:
    return {
      (static_cast<char32_t>(u8[0]) & 0b0000'0111) << 18 |
      (static_cast<char32_t>(u8[1]) & 0b0011'1111) << 12 |
      (static_cast<char32_t>(u8[2]) & 0b0011'1111) << 6 |
      (static_cast<char32_t>(u8[3]) & 0b0011'1111) << 0,
      4
    };
  default:
    return { 0, 0 };
  }
}

bool IsHankaku(char32_t c) {
  return c <= 0x7f;
}

WithError<FT_Face> NewFTFace() {
  if (!nihongo_buf) {
    return { 0, MAKE_ERROR(Error::kNoSuchEntry) };
  }

  FT_Face face;
  if (int err = FT_New_Memory_Face(
        ft_library, nihongo_buf->data(), nihongo_buf->size(), 0, &face)) {
    return { face, MAKE_ERROR(Error::kFreeTypeError) };
  }
  if (int err = FT_Set_Pixel_Sizes(face, 16, 16)) {
    return { face, MAKE_ERROR(Error::kFreeTypeError) };
  }
  return { face, MAKE_ERROR(Error::kSuccess) };
}

Error WriteUnicode(PixelWriter& writer, Vector2D<int> pos,
                  char32_t c, const PixelColor& color) {
  if (c <= 0x7f) {
    WriteAscii(writer, pos, c, color);
    return MAKE_ERROR(Error::kSuccess);
  }

  auto [ face, err ] = NewFTFace();
  if (err) {
    WriteAscii(writer, pos, '?', color);
    WriteAscii(writer, pos + Vector2D<int>{8, 0}, '?', color);
    return err;
  }
  if (auto err = RenderUnicode(c, face)) {
    FT_Done_Face(face);
    WriteAscii(writer, pos, '?', color);
    WriteAscii(writer, pos + Vector2D<int>{8, 0}, '?', color);
    return err;
  }
  FT_Bitmap& bitmap = face->glyph->bitmap;

  const int baseline = (face->height + face->descender) *
    face->size->metrics.y_ppem / face->units_per_EM;
  const auto glyph_topleft = pos + Vector2D<int>{
    face->glyph->bitmap_left, baseline - face->glyph->bitmap_top};

  for (int dy = 0; dy < bitmap.rows; ++dy) {
    unsigned char* q = &bitmap.buffer[bitmap.pitch * dy];
    if (bitmap.pitch < 0) {
      q -= bitmap.pitch * bitmap.rows;
    }
    for (int dx = 0; dx < bitmap.width; ++dx) {
      const bool b = q[dx >> 3] & (0x80 >> (dx & 0x7));
      if (b) {
        writer.Write(glyph_topleft + Vector2D<int>{dx, dy}, color);
      }
    }
  }

  FT_Done_Face(face);
  return MAKE_ERROR(Error::kSuccess);
}

void InitializeFont() {
  if (int err = FT_Init_FreeType(&ft_library)) {
    Log(kError, "failed to initialize FreeType library\n");
    exit(1);
  }

  auto [ entry, pos_slash ] = fat::FindFile("/nihongo.ttf");
  if (entry == nullptr || pos_slash) {
    Log(kWarn, "no nihongo.ttf\n");
    return;
  }

  const size_t size = entry->file_size;
  nihongo_buf = new std::vector<uint8_t>(size);
  if (LoadFile(nihongo_buf->data(), size, *entry) != size) {
    delete nihongo_buf;
    Log(kError, "failed to load nihongo.ttf");
    exit(1);
  }
}
