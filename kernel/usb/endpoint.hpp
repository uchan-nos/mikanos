/**
 * @file usb/endpoint.hpp
 *
 * エンドポイント設定に関する機能．
 */

#pragma once

#include "error.hpp"

namespace usb {
  enum class EndpointType {
    kControl = 0,
    kIsochronous = 1,
    kBulk = 2,
    kInterrupt = 3,
  };

  struct EndpointConfig {
    /** エンドポイント番号 */
    int ep_num;

    /** このエンドポイントが IN 方向なら true */
    bool dir_in;

    /** このエンドポイントの種別 */
    EndpointType ep_type;

    /** このエンドポイントの最大パケットサイズ（バイト） */
    int max_packet_size;

    /** このエンドポイントの制御周期（125*2^(interval-1) マイクロ秒） */
    int interval;
  };
}
