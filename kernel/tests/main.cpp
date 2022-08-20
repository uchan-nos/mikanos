
#include "tokenizer_test.hpp"

int main() {
  int ret = 0;

  printf("test: tokenizer\n");
  ret = ret | test_tokenize();

  if (ret) {
    printf("\e[38;5;9mERR\e[0m\n");
  } else {
    printf("\e[38;5;10mOK\e[0m\n");
  }

  return ret;
}
