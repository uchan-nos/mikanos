/**
 * @file usb/xhci/speed.hpp
 *
 * Protocol Speed ID のデフォルト定義．PSIC == 0 のときのみ有効．
 */

#pragma once

namespace usb::xhci {
  const int kFullSpeed = 1;
  const int kLowSpeed = 2;
  const int kHighSpeed = 3;
  const int kSuperSpeed = 4;
  const int kSuperSpeedPlus = 5;
}
