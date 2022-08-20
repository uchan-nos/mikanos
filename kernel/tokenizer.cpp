#include "tokenizer.hpp"

#include <vector>
#include <string>

TokenizerInnerState *tokenize(const char *c, std::vector<std::string>& tokens,
  int *redir_idx, int *pipe_idx, TokenizerInnerState *last_istate) {
  State state = Init;
  State last_state = Init;
  std::string tmp_token; 
  if (last_istate) {
    state = last_istate->state;
    last_state = last_istate->last_state;
    tmp_token = last_istate->tmp_token;
  }
  auto update_state = [&](State new_state) {
    last_state = state; state = new_state;
  };
  auto revert_state = [&](State new_state) {
    state = last_state; last_state = new_state;
  };
  while (true) {
    switch (state) {
      case BackSlash:
        if (!*c) { revert_state(BackSlash); break; }
        switch (last_state) {
          case Init:
            if (!isspace(*c)) { tmp_token = *c; }
            state = InToken; last_state = BackSlash;
            break;
          case InToken:
            if (*c != '\n') { tmp_token += *c; }
            revert_state(BackSlash);
            break;
          case InDoubleQuoted:
          case InSingleQuoted:
            if (*c != '\n') {
              tmp_token += '\\';
              tmp_token += *c;
            }
            revert_state(BackSlash);
            break;
        }
        break;
      case Init:
        if (*c == '>' || *c == '|') {
          tmp_token = *c;
          tokens.push_back(tmp_token);
          if (*c == '>' && *redir_idx == -1) { *redir_idx = tokens.size() -1; } 
          if (*c == '|' && *pipe_idx == -1) { *pipe_idx = tokens.size() -1; } 
        } else if (*c == '"') { 
          update_state(InDoubleQuoted);
          tmp_token = c[1];
          c++;
        } else if (*c == '\'') {
          update_state(InSingleQuoted);
          tmp_token = c[1];
          c++;
        } else if (!isspace(*c) && *c) {
          update_state(InToken);
          tmp_token = *c;
        }
        break;
      case InToken:
        if (*c == '>' || *c == '|') {
          update_state(Init);
          tokens.push_back(tmp_token);
          tmp_token = *c;
          tokens.push_back(tmp_token);
          if (*c == '>' && *redir_idx == -1) { *redir_idx = tokens.size() -1; } 
          if (*c == '|' && *pipe_idx == -1) { *pipe_idx = tokens.size() -1; } 
        }
        if (isspace(*c) || *c == '\0') {
          update_state(Init);
          tokens.push_back(tmp_token);
          tmp_token.clear();
        } else if (*c != '"' && *c != '\'') {
          tmp_token += *c;
        }
        break;
      case InDoubleQuoted:
        if (*c == '"') {
          update_state(Init);
          tokens.push_back(tmp_token);
          tmp_token.clear();
        } else if (*c) {
          tmp_token += *c;
        }
        break;
      case InSingleQuoted:
        if (*c == '\'') {
          update_state(Init);
          tokens.push_back(tmp_token);
          tmp_token.clear();
        } else if (*c) {
          tmp_token += *c;
        }
        break;
    }
    if (*c == '\0') { break; }
    c++;
    if (*c == '\\') {
      update_state(BackSlash); c++;
    }
  }
  if (state != Init || (state == Init && last_state == BackSlash)) {
    if (!last_istate) {
      last_istate = new TokenizerInnerState ;
    }
    last_istate->state = state;
    last_istate->last_state = last_state;
    last_istate->tmp_token = tmp_token;
    return last_istate;
  } else {
    return nullptr;
  }
}