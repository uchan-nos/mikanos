#pragma once

#include <array>

class Error {
 public:
  enum Code {
    kSuccess,
    kFull,
    kEmpty,
    kNoEnoughMemory,
    kIndexOutOfRange,
    kHostControllerNotHalted,
    kInvalidSlotID,
    kPortNotConnected,
    kInvalidEndpointNumber,
    kTransferRingNotSet,
    kAlreadyAllocated,
    kNotImplemented,
    kLastOfCode,  // この列挙子は常に最後に配置する
  };

  Error(Code code) : code_{code} {}

  operator bool() const {
    return this->code_ != kSuccess;
  }

  const char* Name() const {
    return code_names_[static_cast<int>(this->code_)];
  }

 private:
  static constexpr std::array<const char*, kLastOfCode> code_names_ = {
    "kSuccess",
    "kFull",
    "kEmpty",
    "kNoEnoughMemory",
    "kIndexOutOfRange",
    "kHostControllerNotHalted",
    "kInvalidSlotID",
    "kPortNotConnected",
    "kInvalidEndpointNumber",
    "kAlreadyAllocated",
    "kTransferRingNotSet",
    "kNotImplemented",
  };

  Code code_;
};

template <class T>
struct WithError {
  T value;
  Error error;
};
