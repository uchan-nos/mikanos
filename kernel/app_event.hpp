#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct AppEvent {
  enum Type {
    kQuit,
  } type;
};

#ifdef __cplusplus
} // extern "C"
#endif
