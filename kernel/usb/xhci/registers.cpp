#include "usb/xhci/registers.hpp"

namespace {
  template <class Ptr, class Disp>
  Ptr AddOrNull(Ptr p, Disp d) {
    return d == 0 ? nullptr : p + d;
  }
}

namespace usb::xhci {
  ExtendedRegisterList::Iterator& ExtendedRegisterList::Iterator::operator++() {
    if (reg_) {
      reg_ = AddOrNull(reg_, reg_->Read().bits.next_pointer);
      static_assert(sizeof(*reg_) == 4);
    }
    return *this;
  }

  ExtendedRegisterList::ExtendedRegisterList(uint64_t mmio_base,
                                             HCCPARAMS1_Bitmap hccp)
    : first_{AddOrNull(reinterpret_cast<ValueType*>(mmio_base),
                       hccp.bits.xhci_extended_capabilities_pointer)} {}
}
