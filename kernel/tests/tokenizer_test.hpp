#pragma once

#include "../tokenizer.hpp"

bool isTISsame(struct TokenizerInnerState *tis1, struct TokenizerInnerState *tis2);
void printTIS(struct TokenizerInnerState *tis);
int test_tokenize();
