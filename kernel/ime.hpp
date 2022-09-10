#pragma once

#include <memory>
#include <string>
#include <vector>

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
    void UpdatePosition();

  private:
    // 入力をひらがなに変換した結果
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

    // 変換対象の1区間
    struct ConversionUnit {
      // 変換元のひらがなの開始位置
      int source_from;
      // 選択中の位置
      int selected_pos;
      // 変換結果の候補
      std::vector<std::string> candidates;
    };

    void Draw();
    void DrawDotLine(int start_x, int length);
    void DrawBoldLine(int start_x, int length);
    void UpdateCandidatePosition();
    void AppendChar(char c);
    bool SendChars(const std::string& str);
    ConversionUnit ConvertRange(int start, int length);
    bool IsConverting() const;
    void BeginConversion(int conversion_mode);
    void CancelConversion();
    bool CommitConversion(bool do_draw = true);

    // メインの変換結果を表示するウィンドウ
    std::shared_ptr<Window> main_window_;
    // メインの変換結果を表示するレイヤーのID
    unsigned int main_layer_id_;
    // 変換候補を表示するウィンドウ
    std::shared_ptr<Window> candidate_window_;
    // 変換候補を表示するレイヤーのID
    unsigned int candidate_layer_id_;
    // 状態を表示するウィンドウ
    std::shared_ptr<Window> status_window_;
    // 状態を表示するレイヤーのID
    unsigned int status_layer_id_;
    // IMEが有効になっているか
    bool enabled_;
    // 変換候補を表示するか
    bool show_candidates_;
    // 前回描画した変換候補ウィンドウの幅
    int candidate_width_;
    // 前回描画した変換候補ウィンドウの高さ
    int candidate_height_;
    // 変換候補ウィンドウのx座標 - メインウィンドウのx座標
    int candidate_offset_x_;

    // 入力をひらがなに変換した結果
    std::vector<HiraganaConversionResult> hiragana_;
    // 変換の状態
    std::vector<ConversionUnit> conversion_;
    // 選択中の変換区間
    int current_conversion_unit_;
};

extern IME* ime;

void InitializeIME();
void TaskIME(uint64_t task_id, int64_t data);
