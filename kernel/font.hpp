#pragma once

#include <cstdint>
#include <utility>
#include "graphics.hpp"

void WriteAscii(PixelWriter& writer, Vector2D<int> pos, char c, const PixelColor& color);
void WriteString(PixelWriter& writer, Vector2D<int> pos, const char* s, const PixelColor& color);

int CountUTF8Size(uint8_t c);
std::pair<char32_t, int> ConvertUTF8To32(const char* u8);
bool IsHankaku(char32_t c);
void WriteUnicode(PixelWriter& writer, Vector2D<int> pos,
                  char32_t c, const PixelColor& color);
