/**
 * @file usb/arraymap.hpp
 *
 * 固定長配列を用いた簡易なマップ実装．
 */

#pragma once

#include <array>
#include <optional>

namespace usb {
  template <class K, class V, size_t N = 16>
  class ArrayMap {
   public:
     std::optional<V> Get(const K& key) const {
      for (int i = 0; i < table_.size(); ++i) {
        if (auto opt_k = table_[i].first; opt_k && opt_k.value() == key) {
          return table_[i].second;
        }
      }
      return std::nullopt;
    }

    void Put(const K& key, const V& value) {
      for (int i = 0; i < table_.size(); ++i) {
        if (!table_[i].first) {
          table_[i].first = key;
          table_[i].second = value;
          break;
        }
      }
    }

    void Delete(const K& key) {
      for (int i = 0; i < table_.size(); ++i) {
        if (auto opt_k = table_[i].first; opt_k && opt_k.value() == key) {
          table_[i].first = std::nullopt;
          break;
        }
      }
    }

   private:
    std::array<std::pair<std::optional<K>, V>, N> table_{};
  };
}
