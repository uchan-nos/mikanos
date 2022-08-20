/**
 * @file tokenizer.hpp
 *
 * シェルのトークナイザを提供する。
 */

#pragma once

#include <vector>
#include <string>

enum State {
  Init,
  InToken,
  InDoubleQuoted,
  InSingleQuoted,
  BackSlash,
};

struct TokenizerInnerState {
  State state;
  State last_state;
  std::string tmp_token;
};

TokenizerInnerState *tokenize(const char *c, std::vector<std::string>& tokens,
  int *redir_idx, int *pipe_idx, TokenizerInnerState *last_istate);