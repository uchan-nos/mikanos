#include "terminal.hpp"

#include <cstring>
#include <limits>

#include "font.hpp"
#include "layer.hpp"
#include "pci.hpp"
#include "asmfunc.h"
#include "elf.hpp"
#include "memory_manager.hpp"
#include "paging.hpp"

namespace {

std::vector<char*> MakeArgVector(char* command, char* first_arg) {
  std::vector<char*> argv;
  argv.push_back(command);
  if (!first_arg) {
    return argv;
  }

  char* p = first_arg;
  while (true) {
    while (isspace(p[0])) {
      ++p;
    }
    if (p[0] == 0) {
      break;
    }
    argv.push_back(p);

    while (p[0] != 0 && !isspace(p[0])) {
      ++p;
    }
    if (p[0] == 0) {
      break;
    }
    p[0] = 0;
    ++p;
  }

  return argv;
}

// #@@range_begin(get_phdr)
Elf64_Phdr* GetProgramHeader(Elf64_Ehdr* ehdr) {
  return reinterpret_cast<Elf64_Phdr*>(
      reinterpret_cast<uintptr_t>(ehdr) + ehdr->e_phoff);
}
// #@@range_end(get_phdr)

// #@@range_begin(get_first_addr)
uintptr_t GetFirstLoadAddress(Elf64_Ehdr* ehdr) {
  auto phdr = GetProgramHeader(ehdr);
  for (int i = 0; i < ehdr->e_phnum; ++i) {
    if (phdr[i].p_type != PT_LOAD) continue;
    return phdr[i].p_vaddr;
  }
  return 0;
}
// #@@range_end(get_first_addr)

static_assert(kBytesPerFrame >= 4096);

// #@@range_begin(new_pagemap)
WithError<PageMapEntry*> NewPageMap() {
  auto frame = memory_manager->Allocate(1);
  if (frame.error) {
    return { nullptr, frame.error };
  }

  auto e = reinterpret_cast<PageMapEntry*>(frame.value.Frame());
  memset(e, 0, sizeof(uint64_t) * 512);
  return { e, MAKE_ERROR(Error::kSuccess) };
}
// #@@range_end(new_pagemap)

// #@@range_begin(set_newpagemap)
WithError<PageMapEntry*> SetNewPageMapIfNotPresent(PageMapEntry& entry) {
  if (entry.bits.present) {
    return { entry.Pointer(), MAKE_ERROR(Error::kSuccess) };
  }

  auto [ child_map, err ] = NewPageMap();
  if (err) {
    return { nullptr, err };
  }

  entry.SetPointer(child_map);
  entry.bits.present = 1;

  return { child_map, MAKE_ERROR(Error::kSuccess) };
}
// #@@range_end(set_newpagemap)

// #@@range_begin(setup_pagemap)
WithError<size_t> SetupPageMap(
    PageMapEntry* page_map, int page_map_level, LinearAddress4Level addr, size_t num_4kpages) {
  while (num_4kpages > 0) {
    const auto entry_index = addr.Part(page_map_level);

    auto [ child_map, err ] = SetNewPageMapIfNotPresent(page_map[entry_index]);
    if (err) {
      return { num_4kpages, err };
    }
    page_map[entry_index].bits.writable = 1;

    if (page_map_level == 1) {
      --num_4kpages;
    } else {
      auto [ num_remain_pages, err ] =
        SetupPageMap(child_map, page_map_level - 1, addr, num_4kpages);
      if (err) {
        return { num_4kpages, err };
      }
      num_4kpages = num_remain_pages;
    }

    if (entry_index == 511) {
      break;
    }

    addr.SetPart(page_map_level, entry_index + 1);
    for (int level = page_map_level - 1; level >= 1; --level) {
      addr.SetPart(level, 0);
    }
  }

  return { num_4kpages, MAKE_ERROR(Error::kSuccess) };
}
// #@@range_end(setup_pagemap)

// #@@range_begin(setup_pagemaps)
Error SetupPageMaps(LinearAddress4Level addr, size_t num_4kpages) {
  auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
  return SetupPageMap(pml4_table, 4, addr, num_4kpages).error;
}
// #@@range_end(setup_pagemaps)

// #@@range_begin(copy_loadsegms)
Error CopyLoadSegments(Elf64_Ehdr* ehdr) {
  auto phdr = GetProgramHeader(ehdr);
  for (int i = 0; i < ehdr->e_phnum; ++i) {
    if (phdr[i].p_type != PT_LOAD) continue;

    LinearAddress4Level dest_addr;
    dest_addr.value = phdr[i].p_vaddr;
    const auto num_4kpages = (phdr[i].p_memsz + 4095) / 4096;

    if (auto err = SetupPageMaps(dest_addr, num_4kpages)) {
      return err;
    }

    const auto src = reinterpret_cast<uint8_t*>(ehdr) + phdr[i].p_offset;
    const auto dst = reinterpret_cast<uint8_t*>(phdr[i].p_vaddr);
    memcpy(dst, src, phdr[i].p_filesz);
    memset(dst + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
  }
  return MAKE_ERROR(Error::kSuccess);
}
// #@@range_end(copy_loadsegms)

// #@@range_begin(load_elf)
Error LoadELF(Elf64_Ehdr* ehdr) {
  if (ehdr->e_type != ET_EXEC) {
    return MAKE_ERROR(Error::kInvalidFormat);
  }

  const auto addr_first = GetFirstLoadAddress(ehdr);
  if (addr_first < 0xffff'8000'0000'0000) {
    return MAKE_ERROR(Error::kInvalidFormat);
  }

  if (auto err = CopyLoadSegments(ehdr)) {
    return err;
  }

  return MAKE_ERROR(Error::kSuccess);
}
// #@@range_end(load_elf)

// #@@range_begin(clean_pagemap)
Error CleanPageMap(PageMapEntry* page_map, int page_map_level) {
  for (int i = 0; i < 512; ++i) {
    auto entry = page_map[i];
    if (!entry.bits.present) {
      continue;
    }

    if (page_map_level > 1) {
      if (auto err = CleanPageMap(entry.Pointer(), page_map_level - 1)) {
        return err;
      }
    }

    const auto entry_addr = reinterpret_cast<uintptr_t>(entry.Pointer());
    const FrameID map_frame{entry_addr / kBytesPerFrame};
    if (auto err = memory_manager->Free(map_frame, 1)) {
      return err;
    }
    page_map[i].data = 0;
  }

  return MAKE_ERROR(Error::kSuccess);
}
// #@@range_end(clean_pagemap)

// #@@range_begin(clean_pagemaps)
Error CleanPageMaps(LinearAddress4Level addr) {
  auto pml4_table = reinterpret_cast<PageMapEntry*>(GetCR3());
  auto pdp_table = pml4_table[addr.parts.pml4].Pointer();
  pml4_table[addr.parts.pml4].data = 0;
  if (auto err = CleanPageMap(pdp_table, 3)) {
    return err;
  }

  const auto pdp_addr = reinterpret_cast<uintptr_t>(pdp_table);
  const FrameID pdp_frame{pdp_addr / kBytesPerFrame};
  return memory_manager->Free(pdp_frame, 1);
}
// #@@range_end(clean_pagemaps)

} // namespace

Terminal::Terminal() {
  window_ = std::make_shared<ToplevelWindow>(
      kColumns * 8 + 8 + ToplevelWindow::kMarginX,
      kRows * 16 + 8 + ToplevelWindow::kMarginY,
      screen_config.pixel_format,
      "MikanTerm");
  DrawTerminal(*window_->InnerWriter(), {0, 0}, window_->InnerSize());

  layer_id_ = layer_manager->NewLayer()
    .SetWindow(window_)
    .SetDraggable(true)
    .ID();

  Print(">");
  cmd_history_.resize(8);
}

Rectangle<int> Terminal::BlinkCursor() {
  cursor_visible_ = !cursor_visible_;
  DrawCursor(cursor_visible_);

  return {CalcCursorPos(), {7, 15}};
}

void Terminal::DrawCursor(bool visible) {
  const auto color = visible ? ToColor(0xffffff) : ToColor(0);
  FillRectangle(*window_->Writer(), CalcCursorPos(), {7, 15}, color);
}

Vector2D<int> Terminal::CalcCursorPos() const {
  return ToplevelWindow::kTopLeftMargin +
      Vector2D<int>{4 + 8 * cursor_.x, 4 + 16 * cursor_.y};
}

Rectangle<int> Terminal::InputKey(
    uint8_t modifier, uint8_t keycode, char ascii) {
  DrawCursor(false);

  Rectangle<int> draw_area{CalcCursorPos(), {8*2, 16}};

  if (ascii == '\n') {
    linebuf_[linebuf_index_] = 0;
    if (linebuf_index_ > 0) {
      cmd_history_.pop_back();
      cmd_history_.push_front(linebuf_);
    }
    linebuf_index_ = 0;
    cmd_history_index_ = -1;

    cursor_.x = 0;
    if (cursor_.y < kRows - 1) {
      ++cursor_.y;
    } else {
      Scroll1();
    }
    ExecuteLine();
    Print(">");
    draw_area.pos = ToplevelWindow::kTopLeftMargin;
    draw_area.size = window_->InnerSize();
  } else if (ascii == '\b') {
    if (cursor_.x > 0) {
      --cursor_.x;
      FillRectangle(*window_->Writer(), CalcCursorPos(), {8, 16}, {0, 0, 0});
      draw_area.pos = CalcCursorPos();

      if (linebuf_index_ > 0) {
        --linebuf_index_;
      }
    }
  } else if (ascii != 0) {
    if (cursor_.x < kColumns - 1 && linebuf_index_ < kLineMax - 1) {
      linebuf_[linebuf_index_] = ascii;
      ++linebuf_index_;
      WriteAscii(*window_->Writer(), CalcCursorPos(), ascii, {255, 255, 255});
      ++cursor_.x;
    }
  } else if (keycode == 0x51) { // down arrow
    draw_area = HistoryUpDown(-1);
  } else if (keycode == 0x52) { // up arrow
    draw_area = HistoryUpDown(1);
  }

  DrawCursor(true);

  return draw_area;
}

void Terminal::Scroll1() {
  Rectangle<int> move_src{
    ToplevelWindow::kTopLeftMargin + Vector2D<int>{4, 4 + 16},
    {8*kColumns, 16*(kRows - 1)}
  };
  window_->Move(ToplevelWindow::kTopLeftMargin + Vector2D<int>{4, 4}, move_src);
  FillRectangle(*window_->InnerWriter(),
                {4, 4 + 16*cursor_.y}, {8*kColumns, 16}, {0, 0, 0});
}

void Terminal::ExecuteLine() {
  char* command = &linebuf_[0];
  char* first_arg = strchr(&linebuf_[0], ' ');
  if (first_arg) {
    *first_arg = 0;
    ++first_arg;
  }

  if (strcmp(command, "echo") == 0) {
    if (first_arg) {
      Print(first_arg);
    }
    Print("\n");
  } else if (strcmp(command, "clear") == 0) {
    FillRectangle(*window_->InnerWriter(),
                  {4, 4}, {8*kColumns, 16*kRows}, {0, 0, 0});
    cursor_.y = 0;
  } else if (strcmp(command, "lspci") == 0) {
    char s[64];
    for (int i = 0; i < pci::num_device; ++i) {
      const auto& dev = pci::devices[i];
      auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
      sprintf(s, "%02x:%02x.%d vend=%04x head=%02x class=%02x.%02x.%02x\n",
          dev.bus, dev.device, dev.function, vendor_id, dev.header_type,
          dev.class_code.base, dev.class_code.sub, dev.class_code.interface);
      Print(s);
    }
  } else if (strcmp(command, "ls") == 0) {
    auto root_dir_entries = fat::GetSectorByCluster<fat::DirectoryEntry>(
        fat::boot_volume_image->root_cluster);
    auto entries_per_cluster =
      fat::bytes_per_cluster / sizeof(fat::DirectoryEntry);
    char base[9], ext[4];
    char s[64];
    for (int i = 0; i < entries_per_cluster; ++i) {
      ReadName(root_dir_entries[i], base, ext);
      if (base[0] == 0x00) {
        break;
      } else if (static_cast<uint8_t>(base[0]) == 0xe5) {
        continue;
      } else if (root_dir_entries[i].attr == fat::Attribute::kLongName) {
        continue;
      }

      if (ext[0]) {
        sprintf(s, "%s.%s\n", base, ext);
      } else {
        sprintf(s, "%s\n", base);
      }
      Print(s);
    }
  } else if (strcmp(command, "cat") == 0) {
    char s[64];

    auto file_entry = fat::FindFile(first_arg);
    if (!file_entry) {
      sprintf(s, "no such file: %s\n", first_arg);
      Print(s);
    } else {
      auto cluster = file_entry->FirstCluster();
      auto remain_bytes = file_entry->file_size;

      DrawCursor(false);
      while (cluster != 0 && cluster != fat::kEndOfClusterchain) {
        char* p = fat::GetSectorByCluster<char>(cluster);

        int i = 0;
        for (; i < fat::bytes_per_cluster && i < remain_bytes; ++i) {
          Print(*p);
          ++p;
        }
        remain_bytes -= i;
        cluster = fat::NextCluster(cluster);
      }
      DrawCursor(true);
    }
  } else if (command[0] != 0) {
    auto file_entry = fat::FindFile(command);
    if (!file_entry) {
      Print("no such command: ");
      Print(command);
      Print("\n");
    } else if (auto err = ExecuteFile(*file_entry, command, first_arg)) {
      Print("failed to exec file: ");
      Print(err.Name());
      Print("\n");
    }
  }
}

Error Terminal::ExecuteFile(const fat::DirectoryEntry& file_entry, char* command, char* first_arg) {
  // #@@range_begin(load_file)
  std::vector<uint8_t> file_buf(file_entry.file_size);
  fat::LoadFile(&file_buf[0], file_buf.size(), file_entry);
  // #@@range_end(load_file)

  auto elf_header = reinterpret_cast<Elf64_Ehdr*>(&file_buf[0]);
  if (memcmp(elf_header->e_ident, "\x7f" "ELF", 4) != 0) {
    using Func = void ();
    auto f = reinterpret_cast<Func*>(&file_buf[0]);
    f();
    return MAKE_ERROR(Error::kSuccess);
  }

  // #@@range_begin(load_app)
  auto argv = MakeArgVector(command, first_arg);
  if (auto err = LoadELF(elf_header)) {
    return err;
  }

  auto entry_addr = elf_header->e_entry;
  using Func = int (int, char**);
  auto f = reinterpret_cast<Func*>(entry_addr);
  auto ret = f(argv.size(), &argv[0]);

  char s[64];
  sprintf(s, "app exited. ret = %d\n", ret);
  Print(s);

  const auto addr_first = GetFirstLoadAddress(elf_header);
  if (auto err = CleanPageMaps(LinearAddress4Level{addr_first})) {
    return err;
  }
  // #@@range_end(load_app)

  return MAKE_ERROR(Error::kSuccess);
}

void Terminal::Print(char c) {
  auto newline = [this]() {
    cursor_.x = 0;
    if (cursor_.y < kRows - 1) {
      ++cursor_.y;
    } else {
      Scroll1();
    }
  };

  if (c == '\n') {
    newline();
  } else {
    WriteAscii(*window_->Writer(), CalcCursorPos(), c, {255, 255, 255});
    if (cursor_.x == kColumns - 1) {
      newline();
    } else {
      ++cursor_.x;
    }
  }
}

void Terminal::Print(const char* s) {
  DrawCursor(false);

  while (*s) {
    Print(*s);
    ++s;
  }

  DrawCursor(true);
}

Rectangle<int> Terminal::HistoryUpDown(int direction) {
  if (direction == -1 && cmd_history_index_ >= 0) {
    --cmd_history_index_;
  } else if (direction == 1 && cmd_history_index_ + 1 < cmd_history_.size()) {
    ++cmd_history_index_;
  }

  cursor_.x = 1;
  const auto first_pos = CalcCursorPos();

  Rectangle<int> draw_area{first_pos, {8*(kColumns - 1), 16}};
  FillRectangle(*window_->Writer(), draw_area.pos, draw_area.size, {0, 0, 0});

  const char* history = "";
  if (cmd_history_index_ >= 0) {
    history = &cmd_history_[cmd_history_index_][0];
  }

  strcpy(&linebuf_[0], history);
  linebuf_index_ = strlen(history);

  WriteString(*window_->Writer(), first_pos, history, {255, 255, 255});
  cursor_.x = linebuf_index_ + 1;
  return draw_area;
}

void TaskTerminal(uint64_t task_id, int64_t data) {
  __asm__("cli");
  Task& task = task_manager->CurrentTask();
  Terminal* terminal = new Terminal;
  layer_manager->Move(terminal->LayerID(), {100, 200});
  active_layer->Activate(terminal->LayerID());
  layer_task_map->insert(std::make_pair(terminal->LayerID(), task_id));
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

    switch (msg->type) {
    case Message::kTimerTimeout:
      {
        const auto area = terminal->BlinkCursor();
        Message msg = MakeLayerMessage(
            task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
        __asm__("cli");
        task_manager->SendMessage(1, msg);
        __asm__("sti");
      }
      break;
    case Message::kKeyPush:
      {
        const auto area = terminal->InputKey(msg->arg.keyboard.modifier,
                                             msg->arg.keyboard.keycode,
                                             msg->arg.keyboard.ascii);
        Message msg = MakeLayerMessage(
            task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
        __asm__("cli");
        task_manager->SendMessage(1, msg);
        __asm__("sti");
      }
      break;
    default:
      break;
    }
  }
}
