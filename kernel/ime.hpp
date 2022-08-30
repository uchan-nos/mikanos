#pragma once

#include <memory>
#include <string>

#include "message.hpp"
#include "window.hpp"

class IME {
  public:
    IME();
    void SetEnabled(bool enabled);
    bool GetEnabled() const;
    void ResetInput();
    void ProcessMessage(const Message& msg);
    bool IsEmpty() const;

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
    void DrawDotLine(int start_x, int length);
    void AppendChar(char c);
    bool SendChars(const std::string& str);

    // メインの変換結果を表示するウィンドウ
    std::shared_ptr<Window> main_window_;
    // メインの変換結果を表示するレイヤーのID
    unsigned int main_layer_id_;
    // 状態を表示するウィンドウ
    std::shared_ptr<Window> status_window_;
    //状態を表示するレイヤーのID
    unsigned int status_layer_id_;
    // IMEが有効になっているか
    bool enabled_;

    // 入力をひらがなに変換した結果
    std::vector<HiraganaConversionResult> hiragana_;
};

extern IME* ime;

void InitializeIME();
void TaskIME(uint64_t task_id, int64_t data);
