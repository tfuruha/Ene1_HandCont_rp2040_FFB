#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "shared_data.h"
#include <Arduino.h>
#include <LittleFS.h>


/**
 * @file config_manager.h
 * @brief 設定値の保存・復元を行うクラス
 */

class ConfigManager {
public:
  /**
   * @brief LittleFSの初期化
   * @return true: 成功, false: 失敗
   */
  static bool begin();

  /**
   * @brief 設定値をFlash(LittleFS)に保存する
   * @return true: 成功, false: 失敗
   */
  static bool saveConfig();

  /**
   * @brief Flash(LittleFS)から設定値を読み込む
   * @return true: 成功, false: 失敗
   */
  static bool loadConfig();

private:
  static const char *CONFIG_FILE;
};

#endif // CONFIG_MANAGER_H
