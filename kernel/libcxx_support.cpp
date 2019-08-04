#include <new>
#include <cerrno>

int printk(const char* format, ...);

std::new_handler std::get_new_handler() noexcept {
  return [] {
    printk("not enough memory\n");
    exit(1);
  };
}

extern "C" int posix_memalign(void**, size_t, size_t) {
  return ENOMEM;
}
