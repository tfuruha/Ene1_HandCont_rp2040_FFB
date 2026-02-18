/**
 * @file main.cpp
 * @brief Ene1_HandCont_rp2040_FFB メインプログラム
 */

#include "ADInput.h"
#include "DigitalInput.h"
#include "Ene1HandCont_IO.h"
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

// CANバスラッパー (MCP2515固有の実装を内包)
MCP2515_Wrapper canWrapper(Config::Pin::CAN_CS, Config::Pin::SPI_SCK,
                           Config::Pin::SPI_TX, Config::Pin::SPI_RX,
                           Config::Pin::SPI_INT);

// MF4015モータードライバ (抽象インターフェースに依存)
MF4015_Driver mfMotor(&canWrapper, Config::Steer::CAN_ID);

// mf4015.cpp (旧コードとの互換用) で使用する extern ポインタも一応紐付けておく
extern CANInterface *canBus;

// --- 周期管理 ---
static IntervalTrigger_m hidReportTrigger(Config::Time::HIDREPO_INTERVAL_MS);
static IntervalTrigger_u stearContTrigger(Config::Time::STEAR_CONT_INTERVAL_US);
static IntervalTrigger_m sampleTrigger(Config::Time::SAMPLING_INTERVAL_MS);

// --- Core間通信用 (hidwffb.h に実体があるが main.cpp でも管理が必要なフラグ等)
// ---
static custom_gamepad_report_t core1_input_report = {0};
static FFB_Shared_State_t core1_effects[MAX_EFFECTS];

void setup() {
  // Serial.begin(Config::SERIAL_BAUDRATE);

  // 設定管理の初期化と復元
  if (ConfigManager::begin()) {
    ConfigManager::loadConfig();
  }

  // HIDモジュールの初期化 (1msポーリング)
  hidwffb_begin(1);

  // 共有メモリ・ミューテックスの初期化
  ffb_shared_memory_init();

  hidReportTrigger.init();
  Serial.println("Core 0: System Initialized (USB/Setup)");
}

/**
 * @brief Core 0 ループ
 *
 * USB HID通信の維持と、ホストからのFFBパケット解析を行う
 */

void loop() {
  // 1. HIDレポート送信 (HIDREPO_INTERVAL_MS ms周期)
  if (hidReportTrigger.hasExpired()) {
    if (hidwffb_ready()) {
      custom_gamepad_report_t report = {0};
      // 共有メモリから入力を取得
      ffb_core0_get_input_report(&report);
      // HID送信
      hidwffb_send_report(&report);
    }
  }

  // 2. PID 解析結果の共有 (FFB受信時)
  pid_debug_info_t pid_info;
  if (hidwffb_get_pid_debug_info(&pid_info)) {
    // 解析結果（Gain, Magnitude等）を共有メモリへ
    ffb_core0_update_shared(&pid_info);
  }
}

// ============================================================================
// Core 1: 1ms 周期制御・CAN通信
// ============================================================================

void setup1() {
  // CANインターフェースのポインタをグローバルにも紐付け
  canBus = &canWrapper;
  Serial.begin(Config::SERIAL_BAUDRATE);
  uint32_t start_time = millis();
  while (!Serial && (millis() - start_time < 3000))
    ; // CAN初期化のメッセージ取得のため3秒待機
  // IOの初期化
  diKeyUp.Init();
  diKeyDown.Init();
  adAccel.Init();
  adBrake.Init();
  sampleTrigger.init();

  // CAN通信開始
  if (canWrapper.begin()) {
    Serial.println("Core 1: CAN Initialized");
    // モーターを有効化
    mfMotor.enable();
  } else {
    uint8_t err = canWrapper.getLastError();
    Serial.printf("Core 1: CAN Init FAILED! Error Code: %d (CS Pin: %d)\n", err,
                  Config::Pin::CAN_CS);
    Serial.print("  Hint: ");
    switch (err) {
    case 1:
      Serial.println("ERROR_FAIL (MCP2515 not responding/Reset failed)");
      break;
    case 2:
      Serial.println("ERROR_ALLTXBUSY");
      break;
    case 3:
      Serial.println("ERROR_FAILINIT (Bitrate setting failed)");
      break;
    case 4:
      Serial.println("ERROR_FAILTX");
      break;
    case 5:
      Serial.println("ERROR_NOMSG");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }
  stearContTrigger.init();
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

  // 2. 厳密な 500us 周期制御
  uint32_t now_us = micros();
  static uint32_t lastRequestMicros = 0;
  static bool waitingForEncoder = false;

  if (stearContTrigger.hasExpired()) {
    // A. 周期の冒頭で「最新角度」を要求
    mfMotor.requestEncoder();
    lastRequestMicros = now_us;
    waitingForEncoder = true;
  }

  // B.
  // エンコーダ値が更新された、またはタイムアウト（フェイルセーフ）時に制御を実行
  // タイムアウトは制御周期の半分程度 (250us) に設定
  bool timeout = waitingForEncoder && (now_us - lastRequestMicros > 500);

  if (mfMotor.checkEncoderUpdated() || timeout) {
    waitingForEncoder = false;

    // --- 同期制御処理開始 ---
    sharedData.lastCore1Micros = now_us;
    sharedData.core1LoopCount++;

    // 1. 共有メモリから FFB 命令を取得
    ffb_core1_update_shared(&core1_input_report, core1_effects);

    // 2.
    // 最新のステアリング角度を取得（parseFrameによって更新済み、またはタイムアウト時の前回値）
    core1_input_report.steer = mfMotor.getSteerValue();

    // 3. 他のIO（ペダル等）の読み取り
    if (sampleTrigger.hasExpired()) {
      adBrake.getadc();
      adAccel.getadc();
      diKeyUp.update();
      diKeyDown.update();

      core1_input_report.accel = (int16_t)adAccel.getvalue();
      core1_input_report.brake = (int16_t)adBrake.getvalue();

      uint16_t btnMask = 0;
      if (diKeyUp.getState() == LOW)
        btnMask |= (1 << 0);
      if (diKeyDown.getState() == LOW)
        btnMask |= (1 << 1);
      core1_input_report.buttons = btnMask;
    }

    // 4. トルク演算と出力
    int16_t torque = core1_effects[0].magnitude;
    sharedData.targetTorque = torque;

    // センタリングのテスト (バネ力)
    float k = -0.06f * ((float)Config::Steer::TORQUE_MAX /
                        (float)Config::Steer::ANGLE_MAX);
    torque = (int16_t)(k * (float)(core1_input_report.steer));

    mfMotor.setTorque(torque);

#ifdef PHYSICAL_INPUT_DEBUG_ENABLE
    // 5. デバッグ出力 (1秒周期)
    static IntervalTrigger_m debugTrigger(1000);
    static bool debugInit = false;
    if (!debugInit) {
      debugTrigger.init();
      debugInit = true;
    }

    if (debugTrigger.hasExpired()) {
      Serial.printf(
          "[PHYS_INPUT] Steer:%d, Accel:%d, Brake:%d, Buttons:0x%04X, TO:%s\n",
          core1_input_report.steer, core1_input_report.accel,
          core1_input_report.brake, core1_input_report.buttons,
          timeout ? "YES" : "NO");
      // Serial.printf(
      //     "[PID_RECV] Mag:%d, Gain:%d, Type:0x%02X, Active:%s, Test:%s\n",
      //     core1_effects[0].magnitude, core1_effects[0].gain,
      //     core1_effects[0].type, core1_effects[0].active ? "ON" : "OFF",
      //     core1_effects[0].isCallBackTest ? "YES" : "NO");
    }
#endif
  }
}
