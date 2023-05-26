#include "usb/classdriver/mouse.hpp"

#include <algorithm>
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "logger.hpp"

namespace usb {
  HIDMouseDriver::HIDMouseDriver(Device* dev, int interface_index)
      : HIDBaseDriver{dev, interface_index, 3} {
  }

  Error HIDMouseDriver::OnDataReceived() {
    uint8_t buttons = Buffer()[0];
    int8_t displacement_x = Buffer()[1];
    int8_t displacement_y = Buffer()[2];
    NotifyMouseMove(buttons, displacement_x, displacement_y);
    Log(kDebug, "%02x,(%3d,%3d)\n", buttons, displacement_x, displacement_y);
    return MAKE_ERROR(Error::kSuccess);
  }

  void* HIDMouseDriver::operator new(size_t size) {
    return AllocMem(sizeof(HIDMouseDriver), 0, 0);
  }

  void HIDMouseDriver::operator delete(void* ptr) noexcept {
    FreeMem(ptr);
  }

  void HIDMouseDriver::SubscribeMouseMove(std::function<ObserverType> observer) {
    observers_[num_observers_++] = observer;
  }

  std::function<HIDMouseDriver::ObserverType> HIDMouseDriver::default_observer;

  void HIDMouseDriver::NotifyMouseMove(
      uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
    for (int i = 0; i < num_observers_; ++i) {
      observers_[i](buttons, displacement_x, displacement_y);
    }
  }
}

