/**
 * @file usb/classdriver/base.hpp
 *
 * USB デバイスクラス用のドライバのベースクラス．
 */

#pragma once

#include "error.hpp"
#include "usb/endpoint.hpp"

namespace usb {
  class ClassDriver {
   public:
    virtual ~ClassDriver();
    virtual Error Initialize() = 0;
    virtual Error SetEndpoint(const EndpointConfig& config) = 0;
  };
}
