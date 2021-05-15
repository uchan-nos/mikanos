#include "terminal.hpp"

#include <cstring>
#include <limits>
#include <vector>

#include "font.hpp"
#include "layer.hpp"
#include "pci.hpp"
#include "asmfunc.h"
#include "elf.hpp"
#include "memory_manager.hpp"
#include "paging.hpp"
#include "timer.hpp"
#include "keyboard.hpp"
#include "logger.hpp"
#include "uefi.hpp"
#include "usb/classdriver/cdc.hpp"
#include "usb/xhci/xhci.hpp"

namespace {

WithError<int> MakeArgVector(char* command, char* first_arg,
    char** argv, int argv_len, char* argbuf, int argbuf_len) {
  int argc = 0;
  int argbuf_index = 0;

  auto push_to_argv = [&](const char* s) {
    if (argc >= argv_len || argbuf_index >= argbuf_len) {
      return MAKE_ERROR(Error::kFull);
    }

    argv[argc] = &argbuf[argbuf_index];
    ++argc;
    strcpy(&argbuf[argbuf_index], s);
    argbuf_index += strlen(s) + 1;
    return MAKE_ERROR(Error::kSuccess);
  };

  if (auto err = push_to_argv(command)) {
    return { argc, err };
  }
  if (!first_arg) {
    return { argc, MAKE_ERROR(Error::kSuccess) };
  }

  char* p = first_arg;
  while (true) {
    while (isspace(p[0])) {
      ++p;
    }
    if (p[0] == 0) {
      break;
    }
    const char* arg = p;

    while (p[0] != 0 && !isspace(p[0])) {
      ++p;
    }
    // here: p[0] == 0 || isspace(p[0])
    const bool is_end = p[0] == 0;
    p[0] = 0;
    if (auto err = push_to_argv(arg)) {
      return { argc, err };
    }
    if (is_end) {
      break;
    }
    ++p;
  }

  return { argc, MAKE_ERROR(Error::kSuccess) };
}

Elf64_Phdr* GetProgramHeader(Elf64_Ehdr* ehdr) {
  return reinterpret_cast<Elf64_Phdr*>(
      reinterpret_cast<uintptr_t>(ehdr) + ehdr->e_phoff);
}

uintptr_t GetFirstLoadAddress(Elf64_Ehdr* ehdr) {
  auto phdr = GetProgramHeader(ehdr);
  for (int i = 0; i < ehdr->e_phnum; ++i) {
    if (phdr[i].p_type != PT_LOAD) continue;
    return phdr[i].p_vaddr;
  }
  return 0;
}

static_assert(kBytesPerFrame >= 4096);

WithError<uint64_t> CopyLoadSegments(Elf64_Ehdr* ehdr) {
  auto phdr = GetProgramHeader(ehdr);
  uint64_t last_addr = 0;
  for (int i = 0; i < ehdr->e_phnum; ++i) {
    if (phdr[i].p_type != PT_LOAD) continue;

    LinearAddress4Level dest_addr;
    dest_addr.value = phdr[i].p_vaddr;
    last_addr = std::max(last_addr, phdr[i].p_vaddr + phdr[i].p_memsz);
    const auto num_4kpages =
      ((phdr[i].p_vaddr & 4095) + phdr[i].p_memsz + 4095) / 4096;

    // setup pagemaps as readonly (writable = false)
    if (auto err = SetupPageMaps(dest_addr, num_4kpages, false)) {
      return { last_addr, err };
    }

    const auto src = reinterpret_cast<uint8_t*>(ehdr) + phdr[i].p_offset;
    const auto dst = reinterpret_cast<uint8_t*>(phdr[i].p_vaddr);
    memcpy(dst, src, phdr[i].p_filesz);
    memset(dst + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
  }
  return { last_addr, MAKE_ERROR(Error::kSuccess) };
}

WithError<uint64_t> LoadELF(Elf64_Ehdr* ehdr) {
  if (ehdr->e_type != ET_EXEC) {
    return { 0, MAKE_ERROR(Error::kInvalidFormat) };
  }

  const auto addr_first = GetFirstLoadAddress(ehdr);
  if (addr_first < 0xffff'8000'0000'0000) {
    return { 0, MAKE_ERROR(Error::kInvalidFormat) };
  }

  return CopyLoadSegments(ehdr);
}

WithError<PageMapEntry*> SetupPML4(Task& current_task) {
  auto pml4 = NewPageMap();
  if (pml4.error) {
    return pml4;
  }

  const auto current_pml4 = reinterpret_cast<PageMapEntry*>(GetCR3());
  memcpy(pml4.value, current_pml4, 256 * sizeof(uint64_t));

  const auto cr3 = reinterpret_cast<uint64_t>(pml4.value);
  SetCR3(cr3);
  current_task.Context().cr3 = cr3;
  return pml4;
}

Error FreePML4(Task& current_task) {
  const auto cr3 = current_task.Context().cr3;
  current_task.Context().cr3 = 0;
  ResetCR3();

  return FreePageMap(reinterpret_cast<PageMapEntry*>(cr3));
}

void ListAllEntries(FileDescriptor& fd, uint32_t dir_cluster) {
  const auto kEntriesPerCluster =
    fat::bytes_per_cluster / sizeof(fat::DirectoryEntry);

  while (dir_cluster != fat::kEndOfClusterchain) {
    auto dir = fat::GetSectorByCluster<fat::DirectoryEntry>(dir_cluster);

    for (int i = 0; i < kEntriesPerCluster; ++i) {
      if (dir[i].name[0] == 0x00) {
        return;
      } else if (static_cast<uint8_t>(dir[i].name[0]) == 0xe5) {
        continue;
      } else if (dir[i].attr == fat::Attribute::kLongName) {
        continue;
      }

      char name[13];
      fat::FormatName(dir[i], name);
      PrintToFD(fd, "%s\n", name);
    }

    dir_cluster = fat::NextCluster(dir_cluster);
  }
}

WithError<AppLoadInfo> LoadApp(fat::DirectoryEntry& file_entry, Task& task) {
  PageMapEntry* temp_pml4;
  if (auto [ pml4, err ] = SetupPML4(task); err) {
    return { {}, err };
  } else {
    temp_pml4 = pml4;
  }

  if (auto it = app_loads->find(&file_entry); it != app_loads->end()) {
    AppLoadInfo app_load = it->second;
    auto err = CopyPageMaps(temp_pml4, app_load.pml4, 4, 256);
    app_load.pml4 = temp_pml4;
    return { app_load, err };
  }

  std::vector<uint8_t> file_buf(file_entry.file_size);
  fat::LoadFile(&file_buf[0], file_buf.size(), file_entry);

  auto elf_header = reinterpret_cast<Elf64_Ehdr*>(&file_buf[0]);
  if (memcmp(elf_header->e_ident, "\x7f" "ELF", 4) != 0) {
    return { {}, MAKE_ERROR(Error::kInvalidFile) };
  }

  auto [ last_addr, err_load ] = LoadELF(elf_header);
  if (err_load) {
    return { {}, err_load };
  }

  AppLoadInfo app_load{last_addr, elf_header->e_entry, temp_pml4};
  app_loads->insert(std::make_pair(&file_entry, app_load));

  if (auto [ pml4, err ] = SetupPML4(task); err) {
    return { app_load, err };
  } else {
    app_load.pml4 = pml4;
  }
  auto err = CopyPageMaps(app_load.pml4, temp_pml4, 4, 256);
  return { app_load, err };
}

fat::DirectoryEntry* FindCommand(const char* command,
                                 unsigned long dir_cluster = 0) {
  auto file_entry = fat::FindFile(command, dir_cluster);
  if (file_entry.first != nullptr &&
      (file_entry.first->attr == fat::Attribute::kDirectory ||
       file_entry.second)) {
    return nullptr;
  } else if (file_entry.first) {
    return file_entry.first;
  }

  if (dir_cluster != 0 || strchr(command, '/') != nullptr) {
    return nullptr;
  }

  auto apps_entry = fat::FindFile("apps");
  if (apps_entry.first == nullptr ||
      apps_entry.first->attr != fat::Attribute::kDirectory) {
    return nullptr;
  }
  return FindCommand(command, apps_entry.first->FirstCluster());
}

} // namespace

std::map<fat::DirectoryEntry*, AppLoadInfo>* app_loads;

Terminal::Terminal(Task& task, const TerminalDescriptor* term_desc)
    : task_{task} {
  if (term_desc) {
    show_window_ = term_desc->show_window;
    for (int i = 0; i < files_.size(); ++i) {
      files_[i] = term_desc->files[i];
    }
  } else {
    show_window_ = true;
    for (int i = 0; i < files_.size(); ++i) {
      files_[i] = std::make_shared<TerminalFileDescriptor>(*this);
    }
  }

  if (show_window_) {
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
  }
  cmd_history_.resize(8);
}

Rectangle<int> Terminal::BlinkCursor() {
  cursor_visible_ = !cursor_visible_;
  DrawCursor(cursor_visible_);

  return {CalcCursorPos(), {7, 15}};
}

void Terminal::DrawCursor(bool visible) {
  if (show_window_) {
    const auto color = visible ? ToColor(0xffffff) : ToColor(0);
    FillRectangle(*window_->Writer(), CalcCursorPos(), {7, 15}, color);
  }
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
      if (show_window_) {
        FillRectangle(*window_->Writer(), CalcCursorPos(), {8, 16}, {0, 0, 0});
      }
      draw_area.pos = CalcCursorPos();

      if (linebuf_index_ > 0) {
        --linebuf_index_;
      }
    }
  } else if (ascii != 0) {
    if (cursor_.x < kColumns - 1 && linebuf_index_ < kLineMax - 1) {
      linebuf_[linebuf_index_] = ascii;
      ++linebuf_index_;
      if (show_window_) {
        WriteAscii(*window_->Writer(), CalcCursorPos(), ascii, {255, 255, 255});
      }
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
  char* redir_char = strchr(&linebuf_[0], '>');
  char* pipe_char = strchr(&linebuf_[0], '|');
  if (first_arg) {
    *first_arg = 0;
    do {
      ++first_arg;
    } while (isspace(*first_arg));
  }

  auto original_stdout = files_[1];
  int exit_code = 0;

  if (redir_char) {
    *redir_char = 0;
    char* redir_dest = &redir_char[1];
    while (isspace(*redir_dest)) {
      ++redir_dest;
    }

    auto [ file, post_slash ] = fat::FindFile(redir_dest);
    if (file == nullptr) {
      auto [ new_file, err ] = fat::CreateFile(redir_dest);
      if (err) {
        PrintToFD(*files_[2],
                  "failed to create a redirect file: %s\n", err.Name());
        return;
      }
      file = new_file;
    } else if (file->attr == fat::Attribute::kDirectory || post_slash) {
      PrintToFD(*files_[2], "cannot redirect to a directory\n");
      return;
    }
    files_[1] = std::make_shared<fat::FileDescriptor>(*file);
  }

  std::shared_ptr<PipeDescriptor> pipe_fd;
  uint64_t subtask_id = 0;

  if (pipe_char) {
    *pipe_char = 0;
    char* subcommand = &pipe_char[1];
    while (isspace(*subcommand)) {
      ++subcommand;
    }

    auto& subtask = task_manager->NewTask();
    pipe_fd = std::make_shared<PipeDescriptor>(subtask);
    auto term_desc = new TerminalDescriptor{
      subcommand, true, false,
      { pipe_fd, files_[1], files_[2] }
    };
    files_[1] = pipe_fd;

    subtask_id = subtask
      .InitContext(TaskTerminal, reinterpret_cast<int64_t>(term_desc))
      .Wakeup()
      .ID();
    (*layer_task_map)[layer_id_] = subtask_id;
  }

  if (strcmp(command, "echo") == 0) {
    if (first_arg && first_arg[0] == '$') {
      if (strcmp(&first_arg[1], "?") == 0) {
        PrintToFD(*files_[1], "%d", last_exit_code_);
      }
    } else if (first_arg) {
      PrintToFD(*files_[1], "%s", first_arg);
    }
    PrintToFD(*files_[1], "\n");
  } else if (strcmp(command, "clear") == 0) {
    if (show_window_) {
      FillRectangle(*window_->InnerWriter(),
                    {4, 4}, {8*kColumns, 16*kRows}, {0, 0, 0});
    }
    cursor_.y = 0;
  } else if (strcmp(command, "lspci") == 0) {
    for (int i = 0; i < pci::num_device; ++i) {
      const auto& dev = pci::devices[i];
      auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
      PrintToFD(*files_[1],
          "%02x:%02x.%d vend=%04x head=%02x class=%02x.%02x.%02x\n",
          dev.bus, dev.device, dev.function, vendor_id, dev.header_type,
          dev.class_code.base, dev.class_code.sub, dev.class_code.interface);
    }
  } else if (strcmp(command, "ls") == 0) {
    if (!first_arg || first_arg[0] == '\0') {
      ListAllEntries(*files_[1], fat::boot_volume_image->root_cluster);
    } else {
      auto [ dir, post_slash ] = fat::FindFile(first_arg);
      if (dir == nullptr) {
        PrintToFD(*files_[2], "No such file or directory: %s\n", first_arg);
        exit_code = 1;
      } else if (dir->attr == fat::Attribute::kDirectory) {
        ListAllEntries(*files_[1], dir->FirstCluster());
      } else {
        char name[13];
        fat::FormatName(*dir, name);
        if (post_slash) {
          PrintToFD(*files_[2], "%s is not a directory\n", name);
          exit_code = 1;
        } else {
          PrintToFD(*files_[1], "%s\n", name);
        }
      }
    }
  } else if (strcmp(command, "cat") == 0) {
    std::shared_ptr<FileDescriptor> fd;
    if (!first_arg || first_arg[0] == '\0') {
      fd = files_[0];
    } else {
      auto [ file_entry, post_slash ] = fat::FindFile(first_arg);
      if (!file_entry) {
        PrintToFD(*files_[2], "no such file: %s\n", first_arg);
        exit_code = 1;
      } else if (file_entry->attr != fat::Attribute::kDirectory && post_slash) {
        char name[13];
        fat::FormatName(*file_entry, name);
        PrintToFD(*files_[2], "%s is not a directory\n", name);
        exit_code = 1;
      } else {
        fd = std::make_shared<fat::FileDescriptor>(*file_entry);
      }
    }
    if (fd) {
      char u8buf[1024];
      DrawCursor(false);
      while (true) {
        if (ReadDelim(*fd, '\n', u8buf, sizeof(u8buf)) == 0) {
          break;
        }
        PrintToFD(*files_[1], "%s", u8buf);
      }
      DrawCursor(true);
    }
  } else if (strcmp(command, "noterm") == 0) {
    auto term_desc = new TerminalDescriptor{
      first_arg, true, false, files_
    };
    task_manager->NewTask()
      .InitContext(TaskTerminal, reinterpret_cast<int64_t>(term_desc))
      .Wakeup();
  } else if (strcmp(command, "memstat") == 0) {
    const auto p_stat = memory_manager->Stat();
    PrintToFD(*files_[1], "Phys used : %lu frames (%llu MiB)\n",
        p_stat.allocated_frames,
        p_stat.allocated_frames * kBytesPerFrame / 1024 / 1024);
    PrintToFD(*files_[1], "Phys total: %lu frames (%llu MiB)\n",
        p_stat.total_frames,
        p_stat.total_frames * kBytesPerFrame / 1024 / 1024);
  } else if (strcmp(command, "date") == 0) {
    EFI_TIME t;
    uefi_rt->GetTime(&t, nullptr);
    if (t.TimeZone == EFI_UNSPECIFIED_TIMEZONE) {
      PrintToFD(*files_[1], "%d-%02d-%02d %02d:%02d:%02d\n",
          t.Year, t.Month, t.Day, t.Hour, t.Minute, t.Second);
    } else {
      PrintToFD(*files_[1], "%d-%02d-%02d %02d:%02d:%02d ",
          t.Year, t.Month, t.Day, t.Hour, t.Minute, t.Second);
      if (t.TimeZone >= 0) {
        PrintToFD(*files_[1], "+%02d%02d\n", t.TimeZone / 60, t.TimeZone % 60);
      } else {
        PrintToFD(*files_[1], "-%02d%02d\n", -t.TimeZone / 60, -t.TimeZone % 60);
      }
    }
  } else if (strcmp(command, "reboot") == 0) {
    uefi_rt->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, nullptr);
  } else if (strcmp(command, "poweroff") == 0) {
    uefi_rt->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, nullptr);
  } else if (strcmp(command, "lsusb") == 0) {
    auto devmgr = usb::xhci::controller->DeviceManager();
    for (int slot = 1; slot < 256; ++slot) {
      auto dev = devmgr->FindBySlot(slot);
      if (!dev) {
        continue;
      }
      PrintToFD(*files_[1], "Slot %d: ID %04x:%04x Class %d.%d.%d\n",
                slot,
                dev->DeviceDesc().vendor_id,
                dev->DeviceDesc().product_id,
                dev->DeviceDesc().device_class,
                dev->DeviceDesc().device_sub_class,
                dev->DeviceDesc().device_protocol);
    }
  } else if (strcmp(command, "usbtest") == 0) {
    [&]{
      if (!usb::cdc::driver) {
        PrintToFD(*files_[2], "CDC device not exist\n");
        exit_code = 1;
        return;
      }

      size_t send_len;
      if (first_arg && first_arg[0]) {
        send_len = strlen(first_arg);
        usb::cdc::driver->SendSerial(first_arg, send_len);
      } else {
        send_len = 1;
        usb::cdc::driver->SendSerial("a", 1);
      }

      std::vector<uint8_t> buf(send_len);
      int recv_len = usb::cdc::driver->ReceiveSerial(buf.data(), send_len);
      while (recv_len == 0) {
        recv_len = usb::cdc::driver->ReceiveSerial(buf.data(), send_len);
      }
      PrintToFD(*files_[1], "%.*s\n", recv_len, buf.data());
    }();
  } else if (command[0] != 0) {
    auto file_entry = FindCommand(command);
    if (!file_entry) {
      PrintToFD(*files_[2], "no such command: %s\n", command);
      exit_code = 1;
    } else {
      auto [ ec, err ] = ExecuteFile(*file_entry, command, first_arg);
      if (err) {
        PrintToFD(*files_[2], "failed to exec file: %s\n", err.Name());
        exit_code = -ec;
      } else {
        exit_code = ec;
      }
    }
  }

  if (pipe_fd) {
    pipe_fd->FinishWrite();
    __asm__("cli");
    auto [ ec, err ] = task_manager->WaitFinish(subtask_id);
    (*layer_task_map)[layer_id_] = task_.ID();
    __asm__("sti");
    if (err) {
      Log(kWarn, "failed to wait finish: %s\n", err.Name());
    }
    exit_code = ec;
  }

  last_exit_code_ = exit_code;
  files_[1] = original_stdout;
}

WithError<int> Terminal::ExecuteFile(fat::DirectoryEntry& file_entry,
                                     char* command, char* first_arg) {
  __asm__("cli");
  auto& task = task_manager->CurrentTask();
  __asm__("sti");

  auto [ app_load, err ] = LoadApp(file_entry, task);
  if (err) {
    return { 0, err };
  }

  LinearAddress4Level args_frame_addr{0xffff'ffff'ffff'f000};
  if (auto err = SetupPageMaps(args_frame_addr, 1)) {
    return { 0, err };
  }
  auto argv = reinterpret_cast<char**>(args_frame_addr.value);
  int argv_len = 32; // argv = 8x32 = 256 bytes
  auto argbuf = reinterpret_cast<char*>(args_frame_addr.value + sizeof(char*) * argv_len);
  int argbuf_len = 4096 - sizeof(char*) * argv_len;
  auto argc = MakeArgVector(command, first_arg, argv, argv_len, argbuf, argbuf_len);
  if (argc.error) {
    return { 0, argc.error };
  }

  // #@@range_begin(increase_appstack)
  const int stack_size = 16 * 4096;
  LinearAddress4Level stack_frame_addr{0xffff'ffff'ffff'f000 - stack_size};
  // #@@range_end(increase_appstack)
  if (auto err = SetupPageMaps(stack_frame_addr, stack_size / 4096)) {
    return { 0, err };
  }

  for (int i = 0; i < files_.size(); ++i) {
    task.Files().push_back(files_[i]);
  }

  const uint64_t elf_next_page =
    (app_load.vaddr_end + 4095) & 0xffff'ffff'ffff'f000;
  task.SetDPagingBegin(elf_next_page);
  task.SetDPagingEnd(elf_next_page);

  task.SetFileMapEnd(stack_frame_addr.value);

  int ret = CallApp(argc.value, argv, 3 << 3 | 3, app_load.entry,
                    stack_frame_addr.value + stack_size - 8,
                    &task.OSStackPointer());

  task.Files().clear();
  task.FileMaps().clear();

  if (auto err = CleanPageMaps(LinearAddress4Level{0xffff'8000'0000'0000})) {
    return { ret, err };
  }
  return { ret, FreePML4(task) };
}

void Terminal::Print(char32_t c) {
  if (!show_window_) {
    return;
  }

  auto newline = [this]() {
    cursor_.x = 0;
    if (cursor_.y < kRows - 1) {
      ++cursor_.y;
    } else {
      Scroll1();
    }
  };

  if (c == U'\n') {
    newline();
  } else if (IsHankaku(c)) {
    if (cursor_.x == kColumns) {
      newline();
    }
    WriteUnicode(*window_->Writer(), CalcCursorPos(), c, {255, 255, 255});
    ++cursor_.x;
  } else {
    if (cursor_.x >= kColumns - 1) {
      newline();
    }
    WriteUnicode(*window_->Writer(), CalcCursorPos(), c, {255, 255, 255});
    cursor_.x += 2;
  }
}

void Terminal::Print(const char* s, std::optional<size_t> len) {
  const auto cursor_before = CalcCursorPos();
  DrawCursor(false);

  size_t i = 0;
  const size_t len_ = len ? *len : std::numeric_limits<size_t>::max();

  while (s[i] && i < len_) {
    const auto [ u32, bytes ] = ConvertUTF8To32(&s[i]);
    Print(u32);
    i += bytes;
  }

  DrawCursor(true);
  const auto cursor_after = CalcCursorPos();

  Vector2D<int> draw_pos{ToplevelWindow::kTopLeftMargin.x, cursor_before.y};
  Vector2D<int> draw_size{window_->InnerSize().x,
                          cursor_after.y - cursor_before.y + 16};

  Rectangle<int> draw_area{draw_pos, draw_size};

  Message msg = MakeLayerMessage(
      task_.ID(), LayerID(), LayerOperation::DrawArea, draw_area);
  __asm__("cli");
  task_manager->SendMessage(1, msg);
  __asm__("sti");
}

void Terminal::Redraw() {
  Rectangle<int> draw_area{ToplevelWindow::kTopLeftMargin,
                           window_->InnerSize()};

  Message msg = MakeLayerMessage(
      task_.ID(), LayerID(), LayerOperation::DrawArea, draw_area);
  __asm__("cli");
  task_manager->SendMessage(1, msg);
  __asm__("sti");
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
  const auto term_desc = reinterpret_cast<TerminalDescriptor*>(data);
  bool show_window = true;
  if (term_desc) {
    show_window = term_desc->show_window;
  }

  __asm__("cli");
  Task& task = task_manager->CurrentTask();
  Terminal* terminal = new Terminal{task, term_desc};
  if (show_window) {
    layer_manager->Move(terminal->LayerID(), {100, 200});
    layer_task_map->insert(std::make_pair(terminal->LayerID(), task_id));
    active_layer->Activate(terminal->LayerID());
  }
  __asm__("sti");

  if (term_desc && !term_desc->command_line.empty()) {
    for (int i = 0; i < term_desc->command_line.length(); ++i) {
      terminal->InputKey(0, 0, term_desc->command_line[i]);
    }
    terminal->InputKey(0, 0, '\n');
  }

  if (term_desc && term_desc->exit_after_command) {
    delete term_desc;
    __asm__("cli");
    task_manager->Finish(terminal->LastExitCode());
  }

  auto add_blink_timer = [task_id](unsigned long t){
    timer_manager->AddTimer(Timer{t + static_cast<int>(kTimerFreq * 0.5),
                                  1, task_id});
  };
  add_blink_timer(timer_manager->CurrentTick());

  bool window_isactive = false;

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
      add_blink_timer(msg->arg.timer.timeout);
      if (show_window && window_isactive) {
        const auto area = terminal->BlinkCursor();
        Message msg = MakeLayerMessage(
            task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
        __asm__("cli");
        task_manager->SendMessage(1, msg);
        __asm__("sti");
      }
      break;
    case Message::kKeyPush:
      if (msg->arg.keyboard.press) {
        const auto area = terminal->InputKey(msg->arg.keyboard.modifier,
                                             msg->arg.keyboard.keycode,
                                             msg->arg.keyboard.ascii);
        if (show_window) {
          Message msg = MakeLayerMessage(
              task_id, terminal->LayerID(), LayerOperation::DrawArea, area);
          __asm__("cli");
          task_manager->SendMessage(1, msg);
          __asm__("sti");
        }
      }
      break;
    case Message::kWindowActive:
      window_isactive = msg->arg.window_active.activate;
      break;
    case Message::kWindowClose:
      CloseLayer(msg->arg.window_close.layer_id);
      __asm__("cli");
      task_manager->Finish(terminal->LastExitCode());
      break;
    default:
      break;
    }
  }
}

TerminalFileDescriptor::TerminalFileDescriptor(Terminal& term)
    : term_{term} {
}

size_t TerminalFileDescriptor::Read(void* buf, size_t len) {
  char* bufc = reinterpret_cast<char*>(buf);

  while (true) {
    __asm__("cli");
    auto msg = term_.UnderlyingTask().ReceiveMessage();
    if (!msg) {
      term_.UnderlyingTask().Sleep();
      continue;
    }
    __asm__("sti");

    if (msg->type != Message::kKeyPush || !msg->arg.keyboard.press) {
      continue;
    }
    if (msg->arg.keyboard.modifier & (kLControlBitMask | kRControlBitMask)) {
      char s[3] = "^ ";
      s[1] = toupper(msg->arg.keyboard.ascii);
      term_.Print(s);
      if (msg->arg.keyboard.keycode == 7 /* D */) {
        return 0; // EOT
      }
      continue;
    }

    bufc[0] = msg->arg.keyboard.ascii;
    term_.Print(bufc, 1);
    term_.Redraw();
    return 1;
  }
}

size_t TerminalFileDescriptor::Write(const void* buf, size_t len) {
  term_.Print(reinterpret_cast<const char*>(buf), len);
  term_.Redraw();
  return len;
}

size_t TerminalFileDescriptor::Load(void* buf, size_t len, size_t offset) {
  return 0;
}

PipeDescriptor::PipeDescriptor(Task& task) : task_{task} {
}

size_t PipeDescriptor::Read(void* buf, size_t len) {
  if (len_ > 0) {
    const size_t copy_bytes = std::min(len_, len);
    memcpy(buf, data_, copy_bytes);
    len_ -= copy_bytes;
    memmove(data_, &data_[copy_bytes], len_);
    return copy_bytes;
  }

  if (closed_) {
    return 0;
  }

  while (true) {
    __asm__("cli");
    auto msg = task_.ReceiveMessage();
    if (!msg) {
      task_.Sleep();
      continue;
    }
    __asm__("sti");

    if (msg->type != Message::kPipe) {
      continue;
    }

    if (msg->arg.pipe.len == 0) {
      closed_ = true;
      return 0;
    }

    const size_t copy_bytes = std::min<size_t>(msg->arg.pipe.len, len);
    memcpy(buf, msg->arg.pipe.data, copy_bytes);
    len_ = msg->arg.pipe.len - copy_bytes;
    memcpy(data_, &msg->arg.pipe.data[copy_bytes], len_);
    return copy_bytes;
  }
}

size_t PipeDescriptor::Write(const void* buf, size_t len) {
  auto bufc = reinterpret_cast<const char*>(buf);
  Message msg{Message::kPipe};
  size_t sent_bytes = 0;
  while (sent_bytes < len) {
    msg.arg.pipe.len = std::min(len - sent_bytes, sizeof(msg.arg.pipe.data));
    memcpy(msg.arg.pipe.data, &bufc[sent_bytes], msg.arg.pipe.len);
    sent_bytes += msg.arg.pipe.len;
    __asm__("cli");
    task_.SendMessage(msg);
    __asm__("sti");
  }
  return len;
}

void PipeDescriptor::FinishWrite() {
  Message msg{Message::kPipe};
  msg.arg.pipe.len = 0;
  __asm__("cli");
  task_.SendMessage(msg);
  __asm__("sti");
}
