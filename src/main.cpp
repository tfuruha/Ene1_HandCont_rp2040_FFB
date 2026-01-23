#include "shared_data.h"
#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

// 共有データの実体定義
SharedData sharedData = {0};

// --- USB HID PID 関連定義 ---
// HID Report Descriptor (最小構成のPID/Joystick)
// 注:
// 本来は完全なPIDデスクリプタが必要だが、ここでは最小限のJoystickとして定義し、後ほど拡張可能とする
uint8_t const desc_hid_report[] = {TUD_HID_REPORT_DESC_GAMEPAD()};

Adafruit_USBD_HID usb_hid;

// --- Core 0: USB 通信・アプリ管理 ---
void setup() {
  Serial.begin(115200);

  // TinyUSB の初期化
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.begin();

  // USBがマウントされるのを待つ (任意)
  // while ( !TinyUSBDevice.mounted() ) delay(1);

  Serial.println("Core 0: USB HID PID Initialized");
}

void loop() {
  // HIDレポートの送信 (20ms周期など)
  static uint32_t last_report_ms = 0;
  if (millis() - last_report_ms >= 20) {
    last_report_ms = millis();

    if (TinyUSBDevice.suspended()) {
      TinyUSBDevice.remoteWakeup();
    }

    if (usb_hid.ready()) {
      hid_gamepad_report_t report = {
          .x = (int8_t)map(sharedData.steeringAngle, -32768, 32767, -127, 127),
          .y = 0,
          .z = 0,
          .rz = 0,
          .rx = 0,
          .ry = 0,
          .hat = 0,
          .buttons = sharedData.buttons};
      // 注: 実際にはアクセル/ブレーキもマッピングして送信する
      usb_hid.sendReport(0, &report, sizeof(report));
    }
  }
}

// --- Core 1: 1ms 周期制御 ---
void setup1() {
  // Core 1 用の初期化 (センサ、CANなど)
  pinMode(LED_BUILTIN, OUTPUT);
  sharedData.lastCore1Micros = micros();
  Serial.println("Core 1: Control Loop Started");
}

void loop1() {
  // 厳密な 1ms (1000us) 周期制御
  const uint32_t INTERVAL_US = 1000;
  static uint32_t next_us = micros();

  // 次の実行時刻まで待機 (ビジーウェイト)
  while (micros() < next_us) {
    // 必要に応じて低優先度の処理をここに記述
  }

  // 周期処理の開始
  sharedData.lastCore1Micros = micros();
  sharedData.core1LoopCount++;

  // --- センサ取得・制御ロジックのスケルトン ---
  // sharedData.steeringAngle = readSteering();
  // sharedData.accelerator = readAccel();
  // ...

  // デバッグ用: Lチカ (1秒周期)
  if (sharedData.core1LoopCount % 1000 == 0) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }

  // 次の実行時刻を更新 (ドリフト防止)
  next_us += INTERVAL_US;
}
