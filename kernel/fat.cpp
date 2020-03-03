#include "fat.hpp"

#include <algorithm>
#include <cstring>
#include <cctype>
#include <utility>

namespace {

std::pair<const char*, bool>
NextPathElement(const char* path, char* path_elem) {
  const char* next_slash = strchr(path, '/');
  if (next_slash == nullptr) {
    strcpy(path_elem, path);
    return { nullptr, false };
  }

  const auto elem_len = next_slash - path;
  strncpy(path_elem, path, elem_len);
  path_elem[elem_len] = '\0';
  return { &next_slash[1], true };
}

} // namespace

namespace fat {

BPB* boot_volume_image;
unsigned long bytes_per_cluster;

void Initialize(void* volume_image) {
  boot_volume_image = reinterpret_cast<fat::BPB*>(volume_image);
  bytes_per_cluster =
    static_cast<unsigned long>(boot_volume_image->bytes_per_sector) *
    boot_volume_image->sectors_per_cluster;
}

uintptr_t GetClusterAddr(unsigned long cluster) {
  unsigned long sector_num =
    boot_volume_image->reserved_sector_count +
    boot_volume_image->num_fats * boot_volume_image->fat_size_32 +
    (cluster - 2) * boot_volume_image->sectors_per_cluster;
  uintptr_t offset = sector_num * boot_volume_image->bytes_per_sector;
  return reinterpret_cast<uintptr_t>(boot_volume_image) + offset;
}

void ReadName(const DirectoryEntry& entry, char* base, char* ext) {
  memcpy(base, &entry.name[0], 8);
  base[8] = 0;
  for (int i = 7; i >= 0 && base[i] == 0x20; --i) {
    base[i] = 0;
  }

  memcpy(ext, &entry.name[8], 3);
  ext[3] = 0;
  for (int i = 2; i >= 0 && ext[i] == 0x20; --i) {
    ext[i] = 0;
  }
}

void FormatName(const DirectoryEntry& entry, char* dest) {
  char ext[5] = ".";
  ReadName(entry, dest, &ext[1]);
  if (ext[1]) {
    strcat(dest, ext);
  }
}

unsigned long NextCluster(unsigned long cluster) {
  uintptr_t fat_offset =
    boot_volume_image->reserved_sector_count *
    boot_volume_image->bytes_per_sector;
  uint32_t* fat = reinterpret_cast<uint32_t*>(
      reinterpret_cast<uintptr_t>(boot_volume_image) + fat_offset);
  uint32_t next = fat[cluster];
  if (next >= 0x0ffffff8ul) {
    return kEndOfClusterchain;
  }
  return next;
}

std::pair<DirectoryEntry*, bool>
FindFile(const char* path, unsigned long directory_cluster) {
  if (path[0] == '/') {
    directory_cluster = boot_volume_image->root_cluster;
    ++path;
  } else if (directory_cluster == 0) {
    directory_cluster = boot_volume_image->root_cluster;
  }

  char path_elem[13];
  const auto [ next_path, post_slash ] = NextPathElement(path, path_elem);
  const bool path_last = next_path == nullptr || next_path[0] == '\0';

  while (directory_cluster != kEndOfClusterchain) {
    auto dir = GetSectorByCluster<DirectoryEntry>(directory_cluster);
    for (int i = 0; i < bytes_per_cluster / sizeof(DirectoryEntry); ++i) {
      if (dir[i].name[0] == 0x00) {
        goto not_found;
      } else if (!NameIsEqual(dir[i], path_elem)) {
        continue;
      }

      if (dir[i].attr == Attribute::kDirectory && !path_last) {
        return FindFile(next_path, dir[i].FirstCluster());
      } else {
        // dir[i] がディレクトリではないか，パスの末尾に来てしまったので探索をやめる
        return { &dir[i], post_slash };
      }
    }

    directory_cluster = NextCluster(directory_cluster);
  }

not_found:
  return { nullptr, post_slash };
}

bool NameIsEqual(const DirectoryEntry& entry, const char* name) {
  unsigned char name83[11];
  memset(name83, 0x20, sizeof(name83));

  int i = 0;
  int i83 = 0;
  for (; name[i] != 0 && i83 < sizeof(name83); ++i, ++i83) {
    if (name[i] == '.') {
      i83 = 7;
      continue;
    }
    name83[i83] = toupper(name[i]);
  }

  return memcmp(entry.name, name83, sizeof(name83)) == 0;
}

size_t LoadFile(void* buf, size_t len, const DirectoryEntry& entry) {
  auto is_valid_cluster = [](uint32_t c) {
    return c != 0 && c != fat::kEndOfClusterchain;
  };
  auto cluster = entry.FirstCluster();

  const auto buf_uint8 = reinterpret_cast<uint8_t*>(buf);
  const auto buf_end = buf_uint8 + len;
  auto p = buf_uint8;

  while (is_valid_cluster(cluster)) {
    if (bytes_per_cluster >= buf_end - p) {
      memcpy(p, GetSectorByCluster<uint8_t>(cluster), buf_end - p);
      return len;
    }
    memcpy(p, GetSectorByCluster<uint8_t>(cluster), bytes_per_cluster);
    p += bytes_per_cluster;
    cluster = NextCluster(cluster);
  }
  return p - buf_uint8;
}

FileDescriptor::FileDescriptor(DirectoryEntry& fat_entry)
    : fat_entry_{fat_entry} {
}

size_t FileDescriptor::Read(void* buf, size_t len) {
  if (rd_cluster_ == 0) {
    rd_cluster_ = fat_entry_.FirstCluster();
  }
  uint8_t* buf8 = reinterpret_cast<uint8_t*>(buf);
  len = std::min(len, fat_entry_.file_size - rd_off_);

  size_t total = 0;
  while (total < len) {
    uint8_t* sec = GetSectorByCluster<uint8_t>(rd_cluster_);
    size_t n = std::min(len - total, bytes_per_cluster - rd_cluster_off_);
    memcpy(&buf8[total], &sec[rd_cluster_off_], n);
    total += n;

    rd_cluster_off_ += n;
    if (rd_cluster_off_ == bytes_per_cluster) {
      rd_cluster_ = NextCluster(rd_cluster_);
      rd_cluster_off_ = 0;
    }
  }

  rd_off_ += total;
  return total;
}

} // namespace fat
