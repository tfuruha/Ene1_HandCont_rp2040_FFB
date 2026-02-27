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
#include "control.h"
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

// hidwffb.cpp で管理されているグローバルゲイン (Device Gain Report 0x0D で更新)
extern volatile uint8_t shared_global_gain;

// --- 周期管理 ---
static IntervalTrigger_m hidReportTrigger(Config::Time::HIDREPO_INTERVAL_MS);
static IntervalTrigger_u stearContTrigger(Config::Time::STEAR_CONT_INTERVAL_US);
static IntervalTrigger_u sampleTrigger(Config::Time::SAMPLING_INTERVAL_US);

// --- Core間通信用 (hidwffb.h に実体があるが main.cpp でも管理が必要なフラグ等)
// ---
static custom_gamepad_report_t core1_input_report = {0};
static FFB_Shared_State_t core1_effects[MAX_EFFECTS];

// --- 物理エフェクト (Core 1 で使用) ---
static PhysicalEffect steerEffect(Config::Steer::FRICTION_COEFF,
                                  Config::Steer::SPRING_COEFF,
                                  Config::Steer::DAMPER_COEFF,
                                  Config::Steer::INERTIA_COEFF,
                                  Config::Time::STEAR_CONT_INTERVAL_US);

void setup() {
  // Serial.begin(Config::SERIAL_BAUDRATE);

  // 設定管理の初期化と復元
  if (ConfigManager::begin()) {
    ConfigManager::loadConfig();
  }

  // HIDモジュールの初期化
  hidwffb_begin(Config::Time::USB_POLL_INTERVAL_MS);

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
    if (hidwffb_ready()) { // HID送信バッファが空いている場合
      custom_gamepad_report_t report = {0};
      ffb_core0_get_input_report(&report); // 共有メモリから入力を取得
      hidwffb_send_report(&report);        // HID送信
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
// Core 1 ユーティリティ関数
// ============================================================================

/**
 * @brief PID magnitude をモータートルク指令値にスケーリングする
 *
 * DirectInput PID の慣習値 ±10000 を TORQUE_MIN～TORQUE_MAX にマップし、
 * deviceGain (0..255, 255=100%) を全体ゲインとして乗算する。
 *
 * @param magnitude  PIDから受け取った生値 (DirectInput慣習: ±10000)
 * @param deviceGain グローバルゲイン (0..255)
 * @return           モーター指令値 (TORQUE_MIN..TORQUE_MAX)
 */
static int16_t scaleMagnitudeToTorque(int16_t magnitude, uint8_t deviceGain) {
  static constexpr int32_t MAG_MAX = 10000; // DirectInput 慣習値
  // ±MAG_MAX にクランプ
  int32_t clamped = (int32_t)magnitude;
  if (clamped > MAG_MAX)
    clamped = MAG_MAX;
  if (clamped < -MAG_MAX)
    clamped = -MAG_MAX;
  // deviceGain 適用 (0..255 → 0..1.0 相当) ＆ TORQUE_MAX へスケーリング
  // 整数2段除算による桁落ちを防ぐためfloatで演算し、四捨五入してからint16_tに変換
  float gained = (float)clamped * (float)deviceGain / 255.0f;
  return (int16_t)(gained * (float)Config::Steer::TORQUE_MAX / (float)MAG_MAX +
                   0.5f);
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
  // 1. INT検出トリガ (MCP2515 受信完了イベント)
  //    setTorque()の応答でMF4015から角位置付きデータが返ってくる
  if (canWrapper.available()) {
    uint32_t id;
    uint8_t len;
    uint8_t data[8];
    if (canWrapper.readFrame(id, len, data)) {
      // パース（角位置含むステータス更新）
      mfMotor.parseFrame(id, len, data);
    }

    // 角位置取得（parseFrameで更新済み）
    core1_input_report.steer = mfMotor.getSteerValue();

    // AD変換値・スイッチ入力の最新値（フィルタ処理済み）を取得
    core1_input_report.accel = (int16_t)adAccel.getvalue();
    core1_input_report.brake = (int16_t)adBrake.getvalue();

    uint16_t btnMask = 0;
    if (diKeyUp.getState() == LOW)
      btnMask |= (1 << 0);
    if (diKeyDown.getState() == LOW)
      btnMask |= (1 << 1);
    core1_input_report.buttons = btnMask;

    // トルク補正用の物理量計算
    steerEffect.update(core1_input_report.steer);
  }

  // 2. ステアリング制御タイマートリガ (1000us周期)
  //    トルク指令送信 → CAN送信 → MF4015が応答 → INT発生
  if (stearContTrigger.hasExpired()) {
    sharedData.lastCore1Micros = micros();
    sharedData.core1LoopCount++;

    // 共有メモリから FFB 命令を取得し、入力レポートをCore0へ渡す
    ffb_core1_update_shared(&core1_input_report, core1_effects);

    // トルク演算と送信
    // PID magnitude (DirectInput ±10000) を TORQUE_MIN～TORQUE_MAX
    // にスケーリング
    int16_t torque =
        scaleMagnitudeToTorque(core1_effects[0].magnitude, shared_global_gain);
    sharedData.targetTorque = torque;
    // torque += steerEffect.getEffect(); // テスト用に物理エフェクトのみ
    mfMotor.setTorque(torque);
  }

  // 3. ADC/DI サンプリング (250us周期)
  //    生値の取得のみ行い、物理量変換はINT検出時に実行する
  if (sampleTrigger.hasExpired()) {
    adBrake.getadc();
    adAccel.getadc();
    diKeyUp.update();
    diKeyDown.update();
  }

#ifdef PHYSICAL_INPUT_DEBUG_ENABLE
  // 4. デバッグ出力 (1秒周期)
  static IntervalTrigger_m debugTrigger(1000);
  static bool debugInit = false;
  if (!debugInit) {
    debugTrigger.init();
    debugInit = true;
  }

  if (debugTrigger.hasExpired()) {
    Serial.printf("[PHYS_INPUT] Steer:%d, Accel:%d, Brake:%d, Buttons:0x%04X\n",
                  core1_input_report.steer, core1_input_report.accel,
                  core1_input_report.brake, core1_input_report.buttons);
    Serial.printf("[PID] effects[0].magnitude:%d\n",
                  core1_effects[0].magnitude);
    //  Serial.printf("[RAW_ADC] Accel:%d, Brake:%d\n", adAccel.getRawLatest(),
    //                adBrake.getRawLatest());
  }
#endif
}
