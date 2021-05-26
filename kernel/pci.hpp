/**
 * @file pci.hpp
 *
 * PCI バス制御のプログラムを集めたファイル．
 */

#pragma once

#include <cstdint>
#include <array>

#include "error.hpp"

namespace pci {
  // #@@range_begin(config_addr)
  /** @brief CONFIG_ADDRESS レジスタの IO ポートアドレス */
  const uint16_t kConfigAddress = 0x0cf8;
  /** @brief CONFIG_DATA レジスタの IO ポートアドレス */
  const uint16_t kConfigData = 0x0cfc;
  // #@@range_end(config_addr)

  /** @brief CONFIG_ADDRESS に指定された整数を書き込む */
  void WriteAddress(uint32_t address);
  /** @brief CONFIG_DATA に指定された整数を書き込む */
  void WriteData(uint32_t value);
  /** @brief CONFIG_DATA から 32 ビット整数を読み込む */
  uint32_t ReadData();

  /** @brief ベンダ ID レジスタを読み取る（全ヘッダタイプ共通） */
  uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function);
  /** @brief デバイス ID レジスタを読み取る（全ヘッダタイプ共通） */
  uint16_t ReadDeviceId(uint8_t bus, uint8_t device, uint8_t function);
  /** @brief ヘッダタイプレジスタを読み取る（全ヘッダタイプ共通） */
  uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function);
  /** @brief クラスコードレジスタを読み取る（全ヘッダタイプ共通）
   *
   * 返される 32 ビット整数の構造は次の通り．
   *   - 31:24 : ベースクラス
   *   - 23:16 : サブクラス
   *   - 15:8  : インターフェース
   *   - 7:0   : リビジョン
   */
  uint32_t ReadClassCode(uint8_t bus, uint8_t device, uint8_t function);

  /** @brief バス番号レジスタを読み取る（ヘッダタイプ 1 用）
   *
   * 返される 32 ビット整数の構造は次の通り．
   *   - 23:16 : サブオーディネイトバス番号
   *   - 15:8  : セカンダリバス番号
   *   - 7:0   : リビジョン番号
   */
  uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function);

  /** @brief 単一ファンクションの場合に真を返す． */
  bool IsSingleFunctionDevice(uint8_t header_type);

  /** @brief PCI デバイスを操作するための基礎データを格納する
   *
   * バス番号，デバイス番号，ファンクション番号はデバイスを特定するのに必須．
   * その他の情報は単に利便性のために加えてある．
   * */
  struct Device {
    uint8_t bus, device, function, header_type;
  };

  // #@@range_begin(var_devices)
  /** @brief ScanAllBus() により発見された PCI デバイスの一覧 */
  inline std::array<Device, 32> devices;
  /** @brief devices の有効な要素の数 */
  inline int num_device;
  /** @brief PCI デバイスをすべて探索し devices に格納する
   *
   * バス 0 から再帰的に PCI デバイスを探索し，devices の先頭から詰めて書き込む．
   * 発見したデバイスの数を num_device に設定する．
   */
  Error ScanAllBus();
  // #@@range_end(var_devices)
}
