/**
 * @file usb/descriptor.hpp
 *
 * USB Descriptor の定義集．
 */

#pragma once

#include <cstdint>
#include <array>

namespace usb {
  struct DeviceDescriptor {
    static const uint8_t kType = 1;

    uint8_t length;             // offset 0
    uint8_t descriptor_type;    // offset 1
    uint16_t usb_release;       // offset 2
    uint8_t device_class;       // offset 4
    uint8_t device_sub_class;   // offset 5
    uint8_t device_protocol;    // offset 6
    uint8_t max_packet_size;    // offset 7
    uint16_t vendor_id;         // offset 8
    uint16_t product_id;        // offset 10
    uint16_t device_release;    // offset 12
    uint8_t manufacturer;       // offset 14
    uint8_t product;            // offset 15
    uint8_t serial_number;      // offset 16
    uint8_t num_configurations; // offset 17
  } __attribute__((packed));

  struct ConfigurationDescriptor {
    static const uint8_t kType = 2;

    uint8_t length;             // offset 0
    uint8_t descriptor_type;    // offset 1
    uint16_t total_length;      // offset 2
    uint8_t num_interfaces;     // offset 4
    uint8_t configuration_value;// offset 5
    uint8_t configuration_id;   // offset 6
    uint8_t attributes;         // offset 7
    uint8_t max_power;          // offset 8
  } __attribute__((packed));

  struct InterfaceDescriptor {
    static const uint8_t kType = 4;

    uint8_t length;             // offset 0
    uint8_t descriptor_type;    // offset 1
    uint8_t interface_number;   // offset 2
    uint8_t alternate_setting;  // offset 3
    uint8_t num_endpoints;      // offset 4
    uint8_t interface_class;    // offset 5
    uint8_t interface_sub_class;// offset 6
    uint8_t interface_protocol; // offset 7
    uint8_t interface_id;       // offset 8
  } __attribute__((packed));

  struct EndpointDescriptor {
    static const uint8_t kType = 5;

    uint8_t length;             // offset 0
    uint8_t descriptor_type;    // offset 1
    union {
      uint8_t data;
      struct {
        uint8_t number : 4;
        uint8_t : 3;
        uint8_t dir_in : 1;
      } __attribute__((packed)) bits;
    } endpoint_address;         // offset 2
    union {
      uint8_t data;
      struct {
        uint8_t transfer_type : 2;
        uint8_t sync_type : 2;
        uint8_t usage_type : 2;
        uint8_t : 2;
      } __attribute__((packed)) bits;
    } attributes;               // offset 3
    uint16_t max_packet_size;   // offset 4
    uint8_t interval;           // offset 6
  } __attribute__((packed));

  struct HIDDescriptor {
    static const uint8_t kType = 33;

    uint8_t length;             // offset 0
    uint8_t descriptor_type;    // offset 1
    uint16_t hid_release;       // offset 2
    uint8_t country_code;       // offset 4
    uint8_t num_descriptors;    // offset 5

    struct ClassDescriptor {
      /** @brief クラス特有ディスクリプタのタイプ値． */
      uint8_t descriptor_type;
      /** @brief クラス特有ディスクリプタのバイト数． */
      uint16_t descriptor_length;
    } __attribute__((packed));

    /** @brief HID 特有のディスクリプタに関する情報を得る．
     *
     * HID はクラス特有（class-specific）のディスクリプタを 1 つ以上持つ．
     * その数は num_descriptors に記載されている．
     * Report ディスクリプタ（type = 34）は HID デバイスであれば必ず存在するため，
     * num_descriptors は必ず 1 以上となる．
     *
     * @param index  取得するディスクリプタの番号．0 <= index < num_descriptors.
     * @return index で指定されたディスクリプタの情報．index が範囲外なら nullptr.
     */
    ClassDescriptor* GetClassDescriptor(size_t index) const {
      if (index >= num_descriptors) {
        return nullptr;
      }
      const auto end_of_struct =
        reinterpret_cast<uintptr_t>(this) + sizeof(HIDDescriptor);
      return reinterpret_cast<ClassDescriptor*>(end_of_struct) + index;
    }
  } __attribute__((packed));

  template <class T>
  T* DescriptorDynamicCast(uint8_t* desc_data) {
    if (desc_data[1] == T::kType) {
      return reinterpret_cast<T*>(desc_data);
    }
    return nullptr;
  }

  template <class T>
  const T* DescriptorDynamicCast(const uint8_t* desc_data) {
    if (desc_data[1] == T::kType) {
      return reinterpret_cast<const T*>(desc_data);
    }
    return nullptr;
  }
}
