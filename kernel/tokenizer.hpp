/**
 * @file tokenizer.hpp
 *
 * シェルのトークナイザを提供する。
 */

#pragma once

#include <vector>
#include <string>
#include <memory>

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

std::unique_ptr<TokenizerInnerState> Tokenize(const char *c, std::vector<std::string>& tokens,
  int *redir_idx, int *pipe_idx, std::unique_ptr<TokenizerInnerState> last_istate);
