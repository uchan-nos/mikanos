/**
 * @file logger.hpp
 *
 * カーネルロガーの実装．
 */

#pragma once

enum LogLevel {
  kError = 3,
  kWarn  = 4,
  kInfo  = 6,
  kDebug = 7,
};

/** @brief グローバルなログ優先度のしきい値を変更する．
 *
 * グローバルなログ優先度のしきい値を level に設定する．
 * 以降の Log の呼び出しでは，ここで設定した優先度以上のログのみ記録される．
 */
void SetLogLevel(enum LogLevel level);

/** @brief ログを指定された優先度で記録する．
 *
 * 指定された優先度がしきい値以上ならば記録する．
 * 優先度がしきい値未満ならログは捨てられる．
 *
 * @param level  ログの優先度．しきい値以上の優先度のログのみが記録される．
 * @param format  書式文字列．printk と互換．
 */
int Log(enum LogLevel level, const char* format, ...)
    __attribute__((format(printf, 2, 3)));
