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

static const std::unordered_map<char, std::string>* alpha_map;

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
  hiragana.clear();
  Draw();
}

void IME::ProcessMessage(const Message& msg) {
  if(msg.type == Message::kKeyPush) {
    const auto& info = msg.arg.keyboard;
    if (info.press) {
      if (info.ascii == '\n') {
        // 決定
        std::string str;
        for (size_t i = 0; i < hiragana.size(); ++i) {
          str += hiragana[i].converted;
        }
        if (SendChars(str)) {
          hiragana.clear();
          Draw();
        }
      }if (info.ascii == '\b') {
        // バックスペース
        if (!hiragana.empty()) {
          hiragana.pop_back();
          Draw();
        }
      } else if (info.ascii == ' ') {
        if (hiragana.empty()) {
          // 全角スペースを入力
          SendChars("　");
        } else {
          // 変換
        }
      } else if (info.ascii > ' ') {
        // 文字入力
        AppendChar(info.ascii);
        Draw();
      }
    }
  }
}

bool IME::IsEmpty() {
  return hiragana.empty();
}

void IME::Draw() {
  if (!window_shown) return;
  std::string string_to_draw;
  int draw_right = 4;
  for (size_t i = 0; i < hiragana.size(); ++i) {
    for (size_t j = 0; j < hiragana[i].converted.size(); ) {
      auto [c, size] = ConvertUTF8To32(&hiragana[i].converted[j]);
      if (size <= 0) break;
      draw_right += 8 * (IsHankaku(c) ? 1 : 2);
      if (draw_right > window->InnerSize().x) {
        i = hiragana.size();
        break;
      }
      string_to_draw.append(hiragana[i].converted.begin() + j,
                            hiragana[i].converted.begin() + j + size);
      j += size;
    }
  }
  FillRectangle(*window->InnerWriter(), {0, 0}, window->InnerSize(), {255, 255, 255});
  WriteString(*window->InnerWriter(), {4, 4}, string_to_draw.c_str(), {0, 0, 0});
  layer_manager->Draw(layer_id);
}

void IME::AppendChar(char c) {
  // 数字の後だと変化する記号を処理する
  if (!hiragana.empty() && isdigit(hiragana.back().source[0])) {
    std::string res = "";
    if (c == '.') res = "．";
    else if (c == ',') res = "，";
    else if (c == '-') res = "－";
    if (!res.empty()) {
      hiragana.push_back({res, std::string(1, c), false});
      return;
    }
  }

  // ローマ字からの変換を処理する (数字・記号を含む)
  auto [source, converted] = [&]() -> std::tuple<std::string, std::string> {
    if (!hiragana.empty() && hiragana.back().is_alpha) {
      if (hiragana.size() >= 2 && hiragana[hiragana.size() - 2].is_alpha) {
        // 3文字からの変換を試す
        std::string key;
        key.push_back(tolower(hiragana[hiragana.size() - 2].source[0]));
        key.push_back(tolower(hiragana.back().source[0]));
        key.push_back(tolower(c));
        auto it = hiragana_map->find(key);
        if (it != hiragana_map->end()) {
          std::string source = hiragana[hiragana.size() - 2].source +
                               hiragana.back().source + c;
          hiragana.erase(hiragana.end() - 2, hiragana.end());
          return {source, it->second};
        }
      }
      // 2文字からの変換を試す
      std::string key;
      key.push_back(tolower(hiragana.back().source[0]));
      key.push_back(tolower(c));
      auto it = hiragana_map->find(key);
      if (it != hiragana_map->end()) {
        std::string source = hiragana.back().source + c;
        hiragana.pop_back();
        return {source, it->second};
      }
    }
    // 1文字からの変換を試す
    std::string key(1, c);
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
      hiragana.push_back({converted.substr(0, first_character_size),
                          source.substr(0, 1), false});
      hiragana.push_back({converted.substr(first_character_size),
                          source.substr(1), false});
    } else {
      // 1文字に変換する
      hiragana.push_back({converted, source, false});
    }
    return;
  }

  // 小さい「っ」の入力(同じ子音を重ねる)を処理する
  if (!hiragana.empty() && hiragana.back().is_alpha &&
      hiragana.back().source[0] == c &&
      c != 'a' && c != 'i' && c != 'u' && c != 'e' && c != 'o') {
    std::string source = hiragana.back().source;
    hiragana.pop_back();
    hiragana.push_back({"っ", source, false});
  }

  // 入力された文字(アルファベット)を置く
  auto it = alpha_map->find(c);
  if (it != alpha_map->end()) {
    hiragana.push_back({it->second, std::string(1, c), true});
  } else {
    hiragana.push_back({std::string(1, c), std::string(1, c), true});
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
  alpha_map = new std::unordered_map<char, std::string>{
    {'a', "ａ"}, {'b', "ｂ"}, {'c', "ｃ"}, {'d', "ｄ"}, {'e', "ｅ"}, {'f', "ｆ"},
    {'g', "ｇ"}, {'h', "ｈ"}, {'i', "ｉ"}, {'j', "ｊ"}, {'k', "ｋ"}, {'l', "ｌ"},
    {'m', "ｍ"}, {'n', "ｎ"}, {'o', "ｏ"}, {'p', "ｐ"}, {'q', "ｑ"}, {'r', "ｒ"},
    {'s', "ｓ"}, {'t', "ｔ"}, {'u', "ｕ"}, {'v', "ｖ"}, {'w', "ｗ"}, {'x', "ｘ"},
    {'y', "ｙ"}, {'z', "ｚ"},
    {'A', "Ａ"}, {'B', "Ｂ"}, {'C', "Ｃ"}, {'D', "Ｄ"}, {'E', "Ｅ"}, {'F', "Ｆ"},
    {'G', "Ｇ"}, {'H', "Ｈ"}, {'I', "Ｉ"}, {'J', "Ｊ"}, {'K', "Ｋ"}, {'L', "Ｌ"},
    {'M', "Ｍ"}, {'N', "Ｎ"}, {'O', "Ｏ"}, {'P', "Ｐ"}, {'Q', "Ｑ"}, {'R', "Ｒ"},
    {'S', "Ｓ"}, {'T', "Ｔ"}, {'U', "Ｕ"}, {'V', "Ｖ"}, {'W', "Ｗ"}, {'X', "Ｘ"},
    {'Y', "Ｙ"}, {'Z', "Ｚ"},
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
