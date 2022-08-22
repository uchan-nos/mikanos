/**
 * @file ime.cpp
 *
 * IMEを実装したファイル．
 */

#include "ime.hpp"

#include <cctype>
#include <unordered_map>

#include "font.hpp"
#include "graphics.hpp"
#include "layer.hpp"
#include "task.hpp"

static const std::unordered_map<std::string, std::string>* hiragana_map;

IME::IME() {
  window = std::make_shared<ToplevelWindow>(
      200, 52, screen_config.pixel_format, "IME");

  layer_id = layer_manager->NewLayer()
    .SetWindow(window)
    .SetDraggable(true)
    .Move({580, 480})
    .ID();

  layer_manager->UpDown(layer_id, -1);
  window_shown = false;
}

void IME::ShowWindow(bool show) {
  __asm__("cli");
  if (show) {
    layer_manager->UpDown(layer_id, 0);
    auto active_id = active_layer->GetActive();
    active_layer->Activate(layer_id);
    active_layer->Activate(active_id);
  } else {
    Layer* layer = layer_manager->FindLayer(layer_id);
    if (active_layer->GetActive() == layer_id) {
      active_layer->Activate(0);
    }
    layer_manager->Hide(layer_id);
    if (layer != nullptr) {
      const auto pos = layer->GetPosition();
      const auto size = layer->GetWindow()->Size();
      layer_manager->Draw({pos, size});
    }
  }
  __asm__("sti");
  window_shown = show;
  Draw();
}

void IME::ResetInput() {
  chars_entered.clear();
  Convert();
  Draw();
}

void IME::ProcessMessage(const Message& msg) {
  if(msg.type == Message::kKeyPush) {
    const auto& info = msg.arg.keyboard;
    if (info.press) {
      if (info.ascii == '\n') {
        // 決定
        if (SendChars(current_conversion)) {
          chars_entered.clear();
          Convert();
          Draw();
        }
      }if (info.ascii == '\b') {
        // バックスペース
        if (!chars_entered.empty()) {
          chars_entered.erase(chars_entered.end() - 1);
          Convert();
          Draw();
        }
      } else if (info.ascii == ' ') {
        if (chars_entered.empty()) {
          // 全角スペースを入力
          SendChars("　");
        } else {
          // 変換
        }
      } else if (info.ascii > ' ') {
        // 文字入力
        chars_entered.push_back(info.ascii);
        Convert();
        Draw();
      }
    }
  }
}

bool IME::IsEmpty() {
  return chars_entered.empty();
}

void IME::Draw() {
  if (!window_shown) return;
  std::string string_to_draw;
  int draw_right = 4;
  for (size_t i = 0; i < current_conversion.size(); ) {
    auto [c, size] = ConvertUTF8To32(&current_conversion[i]);
    if (size <= 0) break;
    draw_right += 8 * (IsHankaku(c) ? 1 : 2);
    if (draw_right > window->InnerSize().x) break;
    string_to_draw.append(current_conversion.begin() + i,
                          current_conversion.begin() + i + size);
    i += size;
  }
  FillRectangle(*window->InnerWriter(), {0, 0}, window->InnerSize(), {255, 255, 255});
  WriteString(*window->InnerWriter(), {4, 4}, string_to_draw.c_str(), {0, 0, 0});
  layer_manager->Draw(layer_id);
}

void IME::Convert() {
  converted_hiragana.clear();
  bool prev_is_number = false;
  for (size_t i = 0; i < chars_entered.size(); ){
    std::string res;
    int res_input_length = 0;
    if (prev_is_number) {
      if (chars_entered[i] == '.') res = "．";
      else if (chars_entered[i] == ',') res = "，";
      else if (chars_entered[i] == '-') res = "－";
      if (!res.empty()) {
        converted_hiragana.append(res);
        ++i;
        prev_is_number = false;
        continue;
      }
    }
    prev_is_number = isdigit(chars_entered[i]);
    for (int j = 3; j >= 1; j--) {
      auto search_result = hiragana_map->find(chars_entered.substr(i, j));
      if (search_result != hiragana_map->end()) {
        res = search_result->second;
        res_input_length = j;
        break;
      }
    }
    if (res_input_length > 0) {
      converted_hiragana.append(res);
      i += res_input_length;
    } else if (i + 2 <= chars_entered.size() &&
               chars_entered[i] == chars_entered[i + 1] &&
               !(chars_entered[i] == 'a' || chars_entered[i] == 'i' ||
                 chars_entered[i] == 'u' || chars_entered[i] == 'e' ||
                 chars_entered[i] == 'o')) {
      converted_hiragana.append("っ");
      ++i;
    } else {
      converted_hiragana.push_back(chars_entered[i]);
      ++i;
    }
  }
  current_conversion = converted_hiragana;
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
