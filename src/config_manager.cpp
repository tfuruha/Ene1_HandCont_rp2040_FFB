/**
 * @file config_manager.cpp
 * @brief ConfigManagerクラスの実装
 */

#include "config_manager.h"

const char *ConfigManager::CONFIG_FILE = "/config.bin";

bool ConfigManager::begin() {
  if (!LittleFS.begin()) {
    Serial.println("ConfigManager: LittleFS mount failed. Formatting...");
    if (!LittleFS.format()) {
      Serial.println("ConfigManager: LittleFS format failed.");
      return false;
    }
    if (!LittleFS.begin()) {
      Serial.println("ConfigManager: LittleFS mount failed after format.");
      return false;
    }
  }
  return true;
}

bool ConfigManager::saveConfig() {
  File file = LittleFS.open(CONFIG_FILE, "w");
  if (!file) {
    Serial.println("ConfigManager: Failed to open file for writing");
    return false;
  }

  // 現在のところsharedDataにはconfig構造体がないため、将来的な拡張性を考慮しつつ
  // 現状の主要な設定値をデモとして保存するロジック（またはプレースホルダ）
  // ユーザーの要求どおり「shared_data.config」が存在することを想定したコードにする場合は
  // 構造体の追加が必要。一旦、SharedData全体をバイナリとして保存する例：

  // 注意:
  // volatileポインタからの書き込み時は注意が必要だが、バイナリ一括なら可能
  size_t written = file.write((const uint8_t *)&sharedData, sizeof(SharedData));
  file.close();

  if (written == sizeof(SharedData)) {
    Serial.println("ConfigManager: Config saved successfully");
    return true;
  } else {
    Serial.println("ConfigManager: Save failed (size mismatch)");
    return false;
  }
}

bool ConfigManager::loadConfig() {
  if (!LittleFS.exists(CONFIG_FILE)) {
    Serial.println("ConfigManager: No config file found. Using defaults.");
    return false;
  }

  File file = LittleFS.open(CONFIG_FILE, "r");
  if (!file) {
    Serial.println("ConfigManager: Failed to open file for reading");
    return false;
  }

  SharedData temp;
  size_t readLen = file.read((uint8_t *)&temp, sizeof(SharedData));
  file.close();

  if (readLen == sizeof(SharedData)) {
    // Core1動作中ならミューテックスが必要だが、起動時想定
    sharedData = temp;
    Serial.println("ConfigManager: Config loaded successfully");
    return true;
  } else {
    Serial.println("ConfigManager: Load failed (size mismatch)");
    return false;
  }
}
