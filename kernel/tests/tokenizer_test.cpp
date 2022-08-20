#include "tokenizer_test.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <vector>

#include "../tokenizer.hpp"

bool isTISsame(struct TokenizerInnerState *tis1, struct TokenizerInnerState *tis2) {
  if (tis1 == nullptr ^ tis2 == nullptr) { return false; }
  if (tis1 == nullptr && tis2 == nullptr) { return true; }
  if (tis1->last_state != tis2->last_state) { return false; }
  if (tis1->last_state != tis2->last_state) { return false; }
  if (tis1->tmp_token != tis2->tmp_token) { return false; }
  return true;
}

void printTIS(struct TokenizerInnerState *tis) {
  std::cout << "    >>>>>>>>>>>>>>>>>>>>>>>>>>>" << std::endl;
  if (!tis) { std::cout << "    nullptr" << std::endl; }
  else {
  std::cout << "    stete:          " << tis->last_state << std::endl;
  std::cout << "    last_state:     " << tis->last_state << std::endl;
  std::cout << "    tmp_token:      `" << tis->tmp_token << "`" << std::endl;
  std::cout << "    len(tmp_token): " << tis->tmp_token.length() << std::endl;
  }
  std::cout << "    <<<<<<<<<<<<<<<<<<<<<<<<<<<<" << std::endl;
}

int test_tokenize() {
  int ret = 0;
  struct TokenizerInnerState t0 = { InToken, BackSlash, "hoge"};
  struct TokenizerInnerState t5 = { InDoubleQuoted, Init, "piyo"};
  struct TokenizerInnerState t2 = { Init, BackSlash, ""};
  struct TokenizerInnerState t9 = { InDoubleQuoted, Init, "ed> hoge"};
  struct {
    const int expected;
    const char* result[32];
    int redir;
    int pipe;
    char linebuf[100];
    struct TokenizerInnerState *eis; // expected
    struct TokenizerInnerState *iis; // input
  } tbl[] = {
    /* 00 */ {9, {"hoge",  "|", "fuga", "|", "piyo", ">", "foo", ">", "bar"}, 5, 1, R"(hoge |fuga| piyo >foo> bar)", nullptr, nullptr},
    // パイプ, 空白の前後が空白、複数のリダイレクト
    /* 01 */ {3, {"minied",  "-p", "mini|ed>"}, -1, -1, R"(minied   -p "mini|ed>" hoge\)", &t0, nullptr},
    // ダブルクォート中のリダイレクト, パイプ、\終端
    /* 02 */  {4, {"hoge", ">", "piyo", "|"}, -1, -1, R"(hoge ">" piyo '|')", nullptr, nullptr},
    // クウォーテーションの中にパイプ、リダイレクト単体
    /* 03 */ {2, {"hoge fuga", "piyo"}, -1, -1, R"(hoge\ fuga \pi\yo)", nullptr, nullptr},
    // tokenの中のバックスラッシュ
    /* 04 */ {3, {"ho'ge", "fuga", "piyo"}, -1, -1, R"(ho\'ge fu"ga" pi'yo)", nullptr, nullptr},
    // token中のクウォーテーション、token中の\'
    /* 05 */ {1, {"hoge fuga"}, -1, -1, R"('hoge fuga' "piyo)", &t5, nullptr},
    // 閉じていないクウォーテーション
    /* 06 */ {2, {"hoge", "fugapiyo"}, -1, -1, R"(hoge fuga\
piyo)", nullptr, nullptr},
    // token中の\,改行
    /* 07 */ {2, {"hoge", "fuga piyo"}, -1, -1, R"(hoge "fuga \
piyo")", nullptr, nullptr},
    // ダブルクォート中のバックスラッシュ,改行
    /* 08 */ {2, {"hoge", "fuga\\ \npiyo"}, -1, -1, R"(hoge "fuga\ 
piyo")", nullptr, nullptr},
    //ダブルクォート中のバックスラッシュ,スペース,改行
  };
  for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
    printf("case %zd: `%s`\n", i, tbl[i].linebuf);
    std::vector<std::string> tokens;
    int redir = -1, *p_redir = &redir;
    int pipe = -1, *p_pipe = &pipe;
    struct TokenizerInnerState *t = tbl[i].iis;

    t = tokenize(tbl[i].linebuf, tokens, p_redir, p_pipe, t);
    // return val check
    
    if (!isTISsame(t, tbl[i].eis)) { printTIS(t); printTIS(tbl[i].eis); printf("    \e[38;5;9mERR: invalid return val\e[0m\n"); ret = -1; }
    // size of tokens check
    if (tokens.size() != tbl[i].expected) {
      printf("    \e[38;5;9mERR: num of tokens. expected %d but %zu.\e[0m\n", tbl[i].expected, tokens.size());
      ret = -1;
    }
    // redir & pipe check
    if (redir != tbl[i].redir) { printf("   `%d`, `%d`\n", tbl[i].redir, redir); printf("    \e[38;5;9mredir ERR\e[0m\n"); ret = -1; }
    if (pipe != tbl[i].pipe) { printf("    \e[38;5;9mpipe ERR\e[0m\n"); ret = -1; }
    // token check
    for (size_t j = 0; j < tokens.size(); j++) {
      printf("    cmp `%s`, `%s`\n", tbl[i].result[j], tokens[j].c_str());
      if (tokens[j].c_str() == NULL || tbl[i].result[j] == NULL) { printf("    \e[38;5;9mERR\e[0m\n");ret = -1; continue; }
      if (strcmp(tokens[j].c_str(), tbl[i].result[j])) {
        printf("    \e[38;5;9mERR\e[0m\n"); ret = -1;
      }
    }
  }
  return ret;
}
