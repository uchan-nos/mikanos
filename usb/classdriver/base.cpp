#include "usb/classdriver/base.hpp"

namespace usb {
  ClassDriver::ClassDriver(Device* dev) : dev_{dev} {
  }

  ClassDriver::~ClassDriver() {
  }
}
