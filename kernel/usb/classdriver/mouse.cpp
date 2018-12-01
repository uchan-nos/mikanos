#include "usb/classdriver/mouse.hpp"

#include <algorithm>
#include <functional>
#include "usb/memory.hpp"
#include "usb/device.hpp"

int printk(const char* format, ...);

namespace usb {
  HIDMouseDriver::HIDMouseDriver(Device* dev, int interface_index)
      : HIDBaseDriver{dev, interface_index, 3} {
  }

  Error HIDMouseDriver::OnDataReceived() {
    int8_t displacement_x = Buffer()[1];
    int8_t displacement_y = Buffer()[2];
    NotifyMouseMove(displacement_x, displacement_y);
    return MAKE_ERROR(Error::kSuccess);
  }

  void* HIDMouseDriver::operator new(size_t size) {
    return AllocMem(sizeof(HIDMouseDriver), 0, 0);
  }

  void HIDMouseDriver::operator delete(void* ptr) noexcept {
    FreeMem(ptr);
  }

  void HIDMouseDriver::SubscribeMouseMove(
      std::function<void (int8_t displacement_x, int8_t displacement_y)> observer) {
    observers_[num_observers_++] = observer;
  }

  void HIDMouseDriver::NotifyMouseMove(int8_t displacement_x, int8_t displacement_y) {
    for (int i = 0; i < num_observers_; ++i) {
      observers_[i](displacement_x, displacement_y);
    }
  }
}

