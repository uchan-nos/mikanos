
#include <iostream>
#include "tokenizer_test.hpp"

int main() {
  const int test_num = 1;
  int success = 0;
  const bool verbose = false;

  if (verbose) std::cout << "test: tokenizer" << std::endl;
  if (TestTokenize(verbose)) {
      success++;
  }

  std::cout << "test: " << success << "/" << test_num << " passed" << std::endl;
  if (test_num == success) {
      std::cout << "\e[38;5;10mOK\e[0m\n" << std::endl;
      return 0;
  } else {
      std::cout << "\e[38;5;9mERR\e[0m\n" << std::endl;
      return -1;
  }
}
