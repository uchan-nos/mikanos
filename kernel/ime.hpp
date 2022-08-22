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
    void Draw();
    void Convert();
    bool SendChars(const std::string& str);

    std::shared_ptr<ToplevelWindow> window;
    unsigned int layer_id;
    bool window_shown;

    std::string current_conversion;
    std::string chars_entered;
    std::string converted_hiragana;
};

extern IME* ime;

void InitializeIME();
void TaskIME(uint64_t task_id, int64_t data);
