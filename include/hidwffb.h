/**
 * @file hidwffb.h
 * @brief HIDジョイスティックおよびFFB（PID）制御モジュール
 *        USB PID (Physical Interface Device) 仕様準拠
 */

#ifndef HIDWFFB_H
#define HIDWFFB_H

#include "pico/mutex.h"
#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <stdbool.h>
#include <stdint.h>

// --- 定数定義 ---
#define HID_FFB_REPORT_SIZE 64 ///< FFB受信用レポートのバッファサイズ
#define MAX_EFFECTS 10         ///< 同時管理するエフェクト数の上限

// --- Report IDs (Host -> Device: Output Reports) ---
#define HID_ID_SET_EFFECT 0x01    ///< Set Effect Report
#define HID_ID_SET_ENVELOPE 0x02  ///< Set Envelope Report
#define HID_ID_SET_CONDITION 0x03 ///< Set Condition Report (Spring/Damper等)
#define HID_ID_SET_PERIODIC 0x04  ///< Set Periodic Report (Sine/Square等)
#define HID_ID_SET_CONSTANT_FORCE 0x05 ///< Set Constant Force Report
#define HID_ID_SET_RAMP_FORCE 0x06     ///< Set Ramp Force Report
#define HID_ID_CUSTOM_FORCE_DATA 0x07   ///< Custom Force Data Report
#define HID_ID_DOWNLOAD_FORCE_SAMPLE 0x08///< Download Force Sample
#define HID_ID_EFFECT_OPERATION 0x0A   ///< Effect Operation (Start/Stop)
#define HID_ID_PID_BLOCK_FREE 0x0B     ///< PID Block Free Report
#define HID_ID_DEVICE_CONTROL 0x0C     ///< PID Device Control
#define HID_ID_DEVICE_GAIN 0x0D        ///< Device Gain Report
#define HID_ID_SET_CUSTOM_FORCE 0x0E    ///< Set Custom Force Report

// --- Report IDs (Device -> Host: Input Reports) ---
#define HID_ID_GAMEPAD_INPUT 0x01 ///< Joystick Input Report
#define HID_ID_PID_STATE 0x02     ///< PID State Report

// --- Report IDs (Feature Reports) ---
#define HID_ID_CREATE_NEW_EFFECT 0x05 ///< Create New Effect (Feature, HostRead)
#define HID_ID_PID_BLOCK_LOAD 0x06    ///< PID Block Load Report (Feature)
#define HID_ID_PID_POOL 0x07          ///< PID Pool Report (Feature)

// --- Effect Types (ET) ---
#define HID_ET_CONSTANT 0x01 ///< ET Constant Force
#define HID_ET_RAMP 0x02     ///< ET Ramp
#define HID_ET_SQUARE 0x03   ///< ET Square
#define HID_ET_SINE 0x04     ///< ET Sine
#define HID_ET_TRIANGLE 0x05 ///< ET Triangle
#define HID_ET_SAW_UP 0x06   ///< ET Sawtooth Up
#define HID_ET_SAW_DOWN 0x07 ///< ET Sawtooth Down
#define HID_ET_SPRING 0x08   ///< ET Spring (PIDディスクリプタ列挙番号)
#define HID_ET_DAMPER 0x09   ///< ET Damper
#define HID_ET_INERTIA 0x0A  ///< ET Inertia
#define HID_ET_FRICTION 0x0B ///< ET Friction
#define HID_ET_CUSTOM 0x0C ///< ET Custom Force Data

// Effect Type の Usage値 (0x0F Usage Page)
#define HID_ET_USAGE_CONSTANT 0x26
#define HID_ET_USAGE_RAMP 0x27
#define HID_ET_USAGE_SQUARE 0x30
#define HID_ET_USAGE_SINE 0x31
#define HID_ET_USAGE_TRIANGLE 0x32
#define HID_ET_USAGE_SAW_UP 0x33
#define HID_ET_USAGE_SAW_DOWN 0x34
#define HID_ET_USAGE_SPRING 0x40
#define HID_ET_USAGE_DAMPER 0x41
#define HID_ET_USAGE_INERTIA 0x42
#define HID_ET_USAGE_FRICTION 0x43
#define HID_ET_USAGE_CUSTOM 0x28

// --- Effect Operations ---
#define HID_OP_START 0x01
#define HID_OP_SOLO 0x02
#define HID_OP_STOP 0x03

// --- PID Device Control ---
#define HID_DC_ENABLE_ACTUATORS 0x01
#define HID_DC_DISABLE_ACTUATORS 0x02
#define HID_DC_STOP_ALL_EFFECTS 0x03
#define HID_DC_DEVICE_RESET 0x04
#define HID_DC_DEVICE_PAUSE 0x05
#define HID_DC_DEVICE_CONTINUE 0x06

// --- PID Block Load Status ---
#define HID_BLOCK_LOAD_SUCCESS 0x01
#define HID_BLOCK_LOAD_FULL 0x02
#define HID_BLOCK_LOAD_ERROR 0x03

/**
 * @brief ジョイスティック入力レポート構造体 (16bit軸 x 3, Button x 16)
 * Core 1 -> Core 0 (物理入力/レポート用)
 */
typedef struct {
  uint8_t reportId;               ///< = 0x0E
  uint8_t effectBlockIndex;
  uint8_t sampleCount;
  uint16_t samplePeriod;
} __attribute__((packed)) USB_FFB_Report_SetCustomForce_t;

typedef struct {
  int16_t steer;    ///< ハンドル (X軸: 0x30) 符号付き16bit [-32767 to 32767]
  int16_t dummy_y;
  uint16_t accel;   ///< アクセル (Z軸: 0x32) 符号なし16bit [0 to 65535]
  uint16_t brake;   ///< ブレーキ (Rz軸: 0x35) 符号なし16bit [0 to 65535]
  uint16_t buttons; ///< ボタン (16ビット分) [1:Pressed, 0:Released]
} custom_gamepad_report_t;

// --- PID State Report (Device -> Host, Input ID: 0x02) ---
typedef struct {
  uint8_t reportId; ///< = 0x02
  uint8_t status;   ///< bits[4:0]: PIDフラグ, bits[7:5]: unused
  uint8_t
      effectState; ///< bit[0]: Effect Playing, bits[7:1]: Effect Block Index
} __attribute__((packed)) USB_PIDState_Report_t;

// --- Output Report 構造体定義 ---

/**
 * @brief Set Effect Output Report (ID: 0x01)
 */
typedef struct {
  uint8_t reportId;               ///< = 0x01
  uint8_t effectBlockIndex;       ///< 1..40
  uint8_t effectType;             ///< 1..12
  uint16_t duration;              ///< 0..32767 ms
  uint16_t triggerRepeatInterval; ///< 0..32767 ms
  uint16_t samplePeriod;          ///< 0..32767 ms
  uint8_t gain;                   ///< 0..255
  uint8_t triggerButton;          ///< 0..8
  uint8_t enableAxis;             ///< bits: 0=X, 1=Y, 2=DirectionEnable
  uint8_t directionX;             ///< 0..255 (360deg)
  uint8_t directionY;             ///< 0..255 (360deg)
} __attribute__((packed)) USB_FFB_Report_SetEffect_t;

/**
 * @brief Set Envelope Output Report (ID: 0x02)
 */
typedef struct {
  uint8_t reportId;               ///< = 0x02
  uint8_t effectBlockIndex;       ///< 1..40
  uint16_t attackLevel;
  uint16_t fadeLevel;
  uint32_t attackTime;            ///< ms
  uint32_t fadeTime;              ///< ms
} __attribute__((packed)) USB_FFB_Report_SetEnvelope_t;

/**
 * @brief Set Condition Output Report (ID: 0x03)
 *        Spring / Damper / Inertia / Friction 共通
 */
typedef struct {
  uint8_t reportId;               ///< = 0x03
  uint8_t effectBlockIndex;       ///< 1..40
  uint8_t parameterBlockOffset;   ///< bits: 0..3=parameterBlockOffset, 4..5=instance1, 6..7=instance2
  int16_t cpOffset;
  int16_t positiveCoefficient;
  int16_t negativeCoefficient;
  uint16_t positiveSaturation;
  uint16_t negativeSaturation;
  uint16_t deadBand;
} __attribute__((packed)) USB_FFB_Report_SetCondition_t;

/**
 * @brief Set Periodic Output Report (ID: 0x04)
 *        Sine / Square / Triangle / Sawtooth 共通
 */
typedef struct {
  uint8_t reportId;               ///< = 0x04
  uint8_t effectBlockIndex;       ///< 1..40
  uint16_t magnitude;
  int16_t offset;
  uint16_t phase;                 ///< 0..35999
  uint32_t period;                ///< 0..32767
} __attribute__((packed)) USB_FFB_Report_SetPeriodic_t;

/**
 * @brief Set Constant Force Output Report (ID: 0x05)
 */
typedef struct {
  uint8_t reportId;               ///< = 0x05
  uint8_t effectBlockIndex;       ///< 1..40
  int16_t magnitude;              ///< -10000..10000
} __attribute__((packed)) USB_FFB_Report_SetConstantForce_t;

/**
 * @brief Set Effect Operation Output Report (ID: 0x0A)
 */
typedef struct {
  uint8_t reportId;         ///< = 0x0A
  uint8_t effectBlockIndex; ///< 1..40
  uint8_t operation;        ///< 1: Start, 2: StartSolo, 3: Stop
  uint8_t loopCount;        ///< 0..255
} __attribute__((packed)) USB_FFB_Report_EffectOperation_t;

/**
 * @brief PID Block Free Output Report (ID: 0x0B)
 */
typedef struct {
  uint8_t reportId;         ///< = 0x0B
  uint8_t effectBlockIndex; ///< 1..40
} __attribute__((packed)) USB_FFB_Report_PIDBlockFree_t;

/**
 * @brief PID Device Control Output Report (ID: 0x0C)
 */
typedef struct {
  uint8_t reportId; ///< = 0x0C
  uint8_t control;  ///< 1=EnableAct, 2=DisableAct, 3=StopAll, 4=Reset, 5=Pause,
                    ///< 6=Continue
} __attribute__((packed)) USB_FFB_Report_DeviceControl_t;

/**
 * @brief Device Gain Output Report (ID: 0x0D)
 */
typedef struct {
  uint8_t reportId;   ///< = 0x0D
  uint8_t deviceGain; ///< 0..255
} __attribute__((packed)) USB_FFB_Report_DeviceGain_t;

// --- Feature Report 構造体定義 ---

/**
 * @brief Create New Effect Feature Report (ID: 0x05, Feature - Host Read)
 */
typedef struct {
  uint8_t reportId;   ///< = 0x05
  uint8_t effectType; ///< 列挙値 1..11
  uint16_t byteCount; ///< エフェクトデータサイズ (10bit実効 + 6bit padding)
} __attribute__((packed)) USB_FFB_Feature_CreateNewEffect_t;

/**
 * @brief PID Block Load Feature Report (ID: 0x06, Feature - Device Response)
 */
typedef struct {
  uint8_t reportId;          ///< = 0x06
  uint8_t effectBlockIndex;  ///< 割当インデックス 1..40
  uint8_t loadStatus;        ///< 1=Success, 2=Full, 3=Error
  uint16_t ramPoolAvailable; ///< 残りRAM (ダミー値でよい)
} __attribute__((packed)) USB_FFB_Feature_PIDBlockLoad_t;

/**
 * @brief PID Pool Feature Report (ID: 0x07, Feature - Device Info)
 */
typedef struct {
  uint8_t reportId;              ///< = 0x07
  uint16_t ramPoolSize;          ///< RAM size
  uint8_t simultaneousEffects;   ///< max simultaneous effects count
  uint8_t memoryManagement;      ///< bit0=deviceManagedPool, bit1=sharedParamBlocks
} __attribute__((packed)) USB_FFB_Feature_PIDPool_t;

/**
 * @brief パースされたPIDデータの要約（デバッグ出力用）
 */
typedef struct {
  uint8_t lastReportId;
  bool isConstantForce;
  int16_t magnitude;
  uint8_t deviceGain;
  uint8_t operation;        ///< 0x0A の operation
  uint8_t effectBlockIndex; ///< 0x0A の effectBlockIndex
  bool updated;
} pid_debug_info_t;

// Core間通信用構造体
// Core 0 -> Core 1 (FFB命令)
typedef struct {
  int16_t magnitude;
  int16_t gain; ///< 0x01 で設定される Gain
  uint8_t type; ///< HID_ET_* の値
  // Condition 系パラメータ
  int16_t cpOffset;
  int16_t positiveCoeff;
  // Periodic 系パラメータ
  uint16_t periodicMagnitude;
  int16_t periodicOffset;
  uint8_t periodicPhase;
  uint16_t periodicPeriod;
  volatile bool active;
  volatile bool isCallBackTest; ///< テストモードフラグ
} FFB_Shared_State_t;

// --- 公開関数 ---

void hidwffb_begin(uint8_t poll_interval_ms = 1);
bool hidwffb_send_report(custom_gamepad_report_t *report);
bool hidwffb_sends_pid_state(uint8_t effectBlockIdx, bool playing);
bool hidwffb_is_mounted(void);
void hidwffb_wait_for_mount(void);
bool hidwffb_ready(void);
bool hidwffb_get_ffb_data(uint8_t *buffer);
void hidwffb_clear_ffb_flag(void);

void PID_ParseReport(uint8_t const *buffer, uint16_t bufsize);
bool hidwffb_get_pid_debug_info(pid_debug_info_t *info);

void ffb_shared_memory_init();
void ffb_core0_update_shared(pid_debug_info_t *info);
void ffb_core0_get_input_report(custom_gamepad_report_t *dest);
void ffb_core1_update_shared(custom_gamepad_report_t *new_input,
                             FFB_Shared_State_t *local_effects_dest);
void hidwffb_loopback_test_sync(custom_gamepad_report_t *new_input,
                                FFB_Shared_State_t *local_effects_dest);

#endif // HIDWFFB_H
