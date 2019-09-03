#include "keyboard.hpp"

#include <memory>
#include "usb/classdriver/keyboard.hpp"

namespace {

// #@@range_begin(keycode_map)
const char keycode_map[256] = {
  0,    0,    0,    0,    'a',  'b',  'c',  'd', // 0
  'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l', // 8
  'm',  'n',  'o',  'p',  'q',  'r',  's',  't', // 16
  'u',  'v',  'w',  'x',  'y',  'z',  '1',  '2', // 24
  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0', // 32
  '\n', '\b', 0x08, '\t', ' ',  '-',  '=',  '[', // 40
  ']', '\\',  '#',  ';', '\'',  '`',  ',',  '.', // 48
  '/',  0,    0,    0,    0,    0,    0,    0,   // 56
  0,    0,    0,    0,    0,    0,    0,    0,   // 64
  0,    0,    0,    0,    0,    0,    0,    0,   // 72
  0,    0,    0,    0,    '/',  '*',  '-',  '+', // 80
  '\n', '1',  '2',  '3',  '4',  '5',  '6',  '7', // 88
  '8',  '9',  '0',  '.', '\\',  0,    0,    '=', // 96
};
// #@@range_end(keycode_map)

} // namespace

// #@@range_begin(initkb)
void InitializeKeyboard(std::deque<Message>& msg_queue) {
  usb::HIDKeyboardDriver::default_observer =
    [&msg_queue](uint8_t keycode) {
      Message msg{Message::kKeyPush};
      msg.arg.keyboard.keycode = keycode;
      msg.arg.keyboard.ascii = keycode_map[keycode];
      msg_queue.push_back(msg);
    };
}
// #@@range_end(initkb)
