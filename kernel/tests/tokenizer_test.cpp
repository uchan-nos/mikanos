#include "tokenizer_test.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <vector>

#include "../tokenizer.hpp"

bool IsTISSame(const std::unique_ptr<TokenizerInnerState> &tis1, const std::unique_ptr<TokenizerInnerState> &tis2) {
  if (tis1 == nullptr ^ tis2 == nullptr) { return false; }
  if (tis1 == nullptr && tis2 == nullptr) { return true; }
  if (tis1->last_state != tis2->last_state) { return false; }
  if (tis1->last_state != tis2->last_state) { return false; }
  if (tis1->tmp_token != tis2->tmp_token) { return false; }
  return true;
}

void PrintTIS(const std::unique_ptr<TokenizerInnerState> &tis) {
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

bool TestTokenize(bool verbose) {
  bool ret = true;
  auto t0 = std::make_unique<TokenizerInnerState>(InToken, BackSlash, "hoge");
  auto t5 = std::make_unique<TokenizerInnerState>(InDoubleQuoted, Init, "piyo");
  auto t2 = std::make_unique<TokenizerInnerState>(Init, BackSlash, "");
  auto t9 = std::make_unique<TokenizerInnerState>(InDoubleQuoted, Init, "ed> hoge");
  struct {
    const int expected;
    const char* result[32];
    int redir;
    int pipe;
    char linebuf[100];
    std::unique_ptr<TokenizerInnerState> eis; // expected
    std::unique_ptr<TokenizerInnerState> iis; // input
  } tbl[] = {
    /* 00 */ {9, {"hoge",  "|", "fuga", "|", "piyo", ">", "foo", ">", "bar"}, 5, 1, R"(hoge |fuga| piyo >foo> bar)", nullptr, nullptr},
    // パイプ, 空白の前後が空白、複数のリダイレクト
    /* 01 */ {3, {"minied",  "-p", "mini|ed>"}, -1, -1, R"(minied   -p "mini|ed>" hoge\)", std::move(t0), nullptr},
    // ダブルクォート中のリダイレクト, パイプ、\終端
    /* 02 */  {4, {"hoge", ">", "piyo", "|"}, -1, -1, R"(hoge ">" piyo '|')", nullptr, nullptr},
    // クウォーテーションの中にパイプ、リダイレクト単体
    /* 03 */ {2, {"hoge fuga", "piyo"}, -1, -1, R"(hoge\ fuga \pi\yo)", nullptr, nullptr},
    // tokenの中のバックスラッシュ
    /* 04 */ {3, {"ho'ge", "fuga", "piyo"}, -1, -1, R"(ho\'ge fu"ga" pi'yo)", nullptr, nullptr},
    // token中のクウォーテーション、token中の\'
    /* 05 */ {1, {"hoge fuga"}, -1, -1, R"('hoge fuga' "piyo)", std::move(t5), nullptr},
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
    if (verbose) printf("case %zd: `%s`\n", i, tbl[i].linebuf);
    std::vector<std::string> tokens;
    int redir = -1, *p_redir = &redir;
    int pipe = -1, *p_pipe = &pipe;
    auto t = std::move(tbl[i].iis);

    t = Tokenize(tbl[i].linebuf, tokens, p_redir, p_pipe, std::move(t));
    // return val check
    
    if (!IsTISSame(t, tbl[i].eis)) { PrintTIS(t); PrintTIS(tbl[i].eis); printf("    \e[38;5;9mERR: invalid return val\e[0m\n"); ret = false; }
    // size of tokens check
    if (tokens.size() != tbl[i].expected) {
      if (verbose) printf("    \e[38;5;9mERR: num of tokens. expected %d but %zu.\e[0m\n", tbl[i].expected, tokens.size());
      ret = false;
    }
    // redir & pipe check
    if (redir != tbl[i].redir) {
      if (verbose) {
        printf("   `%d`, `%d`\n", tbl[i].redir, redir);
        printf("    \e[38;5;9mredir ERR\e[0m\n");
      }
      ret = false;
    }
    if (pipe != tbl[i].pipe) { if (verbose) printf("    \e[38;5;9mpipe ERR\e[0m\n"); ret = false; }
    // token check
    for (size_t j = 0; j < tokens.size(); j++) {
      if (verbose) printf("    cmp `%s`, `%s`\n", tbl[i].result[j], tokens[j].c_str());
      if (tokens[j].c_str() == NULL || tbl[i].result[j] == NULL) { if (verbose) printf("    \e[38;5;9mERR\e[0m\n");ret = false; continue; }
      if (strcmp(tokens[j].c_str(), tbl[i].result[j])) {
        if (verbose) printf("    \e[38;5;9mERR\e[0m\n"); ret = false;
      }
    }
  }
  return ret;
}
