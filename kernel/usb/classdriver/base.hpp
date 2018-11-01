/**
 * @file usb/classdriver/base.hpp
 *
 * USB デバイスクラス用のドライバのベースクラス．
 */

#pragma once

#include "error.hpp"
#include "usb/endpoint.hpp"
#include "usb/setupdata.hpp"

namespace usb {
  class Device;

  class ClassDriver {
   public:
    ClassDriver(Device* dev);
    virtual ~ClassDriver();

    virtual Error Initialize() = 0;
    virtual Error SetEndpoint(const EndpointConfig& config) = 0;
    virtual Error OnEndpointsConfigured() = 0;
    virtual Error OnControlOutCompleted(SetupData setup_data,
                                        const void* buf, int len) = 0;
    virtual Error OnControlInCompleted(SetupData setup_data,
                                       const void* buf, int len) = 0;
    virtual Error OnInterruptOutCompleted(const void* buf, int len) = 0;
    virtual Error OnInterruptInCompleted(const void* buf, int len) = 0;

    /** このクラスドライバを保持する USB デバイスを返す． */
    Device* ParentDevice() const { return dev_; }

   private:
    Device* dev_;
  };
}
