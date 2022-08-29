#pragma once

#include <memory>
#include <string>

#include "message.hpp"
#include "window.hpp"

class IME {
  public:
    IME();
    void ShowWindow(bool show);
    void ResetInput();
    void ProcessMessage(const Message& msg);
    bool IsEmpty();

  private:
    struct HiraganaConversionResult {
      // 変換結果
      std::string converted;
      // 変換元の入力
      std::string source;
      // アルファベット(ローマ字変換対象)か
      bool is_alpha;

      HiraganaConversionResult() = default;
      HiraganaConversionResult(const std::string& _converted,
                               const std::string& _source,
                               bool _is_alpha)
        : converted(_converted), source(_source), is_alpha(_is_alpha) {}
    };

    void Draw();
    void AppendChar(char c);
    bool SendChars(const std::string& str);

    std::shared_ptr<ToplevelWindow> window;
    unsigned int layer_id;
    bool window_shown;

    std::vector<HiraganaConversionResult> hiragana;
};

extern IME* ime;

void InitializeIME();
void TaskIME(uint64_t task_id, int64_t data);
