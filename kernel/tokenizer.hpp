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
  Init, // 初期状態
  InToken, // 通常の文字を受理してトークンを処理している状態
  InDoubleQuoted, //ダブルクウォートを受理してトークンを処理している状態
  InSingleQuoted, // シングルクウォートを受理してトークンを処理している状態
  BackSlash, // バックスラッシュを受理した直後の状態
};

struct TokenizerInnerState {
  State state; // 現在の状態
  State last_state; // １つ前の状態
  std::string tmp_token; // 現在受理している文字列
  TokenizerInnerState(State state=Init, State last_state=Init, const char *tmp_token=""): state(state), last_state(last_state), tmp_token(tmp_token) {}
};
using tis_uniq = std::unique_ptr<TokenizerInnerState>;

tis_uniq Tokenize(const char *c, std::vector<std::string>& tokens,
  int *redir_idx, int *pipe_idx, tis_uniq last_istate);
