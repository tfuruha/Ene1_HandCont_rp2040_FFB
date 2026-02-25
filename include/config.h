#ifndef CONFIG_H
#define CONFIG_H

/**
 * @file config.h
 * @brief Ene1_HandCont_rp2040_FFB プロジェクト全体の設定ファイル
 */

#include <stdint.h>

namespace Config {

// ============================================================================
// ピンアサイン定義
// ============================================================================
namespace Pin {
inline constexpr uint8_t SPI_INT = 1; // SPI INT*
inline constexpr uint8_t SPI_SCK = 2; // SPI Clock
inline constexpr uint8_t SPI_TX = 3;  // SPI MOSI (TX)
inline constexpr uint8_t SPI_RX = 4;  // SPI MISO (RX)
inline constexpr uint8_t CAN_CS = 5;  // MCP2515 Chip Select

inline constexpr uint8_t SHIFT_UP = 14;   // シフトアップボタン (D6, Active Low)
inline constexpr uint8_t SHIFT_DOWN = 15; // シフトダウンボタン (D7, Active Low)

inline constexpr uint8_t ACCEL = 26; // ADC0 (A0) - アクセルペダル
inline constexpr uint8_t BRAKE = 28; // ADC2 (A2) - ブレーキペダル
} // namespace Pin

// ============================================================================
// CAN通信 / モーター設定
// ============================================================================
namespace Steer {
inline constexpr uint32_t CAN_ID = 0x141; // MF4015のCAN ID
inline constexpr uint8_t DEVICE_ID = 1;   // MF4015デバイスID

// エンコーダ分解能: 16bit(65536カウント) / 360度
inline constexpr double ENCODER_COUNTS_PER_DEG = 65536.0 / 360.0;

// 有効角度範囲 (0を中心とした片側角度, 単位: deg)
inline constexpr double ANGLE_RANGE_DEG = 45.0;

// 有効角度範囲 (エンコーダカウント値, constexpr算出)
inline constexpr int32_t ANGLE_MIN =
    -(int32_t)(ANGLE_RANGE_DEG * ENCODER_COUNTS_PER_DEG);
inline constexpr int32_t ANGLE_MAX =
    (int32_t)(ANGLE_RANGE_DEG * ENCODER_COUNTS_PER_DEG);
inline constexpr uint16_t ANGLE_CENTER = 0x7FFF; // センター位置

inline constexpr int16_t TORQUE_MIN = -2048; // トルク電流指令値 最小
inline constexpr int16_t TORQUE_MAX = 2048;  // トルク電流指令値 最大

// PhysicalEffect パラメータ（初期テスト用）
inline constexpr float SPRING_COEFF =
    0.05f * (float)TORQUE_MAX / (float)ANGLE_MAX;
// バネ係数
inline constexpr float FRICTION_COEFF = 0.0f;  // 摩擦係数（現状無効）
inline constexpr float DAMPER_COEFF = 0.0005f; // ダンパー係数（現状無効）
inline constexpr float INERTIA_COEFF = 0.0f;   // 慣性係数（現状無効）
} // namespace Steer

// ============================================================================
// タイミング設定
// ============================================================================
namespace Time {
// ADC/DI サンプリング周期 (us)
inline constexpr uint32_t SAMPLING_INTERVAL_US = 250;
// ステアリング制御インターバル(us)
inline constexpr uint32_t STEAR_CONT_INTERVAL_US = 1000;
// HIDレポート送信周期 (ms) デバイス側の送信間隔(最大値)
inline constexpr uint32_t HIDREPO_INTERVAL_MS = 1;
// USB HID ポーリング周期 (ms) ホスト側のポーリング間隔(最大値)
inline constexpr uint32_t USB_POLL_INTERVAL_MS = 5;
} // namespace Time

// ============================================================================
// アナログ入力設定
// ============================================================================
namespace Adc {
inline constexpr uint16_t ACCEL_MIN = 180; // ADC最小値 (10bit基準)
inline constexpr uint16_t ACCEL_MAX = 890; // ADC最大値 (10bit基準)

inline constexpr uint32_t ACCEL_HID_MIN = 0;
inline constexpr uint32_t ACCEL_HID_MAX = 65535;

inline constexpr uint16_t BRAKE_MIN = 100; // ADC最小値 (10bit基準)
inline constexpr uint16_t BRAKE_MAX = 820; // ADC最大値 (10bit基準)

inline constexpr uint32_t BRAKE_HID_MIN = 0;
inline constexpr uint32_t BRAKE_HID_MAX = 65535;

inline constexpr uint8_t BUFFER_SIZE = 12;  // 移動平均バッファサイズ
inline constexpr uint8_t AVERAGE_COUNT = 8; // 移動平均サンプル数
} // namespace Adc

// ============================================================================
// デジタル入力設定
// ============================================================================
namespace Input {
inline constexpr uint8_t BUTTON_DEBOUNCE_THRESHOLD = 4; // 連続一致サンプル数
}

// ============================================================================
// USB HID設定
// ============================================================================
namespace Hid {
inline constexpr uint8_t BUTTON_COUNT = 2; // ボタン数
inline constexpr uint8_t AXIS_COUNT = 3;   // 軸数
} // namespace Hid

// ============================================================================
// システム設定
// ============================================================================
inline constexpr uint32_t SERIAL_BAUDRATE = 115200;

} // namespace Config

// プリプロセッサフラグは明示的に残す
#define BRAKE_INVERT true           // ブレーキの入力値を反転させる
#define PHYSICAL_INPUT_DEBUG_ENABLE // 物理入力のデバッグを有効にする

#endif // CONFIG_H
