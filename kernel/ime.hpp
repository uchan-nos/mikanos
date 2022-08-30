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
    class HiraganaConversionResult {
      public:
        HiraganaConversionResult() = default;
        HiraganaConversionResult(const std::string& converted,
                                 const std::string& source,
                                 bool is_alpha)
          : converted_{converted}, source_{source}, is_alpha_{is_alpha} {}

        const std::string& Converted() const { return converted_; }
        const std::string& Source() const { return source_; }
        const bool IsAlpha() const { return is_alpha_; }

      private:
        // 変換結果
        std::string converted_;
        // 変換元の入力
        std::string source_;
        // アルファベット(ローマ字変換対象)か
        bool is_alpha_;
    };

    void Draw();
    void AppendChar(char c);
    bool SendChars(const std::string& str);

    std::shared_ptr<ToplevelWindow> window_;
    unsigned int layer_id_;
    bool window_shown_;

    std::vector<HiraganaConversionResult> hiragana_;
};

extern IME* ime;

void InitializeIME();
void TaskIME(uint64_t task_id, int64_t data);
