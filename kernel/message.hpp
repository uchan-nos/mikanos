#pragma once

struct Message {
  enum Type {
    kInterruptXHCI,
  } type;
};
