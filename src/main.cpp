/**
 * @file main.cpp
 * @brief Ene1_HandCont_rp2040_FFB メインプログラム
 */

#include "MCP2515_Wrapper.h"
#include "MF4015_Driver.h"
#include "config.h"
#include "config_manager.h"
#include "shared_data.h"
#include "util.h"
#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

// ============================================================================
// グローバルインスタンス
// ============================================================================

// 共有データの実体
SharedData sharedData = {0};

// USB HID インスタンス
Adafruit_USBD_HID usb_hid;

// CANバスラッパー (MCP2515固有の実装を内包)
MCP2515_Wrapper canWrapper(PIN_CAN_CS);

// MF4015モータードライバ (抽象インターフェースに依存)
MF4015_Driver mfMotor(&canWrapper, MF4015_CAN_ID);

// mf4015.cpp (旧コードとの互換用) で使用する extern ポインタも一応紐付けておく
extern CANInterface *canBus;

// --- HIDレポート記述子定義 ---
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_GAMEPAD() // 最小構成。将来的にPID/FFB拡張が必要。
};

// ============================================================================
// Core 0: USB 通信・アプリ管理・FFB受信
// ============================================================================
static uint32_t last_hid_report_ms; // HIDレポート送信 前回時刻

void setup() {
  Serial.begin(SERIAL_BAUDRATE);

  // 設定管理の初期化と復元
  if (ConfigManager::begin()) {
    ConfigManager::loadConfig();
  }

  // TinyUSB の初期化
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();
  last_hid_report_ms = 0; //
  Serial.println("Core 0: System Initialized (USB/Setup)");
}

/**
 * @brief Core 0 ループ
 *
 * USB HID通信の維持と、ホストからのFFBパケット解析を行う
 */

void loop() {
  // HIDレポート送信 (HIDREO_INTERVAL_MS ms周期)
  if (checkInterval_m(last_hid_report_ms, HIDREPO_INTERVAL_MS)) {
    if (usb_hid.ready()) {
      hid_gamepad_report_t report = {0};
      // Core 1が計算したステアリング・ペダル値をHIDパケットに変換
      report.x =
          (int8_t)map(sharedData.steeringAngle, -32768, 32767, -127, 127);
      report.y = (int8_t)map(sharedData.accelerator, 0, 65535, -127, 127);
      report.z = (int8_t)map(sharedData.brake, 0, 65535, -127, 127);
      report.buttons = sharedData.buttons;

      usb_hid.sendReport(0, &report, sizeof(report));
    }
  }

  // --- FFB 受信スケルトン ---
  // PCからのFFB Outパケット（Output Report）をチェックするロジック
  //
  // note: adafruit_hid には getOutputReport() 等のコールバックがある
  // if (usb_hid.available()) {
  //     // 受信したPIDパケットから targetTorque を抽出し sharedData に書き込む
  //     // sharedData.targetTorque = parse_ffb_packet(...);
  // }
}

// ============================================================================
// Core 1: 1ms 周期制御・CAN通信
// ============================================================================
static uint32_t last_strcont_us; // ステアリング制御前回時刻
void setup1() {
  // CANインターフェースのポインタをグローバルにも紐付け
  canBus = &canWrapper;

  // CAN通信開始
  if (canWrapper.begin()) {
    Serial.println("Core 1: CAN Initialized");
    // モーターを有効化
    mfMotor.enable();
  } else {
    Serial.println("Core 1: CAN Init FAILED!");
  }
  last_strcont_us = micros(); // ステアリング制御前回時刻 初期化
  sharedData.lastCore1Micros = micros();
  Serial.println("Core 1: Control Loop Started");
}

/**
 * @brief Core 1 ループ (1000us周期)
 *
 * センサー値の読み取り、CANメッセージの受信解析、
 * および目標トルクに基づくモーター制御指令の送出を行う。
 * 非ブロッキングでCANバッファを監視しつつ、高精度な周期実行を行う。
 */
void loop1() {
  // 1. CAN受信バッファの常時監視 (周期に関わらず可能な限り頻繁に実行)
  if (canWrapper.available()) {
    uint32_t id;
    uint8_t len;
    uint8_t data[8];
    if (canWrapper.readFrame(id, len, data)) {
      // モータードライバにパケットを渡してステータス更新
      mfMotor.parseFrame(id, len, data);
    }
  }

  // 2. 厳密な 1ms 周期制御 (累積誤差の排除)
  uint32_t now_us = micros();
  if (checkInterval_u(last_strcont_us, STEAR_CONT_INTERVAL_US)) {

    // --- 周期処理開始 ---
    sharedData.lastCore1Micros = now_us;
    sharedData.core1LoopCount++;

    // A. ドライバから読み取ったステータスを sharedData に反映
    sharedData.steeringAngle = (int16_t)mfMotor.getEncoderValue();

    // B. 他のIO（ペダル等）の読み取り（将来の実装用）
    // sharedData.accelerator = ...
    // sharedData.brake = ...

    // C. Core 0 が更新した目標トルクをモーターに送信
    int16_t torque = sharedData.targetTorque;
    mfMotor.setTorque(torque);
  }
}
