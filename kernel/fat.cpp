#include "fat.hpp"

#include <cstring>

namespace fat {

// #@@range_begin(init_fat)
BPB* boot_volume_image;

void Initialize(void* volume_image) {
  boot_volume_image = reinterpret_cast<fat::BPB*>(volume_image);
}
// #@@range_end(init_fat)

// #@@range_begin(get_cluster_addr)
uintptr_t GetClusterAddr(unsigned long cluster) {
  unsigned long sector_num =
    boot_volume_image->reserved_sector_count +
    boot_volume_image->num_fats * boot_volume_image->fat_size_32 +
    (cluster - 2) * boot_volume_image->sectors_per_cluster;
  uintptr_t offset = sector_num * boot_volume_image->bytes_per_sector;
  return reinterpret_cast<uintptr_t>(boot_volume_image) + offset;
}
// #@@range_end(get_cluster_addr)

// #@@range_begin(read_name)
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
// #@@range_end(read_name)

} // namespace fat
