#include "usb/device.hpp"

#include "usb/setupdata.hpp"

namespace usb {
  Error GetDescriptor(Device& dev, int ep_num,
                      uint8_t desc_type, uint8_t desc_index,
                      void* buf, int len, bool debug) {
    SetupData setup_data{};
    setup_data.bits.direction = request_type::kIn;
    setup_data.bits.type = request_type::kStandard;
    setup_data.bits.recipient = request_type::kDevice;
    setup_data.bits.request = request::kGetDescriptor;
    setup_data.bits.value = (static_cast<uint16_t>(desc_type) << 8) | desc_index;
    setup_data.bits.index = 0;
    setup_data.bits.length = len;
    return dev.ControlIn(ep_num, setup_data.data, buf, len);
  }

  /*
  Error ConfigureEndpoints(Device& dev, bool debug) {
    if (debug) printk("configuring endpoints\n");

    initialize_phase_ = 1;

    memset(desc_buf_, 0, sizeof(desc_buf_));
    if (auto err = GetDescriptor(descriptor_type::kDevice, 0); IsError(err))
    {
      return err;
    }

    return error::kSuccess;
  }
  */
}
