#pragma once

#include "../tokenizer.hpp"

bool IsTisSame(struct TokenizerInnerState *tis1, struct TokenizerInnerState *tis2);
void PrintTis(struct TokenizerInnerState *tis);
int TestTokenize();
