/**
 * @file ime.cpp
 *
 * IMEを実装したファイル．
 */

#include "ime.hpp"

#include <cctype>
#include <limits>
#include <unordered_map>

#include "font.hpp"
#include "graphics.hpp"
#include "layer.hpp"
#include "task.hpp"

const PixelColor kIMETransparentColor{255, 1, 1};

// 文字種指定の変換ではなく、通常のかな漢字変換を行う
const int kNormalConversion = 0;
// 各文字種が変換候補の後ろから何番目か
const int kOffsetHiragana = -5;
const int kOffsetFullKatakana = -4;
const int kOffsetHalfKatakana = -3;
const int kOffsetFullAlpha = -2;
const int kOffsetHalfAlpha = -1;

static const std::unordered_map<std::string, std::string>
  *hiragana_map, // 半角英数 → ひらがな
  *alpha_map, // 半角英数 → 全角英数
  *full_katakana_map, // ひらがな → 全角カタカナ
  *half_katakana_map; // 全角カタカナ → 半角カタカナ

// 文字変換テーブル (変換元が1文字のみのもの) を文字列に適用する
static std::string ApplyMap(const std::string& str,
                            const std::unordered_map<std::string, std::string>& map) {
  std::string result;
  for (size_t i = 0; i < str.size(); ) {
    int char_size = CountUTF8Size(str[i]);
    std::string query = str.substr(i, char_size);
    auto it = map.find(query);
    if (it != map.end()) {
      result += it->second;
    } else {
      result += query;
    }
    i += char_size;
  }
  return result;
}

IME::IME() {
  main_window_ = std::make_shared<Window>(ScreenSize().x, 19, screen_config.pixel_format);
  main_window_->SetTransparentColor(kIMETransparentColor);
  main_layer_id_ = layer_manager->NewLayer(kIMELayerPriority)
    .SetWindow(main_window_)
    .SetDraggable(false)
    .Move({580, 480})
    .ID();

  status_window_ = std::make_shared<Window>(16, 16, screen_config.pixel_format);
  status_layer_id_ = layer_manager->NewLayer(kBgObjectLayerPriority)
    .SetWindow(status_window_)
    .SetDraggable(false)
    .Move(ScreenSize() - status_window_->Size() -
          Vector2D<int>{8 * 10 + 4 + 16, 8 + 8})
    .ID();
  layer_manager->UpDown(status_layer_id_, std::numeric_limits<int>::max());

  enabled_ = false;
  Draw();
}

void IME::SetEnabled(bool enabled) {
  __asm__("cli");
  if (enabled) {
    layer_manager->UpDown(main_layer_id_, std::numeric_limits<int>::max());
  } else {
    Layer* layer = layer_manager->FindLayer(main_layer_id_);
    layer_manager->UpDown(main_layer_id_, -1);
    if (layer != nullptr) {
      const auto pos = layer->GetPosition();
      const auto size = layer->GetWindow()->Size();
      layer_manager->Draw({pos, size});
    }
    ResetInput();
  }
  __asm__("sti");
  enabled_ = enabled;
  Draw();
}

bool IME::GetEnabled() const {
  return enabled_;
}

void IME::ResetInput() {
  hiragana_.clear();
  conversion_.clear();
  Draw();
}

void IME::ProcessMessage(const Message& msg) {
  auto handle_function_key = [&](int offset) {
    if (IsConverting()) {
      auto& unit = conversion_[current_conversion_unit_];
      unit.selected_pos = unit.candidates.size() + offset;
      Draw();
    } else {
      BeginConversion(offset);
    }
  };

  if(msg.type == Message::kKeyPush) {
    const auto& info = msg.arg.keyboard;
    if (info.press) {
      if (info.ascii == '\n') {
        // 決定
        CommitConversion();
      } else if (info.keycode == 41 /* Esc */) {
        if (IsConverting()) {
          CancelConversion();
        } else {
          ResetInput();
        }
      } else if (info.ascii == '\b') {
        // バックスペース
        if (IsConverting()) {
          CancelConversion();
        } else {
          if (!hiragana_.empty()) {
            hiragana_.pop_back();
            Draw();
          }
        }
      } else if (info.ascii == ' ') {
        if (hiragana_.empty()) {
          // 全角スペースを入力
          SendChars("　");
        } else if (IsConverting()) {
          // 次の変換候補に進む
          auto& unit = conversion_[current_conversion_unit_];
          unit.selected_pos = (unit.selected_pos + 1) % unit.candidates.size();
          Draw();
        } else {
          // 変換
          BeginConversion(kNormalConversion);
        }
      } else if (info.ascii > ' ') {
        // 文字入力
        if (IsConverting()) CommitConversion(false);
        AppendChar(info.ascii);
        Draw();
      } else if (info.keycode == 63 /* F6 */) {
        handle_function_key(kOffsetHiragana);
      } else if (info.keycode == 64 /* F7 */) {
        handle_function_key(kOffsetFullKatakana);
      } else if (info.keycode == 65 /* F8 */) {
        handle_function_key(kOffsetHalfKatakana);
      } else if (info.keycode == 66 /* F9 */) {
        handle_function_key(kOffsetFullAlpha);
      } else if (info.keycode == 67 /* F10 */) {
        handle_function_key(kOffsetHalfAlpha);
      } else if (info.keycode == 80 /* ← */) {
        if (IsConverting()) {
          if (info.modifier & 2 /* Shift */) {
            int current_pos = conversion_[current_conversion_unit_].source_from;
            int next_pos = conversion_[current_conversion_unit_ + 1].source_from;
            if (current_pos + 1 < next_pos) {
              conversion_[current_conversion_unit_] = ConvertRange(current_pos,
                                                                   next_pos - current_pos - 1);
              conversion_.insert(conversion_.begin() + current_conversion_unit_ + 1,
                                 ConvertRange(next_pos - 1, 1));
            }
          } else {
            if (current_conversion_unit_ > 0) --current_conversion_unit_;
          }
          Draw();
        }
      } else if (info.keycode == 79 /* → */) {
          if (!conversion_[current_conversion_unit_ + 1].candidates.empty()) {
            if (info.modifier & 2 /* Shift */) {
              int current_pos = conversion_[current_conversion_unit_].source_from;
              int next_pos = conversion_[current_conversion_unit_ + 1].source_from;
              int next_next_pos = conversion_[current_conversion_unit_ + 2].source_from;
              conversion_[current_conversion_unit_] = ConvertRange(current_pos,
                                                                   next_pos - current_pos + 1);
              if (next_pos + 1 < next_next_pos) {
                conversion_[current_conversion_unit_ + 1] = ConvertRange(next_pos + 1,
                                                                         next_next_pos - next_pos - 1);
              } else {
                conversion_.erase(conversion_.begin() + current_conversion_unit_ + 1);
              }
            } else {
              ++current_conversion_unit_;
            }
          }
          Draw();
      } else if (info.keycode == 82 /* ↑ */) {
        if (IsConverting()) {
          auto& unit = conversion_[current_conversion_unit_];
          --unit.selected_pos;
          if (unit.selected_pos < 0) unit.selected_pos = unit.candidates.size() - 1;
          Draw();
        }
      } else if (info.keycode == 81 /* ↓ */) {
        if (IsConverting()) {
          auto& unit = conversion_[current_conversion_unit_];
          unit.selected_pos = (unit.selected_pos + 1) % unit.candidates.size();
          Draw();
        }
      } else if (info.keycode == 75 /* PgUp */) {
        if (IsConverting()) {
          auto& unit = conversion_[current_conversion_unit_];
          unit.selected_pos = unit.selected_pos / 9 * 9 - 9;
          if (unit.selected_pos < 0) unit.selected_pos = (unit.candidates.size() - 1) / 9 * 9;
          Draw();
        }
      } else if (info.keycode == 78 /* PgDn */) {
        if (IsConverting()) {
          auto& unit = conversion_[current_conversion_unit_];
          unit.selected_pos = unit.selected_pos / 9 * 9 + 9;
          if (static_cast<unsigned int>(unit.selected_pos) >= unit.candidates.size()) {
            unit.selected_pos = 0;
          }
          Draw();
        }
      }
    }
  }
}

bool IME::IsEmpty() const {
  return hiragana_.empty();
}

void IME::Draw() {
  FillRectangle(*status_window_->Writer(), {0, 0}, status_window_->Size(), {0, 0, 0});
  if (!enabled_) {
    WriteString(*status_window_->Writer(), {8, 0}, "A", {255, 255, 255});
    layer_manager->Draw(status_layer_id_);
    return;
  }
  WriteString(*status_window_->Writer(), {0, 0}, "あ", {255, 255, 255});
  layer_manager->Draw(status_layer_id_);

  int draw_right = 0;
  const auto& area_size = main_window_->Size();
  auto draw_unit = [&](const std::string& str, bool is_selected, bool is_last) {
    int draw_left = draw_right;
    std::string string_to_draw;
    bool limit_reached = false;
    for (size_t i = 0; i < str.size(); ) {
      auto [c, size] = ConvertUTF8To32(&str[i]);
      if (size <= 0) break;
      auto new_draw_right = draw_right + 8 * (IsHankaku(c) ? 1 : 2);
      if (new_draw_right > area_size.x) {
        i = hiragana_.size();
        limit_reached = true;
        break;
      }
      string_to_draw.append(str.begin() + i, str.begin() + i + size);
      i += size;
      draw_right = new_draw_right;
    }
    FillRectangle(*main_window_->Writer(), {draw_left, 0},
                  {draw_right - draw_left, area_size.y}, {255, 255, 255});
    WriteString(*main_window_->Writer(), {draw_left, 0},
                string_to_draw.c_str(), {0, 0, 0});
    int line_length = draw_right - draw_left - (is_last || limit_reached ? 0 : 3);
    if (is_selected) {
      DrawBoldLine(draw_left, line_length);
    } else {
      DrawDotLine(draw_left, line_length);
    }
    return limit_reached;
  };

  if (IsConverting()) {
    for (size_t i = 0; !conversion_[i].candidates.empty(); ++i) {
      auto& unit = conversion_[i];
      if (draw_unit(unit.candidates[unit.selected_pos],
                    static_cast<unsigned int>(current_conversion_unit_) == i,
                    conversion_[i + 1].candidates.empty())) {
        // 描画できる右端に到達した
        break;
      }
    }
  } else {
    std::string converted_string;
    for (size_t i = 0; i < hiragana_.size(); ++i) {
      converted_string += hiragana_[i].Converted();
    }
    draw_unit(converted_string, false, true);
  }
  FillRectangle(*main_window_->Writer(), {draw_right, 0},
                {area_size.x - draw_right, area_size.y}, kIMETransparentColor);
  layer_manager->Draw({layer_manager->FindLayer(main_layer_id_)->GetPosition(),
                       main_window_->Size()});
}

void IME::DrawDotLine(int start_x, int length) {
  for (int i = 0; i < length; i++) {
    if (i % 3 != 2) main_window_->Writer()->Write({start_x + i, 18}, {0, 0, 0});
  }
}

void IME::DrawBoldLine(int start_x, int length) {
  FillRectangle(*main_window_->Writer(), {start_x, 17}, {length, 2}, {0, 0, 0});
}

void IME::AppendChar(char c) {
  // 数字の後だと変化する記号を処理する
  if (!hiragana_.empty() && isdigit(hiragana_.back().Source()[0])) {
    std::string res = "";
    if (c == '.') res = "．";
    else if (c == ',') res = "，";
    else if (c == '-') res = "－";
    if (!res.empty()) {
      hiragana_.push_back({res, std::string(1, c), false});
      return;
    }
  }

  // ローマ字からの変換を処理する (数字・記号を含む)
  auto [source, converted] = [&]() -> std::tuple<std::string, std::string> {
    if (!hiragana_.empty() && hiragana_.back().IsAlpha()) {
      if (hiragana_.size() >= 2 && hiragana_[hiragana_.size() - 2].IsAlpha()) {
        // 3文字からの変換を試す
        std::string key;
        key.push_back(tolower(hiragana_[hiragana_.size() - 2].Source()[0]));
        key.push_back(tolower(hiragana_.back().Source()[0]));
        key.push_back(tolower(c));
        auto it = hiragana_map->find(key);
        if (it != hiragana_map->end()) {
          std::string source = hiragana_[hiragana_.size() - 2].Source() +
                               hiragana_.back().Source() + c;
          hiragana_.erase(hiragana_.end() - 2, hiragana_.end());
          return {source, it->second};
        }
      }
      // 2文字からの変換を試す
      std::string key;
      key.push_back(tolower(hiragana_.back().Source()[0]));
      key.push_back(tolower(c));
      auto it = hiragana_map->find(key);
      if (it != hiragana_map->end()) {
        std::string source = hiragana_.back().Source() + c;
        hiragana_.pop_back();
        return {source, it->second};
      }
    }
    // 1文字からの変換を試す
    std::string key(1, tolower(c));
    auto it = hiragana_map->find(key);
    if (it != hiragana_map->end()) {
      return {key, it->second};
    }
    return {"", ""};
  }();
  if (!converted.empty()) {
    int first_character_size = CountUTF8Size(converted[0]);
    if (converted.size() > static_cast<unsigned int>(first_character_size)) {
      // 2文字に変換する (3文字以上への変換は存在しないので)
      hiragana_.push_back({converted.substr(0, first_character_size),
                           source.substr(0, 1), false});
      hiragana_.push_back({converted.substr(first_character_size),
                           source.substr(1), false});
    } else {
      // 1文字に変換する
      hiragana_.push_back({converted, source, false});
    }
    return;
  }

  int cl = tolower(c);
  if (!hiragana_.empty() && hiragana_.back().IsAlpha() &&
      cl != 'a' && cl != 'i' && cl != 'u' && cl != 'e' && cl != 'o') {
    if (tolower(hiragana_.back().Source()[0]) == cl) {
      // 小さい「っ」の入力(同じ子音を重ねる)
      std::string source = hiragana_.back().Source();
      hiragana_.pop_back();
      hiragana_.push_back({"っ", source, false});
    } else if(tolower(hiragana_.back().Source()[0]) == 'n' && cl != 'y') {
      // n 1個による「ん」の入力
      std::string source = hiragana_.back().Source();
      hiragana_.pop_back();
      hiragana_.push_back({"ん", source, false});
    }
  }

  // 入力された文字(アルファベット)を置く
  std::string key_as_is = std::string(1, c);
  auto it = alpha_map->find(key_as_is);
  if (it != alpha_map->end()) {
    hiragana_.push_back({it->second, key_as_is, true});
  } else {
    hiragana_.push_back({key_as_is, key_as_is, true});
  }
}

bool IME::SendChars(const std::string& str) {
  auto dest_layer = active_layer->GetActive();
  __asm__("cli");
  auto dest_task_itr = layer_task_map->find(dest_layer);
  __asm__("sti");
  if (dest_task_itr == layer_task_map->end()) return false;
  auto dest_task_id = dest_task_itr->second;
  Message msg{Message::kCharInput, task_manager->CurrentTask().ID()};
  for (auto it = str.begin(); it != str.end(); ) {
    auto [c, size] = ConvertUTF8To32(&*it);
    if (size <= 0) break;
    msg.arg.char_input.ch = c;
    __asm__("cli");
    task_manager->SendMessage(dest_task_id, msg);
    __asm__("sti");
    it += size;
  }
  return true;
}

IME::ConversionUnit IME::ConvertRange(int start, int length) {
  std::vector<std::string> candidates;

  // 各種文字への変換を行う
  std::string hiragana;
  std::string full_katakana, half_katakana;
  std::string full_alpha, half_alpha;
  for (int i = 0; i < length; i++) {
    if (i + 1 >= length && hiragana_[start + i].IsAlpha() &&
        hiragana_[start + i].Source() =="n") {
      // 最後に1個だけ残っている「n」を「ん」に変換する
      hiragana += "ん";
    } else {
      hiragana += hiragana_[start + i].Converted();
    }
    half_alpha += hiragana_[start + i].Source();
  }
  full_katakana = ApplyMap(hiragana, *full_katakana_map);
  half_katakana = ApplyMap(full_katakana, *half_katakana_map);
  full_alpha = ApplyMap(half_alpha, *alpha_map);

  // 各種文字への変換結果を変換候補に加える
  candidates.push_back(hiragana);
  candidates.push_back(full_katakana);
  candidates.push_back(half_katakana);
  candidates.push_back(full_alpha);
  candidates.push_back(half_alpha);

  return ConversionUnit{start, 0, candidates};
}

bool IME::IsConverting() const {
  return !conversion_.empty();
}

void IME::BeginConversion(int conversion_mode) {
  if (hiragana_.empty()) return;
  conversion_.clear();
  if (conversion_mode == kNormalConversion) {
    // TODO
    conversion_.push_back(ConvertRange(0, hiragana_.size()));
    conversion_.back().selected_pos = conversion_.back().candidates.size() + kOffsetFullKatakana;
    conversion_.push_back({static_cast<int>(hiragana_.size()), 0, {}}); // 番兵
  } else if (kOffsetHiragana <= conversion_mode &&
             conversion_mode <= kOffsetHalfAlpha) {
    conversion_.push_back(ConvertRange(0, hiragana_.size()));
    conversion_.back().selected_pos = conversion_.back().candidates.size() + conversion_mode;
    conversion_.push_back({static_cast<int>(hiragana_.size()), 0, {}}); // 番兵
  } else {
    return;
  }
  current_conversion_unit_ = 0;
  Draw();
}

void IME::CancelConversion() {
  conversion_.clear();
  Draw();
}

bool IME::CommitConversion(bool do_draw) {
  std::string result = "";
  if (IsConverting()) {
    for (size_t i = 0; i < conversion_.size(); ++i) {
      result += conversion_[i].candidates[conversion_[i].selected_pos];
    }
  } else {
    for (size_t i = 0; i < hiragana_.size(); ++i) {
      result += hiragana_[i].Converted();
    }
  }
  if (SendChars(result)) {
    ResetInput();
    if (do_draw) Draw();
    return true;
  }
  return false;
}

IME* ime;

void InitializeIME() {
  ime = new IME();

  hiragana_map = new std::unordered_map<std::string, std::string>{
    {"a", "あ"}, {"i", "い"}, {"u", "う"}, {"e", "え"}, {"o", "お"},
    {"ka", "か"}, {"ki", "き"}, {"ku", "く"}, {"ke", "け"}, {"ko", "こ"},
    {"sa", "さ"}, {"si", "し"}, {"su", "す"}, {"se", "せ"}, {"so", "そ"},
    {"ta", "た"}, {"ti", "ち"}, {"tu", "つ"}, {"te", "て"}, {"to", "と"},
    {"na", "な"}, {"ni", "に"}, {"nu", "ぬ"}, {"ne", "ね"}, {"no", "の"},
    {"ha", "は"}, {"hi", "ひ"}, {"hu", "ふ"}, {"he", "へ"}, {"ho", "ほ"},
    {"ma", "ま"}, {"mi", "み"}, {"mu", "む"}, {"me", "め"}, {"mo", "も"},
    {"ya", "や"}, {"yu", "ゆ"}, {"yo", "よ"},
    {"ra", "ら"}, {"ri", "り"}, {"ru", "る"}, {"re", "れ"}, {"ro", "ろ"},
    {"wa", "わ"}, {"wi", "うぃ"}, {"wu", "う"}, {"we", "うぇ"}, {"wo", "を"},
    {"nn", "ん"},
    {"ga", "が"}, {"gi", "ぎ"}, {"gu", "ぐ"}, {"ge", "げ"}, {"go", "ご"},
    {"za", "ざ"}, {"zi", "じ"}, {"zu", "ず"}, {"ze", "ぜ"}, {"zo", "ぞ"},
    {"da", "だ"}, {"di", "ぢ"}, {"du", "づ"}, {"de", "で"}, {"do", "ど"},
    {"ba", "ば"}, {"bi", "び"}, {"bu", "ぶ"}, {"be", "べ"}, {"bo", "ぼ"},
    {"pa", "ぱ"}, {"pi", "ぴ"}, {"pu", "ぷ"}, {"pe", "ぺ"}, {"po", "ぽ"},
    {"kya", "きゃ"}, {"kyi", "きぃ"}, {"kyu", "きゅ"}, {"kye", "きぇ"}, {"kyo", "きょ"},
    {"sya", "しゃ"}, {"syi", "しぃ"}, {"syu", "しゅ"}, {"sye", "しぇ"}, {"syo", "しょ"},
    {"tya", "ちゃ"}, {"tyi", "ちぃ"}, {"tyu", "ちゅ"}, {"tye", "ちぇ"}, {"tyo", "ちょ"},
    {"nya", "にゃ"}, {"nyi", "にぃ"}, {"nyu", "にゅ"}, {"nye", "にぇ"}, {"nyo", "にょ"},
    {"hya", "ひゃ"}, {"hyi", "ひぃ"}, {"hyu", "ひゅ"}, {"hye", "ひぇ"}, {"hyo", "ひょ"},
    {"mya", "みゃ"}, {"myi", "みぃ"}, {"myu", "みゅ"}, {"mye", "みぇ"}, {"myo", "みょ"},
    {"rya", "りゃ"}, {"ryi", "りぃ"}, {"ryu", "りゅ"}, {"rye", "りぇ"}, {"ryo", "りょ"},
    {"gya", "ぎゃ"}, {"gyi", "ぎぃ"}, {"gyu", "ぎゅ"}, {"gye", "ぎぇ"}, {"gyo", "ぎょ"},
    {"zya", "じゃ"}, {"zyi", "じぃ"}, {"zyu", "じゅ"}, {"zye", "じぇ"}, {"zyo", "じょ"},
    {"dya", "ぢゃ"}, {"dyi", "ぢぃ"}, {"dyu", "ぢゅ"}, {"dye", "ぢぇ"}, {"dyo", "ぢょ"},
    {"bya", "びゃ"}, {"byi", "びぃ"}, {"byu", "びゅ"}, {"bye", "びぇ"}, {"byo", "びょ"},
    {"pya", "ぴゃ"}, {"pyi", "ぴぃ"}, {"pyu", "ぴゅ"}, {"pye", "ぴぇ"}, {"pyo", "ぴょ"},
    {"xa", "ぁ"}, {"xi", "ぃ"}, {"xu", "ぅ"}, {"xe", "ぇ"}, {"xo", "ぉ"}, {"xtu", "っ"},
    {"la", "ぁ"}, {"li", "ぃ"}, {"lu", "ぅ"}, {"le", "ぇ"}, {"lo", "ぉ"}, {"ltu", "っ"},
    {"dha", "でゃ"}, {"dhi", "でぃ"}, {"dhu", "でゅ"}, {"dhe", "でぇ"}, {"dho", "でょ"},
    {"sha", "しゃ"}, {"shi", "し"}, {"shu", "しゅ"}, {"she","しぇ"}, {"sho", "しょ"},
    {"cha", "ちゃ"}, {"chi", "ち"}, {"chu","ちゅ"}, {"che", "ちぇ"}, {"cho", "ちょ"},
    {"tha", "てゃ"}, {"thi", "てぃ"}, {"thu", "てゅ"}, {"the", "てぇ"}, {"tho", "てょ"},
    {"fa", "ふぁ"}, {"fi", "ふぃ"}, {"fu", "ふ"}, {"fe", "ふぇ"}, {"fo", "ふぉ"},
    {"fya", "ふゃ"}, {"fyu", "ふゅ"}, {"fyo", "ふょ"},
    {"ja", "じゃ"}, {"ji", "じ"}, {"ju", "じゅ"}, {"je", "じぇ"}, {"jo", "じょ"},
    {"jya", "じゃ"}, {"jyi", "じぃ"}, {"jyu", "じゅ"}, {"jye", "じぇ"}, {"jyo","じょ"},
    {"va", "ゔぁ"}, {"vi", "ゔぃ"}, {"vu", "ゔ"}, {"ve", "ゔぇ"}, {"vo", "ゔぉ"},
    {"0", "０"}, {"1", "１"}, {"2", "２"}, {"3", "３"}, {"4", "４"},
    {"5", "５"}, {"6", "６"}, {"7", "７"}, {"8", "８"}, {"9", "９"},
    {"!", "！"}, {"\"", "”"}, {"#", "＃"}, {"$", "＄"}, {"%", "％"}, {"'", "’"},
    {"(", "（"}, {")", "）"}, {"*", "＊"}, {"+", "＋"}, {",", "、"}, {"-", "ー"},
    {".", "。"}, {"/", "・"}, {":", "："}, {";", "；"}, {"<", "＜"}, {"=", "＝"},
    {">", "＞"}, {"?", "？"}, {"@", "＠"}, {"[", "「"}, {"\\", "￥"}, {"]", "」"},
    {"^", "＾"}, {"_", "＿"}, {"`", "｀"}, {"{", "｛"}, {"|", "｜"}, {"}", "｝"},
    {"~", "～"},
    {"zh", "←"}, {"zj", "↓"}, {"zk", "↑"}, {"zl", "→"},
  };

  alpha_map = new std::unordered_map<std::string, std::string>{
    {"a", "ａ"}, {"b", "ｂ"}, {"c", "ｃ"}, {"d", "ｄ"}, {"e", "ｅ"}, {"f", "ｆ"},
    {"g", "ｇ"}, {"h", "ｈ"}, {"i", "ｉ"}, {"j", "ｊ"}, {"k", "ｋ"}, {"l", "ｌ"},
    {"m", "ｍ"}, {"n", "ｎ"}, {"o", "ｏ"}, {"p", "ｐ"}, {"q", "ｑ"}, {"r", "ｒ"},
    {"s", "ｓ"}, {"t", "ｔ"}, {"u", "ｕ"}, {"v", "ｖ"}, {"w", "ｗ"}, {"x", "ｘ"},
    {"y", "ｙ"}, {"z", "ｚ"},
    {"A", "Ａ"}, {"B", "Ｂ"}, {"C", "Ｃ"}, {"D", "Ｄ"}, {"E", "Ｅ"}, {"F", "Ｆ"},
    {"G", "Ｇ"}, {"H", "Ｈ"}, {"I", "Ｉ"}, {"J", "Ｊ"}, {"K", "Ｋ"}, {"L", "Ｌ"},
    {"M", "Ｍ"}, {"N", "Ｎ"}, {"O", "Ｏ"}, {"P", "Ｐ"}, {"Q", "Ｑ"}, {"R", "Ｒ"},
    {"S", "Ｓ"}, {"T", "Ｔ"}, {"U", "Ｕ"}, {"V", "Ｖ"}, {"W", "Ｗ"}, {"X", "Ｘ"},
    {"Y", "Ｙ"}, {"Z", "Ｚ"},
    {"0", "０"}, {"1", "１"}, {"2", "２"}, {"3", "３"}, {"4", "４"},
    {"5", "５"}, {"6", "６"}, {"7", "７"}, {"8", "８"}, {"9", "９"},
    {"!", "！"}, {"\"", "”"}, {"#", "＃"}, {"$", "＄"}, {"%", "％"}, {"'", "’"},
    {"(", "（"}, {")", "）"}, {"*", "＊"}, {"+", "＋"}, {",", ","}, {"-", "－"},
    {".", "."}, {"/", "／"}, {":", "："}, {";", "；"}, {"<", "＜"}, {"=", "＝"},
    {">", "＞"}, {"?", "？"}, {"@", "＠"}, {"[", "［"}, {"\\", "￥"}, {"]", "］"},
    {"^", "＾"}, {"_", "＿"}, {"`", "｀"}, {"{", "｛"}, {"|", "｜"}, {"}", "｝"},
    {"~", "～"},
  };

  full_katakana_map = new std::unordered_map<std::string, std::string>{
    {"あ", "ア"}, {"い", "イ"}, {"う", "ウ"}, {"え", "エ"}, {"お", "オ"},
    {"か", "カ"}, {"き", "キ"}, {"く", "ク"}, {"け", "ケ"}, {"こ", "コ"},
    {"さ", "サ"}, {"し", "シ"}, {"す", "ス"}, {"せ", "セ"}, {"そ", "ソ"},
    {"た", "タ"}, {"ち", "チ"}, {"つ", "ツ"}, {"て", "テ"}, {"と", "ト"},
    {"な", "ナ"}, {"に", "ニ"}, {"ぬ", "ヌ"}, {"ね", "ネ"}, {"の", "ノ"},
    {"は", "ハ"}, {"ひ", "ヒ"}, {"ふ", "フ"}, {"へ", "ヘ"}, {"ほ", "ホ"},
    {"ま", "マ"}, {"み", "ミ"}, {"む", "ム"}, {"め", "メ"}, {"も", "モ"},
    {"や", "ヤ"}, {"ゆ", "ユ"}, {"よ", "ヨ"},
    {"ら", "ラ"}, {"り", "リ"}, {"る", "ル"}, {"れ", "レ"}, {"ろ", "ロ"},
    {"わ", "ワ"}, {"ゐ", "ヰ"}, {"ゑ", "ヱ"}, {"を", "ヲ"}, {"ん", "ン"},
    {"が", "ガ"}, {"ぎ", "ギ"}, {"ぐ", "グ"}, {"げ", "ゲ"}, {"ご", "ゴ"},
    {"ざ", "ザ"}, {"じ", "ジ"}, {"ず", "ズ"}, {"ぜ", "ゼ"}, {"ぞ", "ゾ"},
    {"だ", "ダ"}, {"ぢ", "ヂ"}, {"づ", "ヅ"}, {"で", "デ"}, {"ど", "ド"},
    {"ば", "バ"}, {"び", "ビ"}, {"ぶ", "ブ"}, {"べ", "ベ"}, {"ぼ", "ボ"},
    {"ぱ", "パ"}, {"ぴ", "ピ"}, {"ぷ", "プ"}, {"ぺ", "ペ"}, {"ぽ", "ポ"},
    {"ゔ", "ヴ"}, {"ゃ", "ャ"}, {"ゅ", "ュ"}, {"ょ", "ョ"}, {"っ", "ッ"},
    {"ぁ", "ァ"}, {"ぃ", "ィ"}, {"ぅ", "ゥ"}, {"ぇ", "ェ"}, {"ぉ", "ォ"},
  };

  half_katakana_map = new std::unordered_map<std::string, std::string>{
    {"ア", "ｱ"}, {"イ", "ｲ"}, {"ウ", "ｳ"}, {"エ", "ｴ"}, {"オ", "ｵ"},
    {"カ", "ｶ"}, {"キ", "ｷ"}, {"ク", "ｸ"}, {"ケ", "ｹ"}, {"コ", "ｺ"},
    {"サ", "ｻ"}, {"シ", "ｼ"}, {"ス", "ｽ"}, {"セ", "ｾ"}, {"ソ", "ｿ"},
    {"タ", "ﾀ"}, {"チ", "ﾁ"}, {"ツ", "ﾂ"}, {"テ", "ﾃ"}, {"ト", "ﾄ"},
    {"ナ", "ﾅ"}, {"ニ", "ﾆ"}, {"ヌ", "ﾇ"}, {"ネ", "ﾈ"}, {"ノ", "ﾉ"},
    {"ハ", "ﾊ"}, {"ヒ", "ﾋ"}, {"フ", "ﾌ"}, {"ヘ", "ﾍ"}, {"ホ", "ﾎ"},
    {"マ", "ﾏ"}, {"ミ", "ﾐ"}, {"ム", "ﾑ"}, {"メ", "ﾒ"}, {"モ", "ﾓ"},
    {"ヤ", "ﾔ"}, {"ユ", "ﾕ"}, {"ヨ", "ﾖ"},
    {"ラ", "ﾗ"}, {"リ", "ﾘ"}, {"ル", "ﾙ"}, {"レ", "ﾚ"}, {"ロ", "ﾛ"},
    {"ワ", "ﾜ"}, {"ヰ", "ｲ"}, {"ヱ", "ｴ"}, {"ヲ", "ｦ"}, {"ン", "ﾝ"},
    {"ガ", "ｶﾞ"}, {"ギ", "ｷﾞ"}, {"グ", "ｸﾞ"}, {"ゲ", "ｹﾞ"}, {"ゴ", "ｺﾞ"},
    {"ザ", "ｻﾞ"}, {"ジ", "ｼﾞ"}, {"ズ", "ｽﾞ"}, {"ゼ", "ｾﾞ"}, {"ゾ", "ｿﾞ"},
    {"ダ", "ﾀﾞ"}, {"ヂ", "ﾁﾞ"}, {"ヅ", "ﾂﾞ"}, {"デ", "ﾃﾞ"}, {"ド", "ﾄﾞ"},
    {"バ", "ﾊﾞ"}, {"ビ", "ﾋﾞ"}, {"ブ", "ﾌﾞ"}, {"ベ", "ﾍﾞ"}, {"ボ", "ﾎﾞ"},
    {"パ", "ﾊﾟ"}, {"ピ", "ﾋﾟ"}, {"プ", "ﾌﾟ"}, {"ペ", "ﾍﾟ"}, {"ポ", "ﾎﾟ"},
    {"ヴ", "ｳﾞ"}, {"ャ", "ｬ"}, {"ュ", "ｭ"}, {"ョ", "ｮ"}, {"ッ", "ｯ"},
    {"ァ", "ｧ"}, {"ィ", "ｨ"}, {"ゥ", "ｩ"}, {"ェ", "ｪ"}, {"ォ", "ｫ"},
    {"０", "0"}, {"１", "1"}, {"２", "2"}, {"３", "3"}, {"４", "4"},
    {"５", "5"}, {"６", "6"}, {"７", "7"}, {"８", "8"}, {"９", "9"},
    {"！", "!"}, {"”", "\""}, {"＃", "#"}, {"＄", "$"}, {"％", "%"}, {"’", "'"},
    {"（", "("}, {"）", ")"}, {"＊", "*"}, {"＋", "+"}, {"、", "､"}, {"ー", "ｰ"},
    {"。", "｡"}, {"・", "･"}, {"：", ":"}, {"；", ";"}, {"＜", "<"}, {"＝", "="},
    {"＞", ">"}, {"？", "?"}, {"＠", "@"}, {"「", "｢"}, {"￥", "\\"}, {"」", "｣"},
    {"＾", "^"}, {"＿", "_"}, {"｀", "`"}, {"｛", "{"}, {"｜", "|"}, {"｝", "}"},
    {"～", "~"}, {"－", "-"}, {"．", "."}, {"，", ","},
    {"ａ", "a"}, {"ｂ", "b"}, {"ｃ", "c"}, {"ｄ", "d"}, {"ｅ", "e"}, {"ｆ", "f"},
    {"ｇ", "g"}, {"ｈ", "h"}, {"ｉ", "i"}, {"ｊ", "j"}, {"ｋ", "k"}, {"ｌ", "l"},
    {"ｍ", "m"}, {"ｎ", "n"}, {"ｏ", "o"}, {"ｐ", "p"}, {"ｑ", "q"}, {"ｒ", "r"},
    {"ｓ", "s"}, {"ｔ", "t"}, {"ｕ", "u"}, {"ｖ", "v"}, {"ｗ", "w"}, {"ｘ", "x"},
    {"ｙ", "y"}, {"ｚ", "z"},
    {"Ａ", "A"}, {"Ｂ", "B"}, {"Ｃ", "C"}, {"Ｄ", "D"}, {"Ｅ", "E"}, {"Ｆ", "F"},
    {"Ｇ", "G"}, {"Ｈ", "H"}, {"Ｉ", "I"}, {"Ｊ", "J"}, {"Ｋ", "K"}, {"Ｌ", "L"},
    {"Ｍ", "M"}, {"Ｎ", "N"}, {"Ｏ", "O"}, {"Ｐ", "P"}, {"Ｑ", "Q"}, {"Ｒ", "R"},
    {"Ｓ", "S"}, {"Ｔ", "T"}, {"Ｕ", "U"}, {"Ｖ", "V"}, {"Ｗ", "W"}, {"Ｘ", "X"},
    {"Ｙ", "Y"}, {"Ｚ", "Z"},
  };
}

void TaskIME(uint64_t task_id, int64_t data) {
  __asm__("cli");
  Task& task = task_manager->CurrentTask();
  __asm__("sti");

  while (true) {
    __asm__("cli");
    auto msg = task.ReceiveMessage();
    if (!msg) {
      task.Sleep();
      __asm__("sti");
      continue;
    }
    __asm__("sti");

    ime->ProcessMessage(*msg);
  }
}
