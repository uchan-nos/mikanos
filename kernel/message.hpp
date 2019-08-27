#pragma once

struct Message {
  enum Type {
    kInterruptXHCI,
    kInterruptLAPICTimer,
  } type;
};
