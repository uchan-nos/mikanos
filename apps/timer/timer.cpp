#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../syscall.h"

int main(int argc, char** argv) {
  if (argc <= 1) {
    printf("Usage: timer <msec>\n");
    return 1;
  }

  const unsigned long duration_ms = atoi(argv[1]);
  const auto timeout = SyscallCreateTimer(TIMER_ONESHOT_REL, 1, duration_ms);
  printf("timer created. timeout = %lu\n", timeout.value);

  AppEvent events[1];
  while (true) {
    SyscallReadEvent(events, 1);
    if (events[0].type == AppEvent::kTimerTimeout) {
      printf("%lu msecs elapsed!\n", duration_ms);
      break;
    } else {
      printf("unknown event: type = %d\n", events[0].type);
    }
  }
  return 0;
}
