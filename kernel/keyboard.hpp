/**
 * @file keyboard.hpp
 *
 * キーボード制御プログラム．
 */

#pragma once

#include <deque>
#include "message.hpp"

static const int kLControlBitMask = 0b00000001u;
static const int kLShiftBitMask   = 0b00000010u;
static const int kLAltBitMask     = 0b00000100u;
static const int kLGUIBitMask     = 0b00001000u;
static const int kRControlBitMask = 0b00010000u;
static const int kRShiftBitMask   = 0b00100000u;
static const int kRAltBitMask     = 0b01000000u;
static const int kRGUIBitMask     = 0b10000000u;

void InitializeKeyboard();
