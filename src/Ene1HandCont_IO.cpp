/**
 * @file Ene1HandCont_IO.cpp
 * @brief Ene1ハンドルコントローラ 入出力処理
 * @date 2026-02-07
 *
 * デジタル入力（シフトスイッチ）とアナログ入力（アクセル・ブレーキ）の
 * インスタンスの定義と変換関数を記述
 */

#include "ADInput.h"
#include "DigitalInput.h"
#include "config.h"
#include <Arduino.h>

// ============================================================================
// ボタン入力処理用クラス
// ============================================================================

DigitalInputChannel diKeyUp(Config::Pin::SHIFT_UP,
                            Config::Input::BUTTON_DEBOUNCE_THRESHOLD);
DigitalInputChannel diKeyDown(Config::Pin::SHIFT_DOWN,
                              Config::Input::BUTTON_DEBOUNCE_THRESHOLD);

// ============================================================================
// アナログ入力処理用クラス
// ============================================================================

// 変換関数：ブレーキ用（範囲変換と反転処理）
int transformBrake(int val) {
  // config.h の BRAKE_ADC_MIN/MAX 範囲を -32767..32767 に変換
  // BRAKE_INVERT が true の場合は反転（踏み込みで値が増えるように）
  // return val; // for debug
#ifdef BRAKE_INVERT
  int mapped = map(val, Config::Adc::BRAKE_MIN, Config::Adc::BRAKE_MAX,
                   Config::Adc::BRAKE_HID_MAX, Config::Adc::BRAKE_HID_MIN);
#else
  int mapped = map(val, Config::Adc::BRAKE_MIN, Config::Adc::BRAKE_MAX,
                   Config::Adc::BRAKE_HID_MIN, Config::Adc::BRAKE_HID_MAX);
#endif // BRAKE_INVERT
  return constrain(mapped, Config::Adc::BRAKE_HID_MIN,
                   Config::Adc::BRAKE_HID_MAX);
}

// 変換関数：アクセル用（範囲変換）
int transformAccel(int val) {
  // config.h の ACCEL_ADC_MIN/MAX 範囲を -32767..32767 に変換
  // return val; // for debug
  int mapped = map(val, Config::Adc::ACCEL_MIN, Config::Adc::ACCEL_MAX,
                   Config::Adc::ACCEL_HID_MIN, Config::Adc::ACCEL_HID_MAX);
  return constrain(mapped, Config::Adc::ACCEL_HID_MIN,
                   Config::Adc::ACCEL_HID_MAX);
}

// AD入力チャンネルインスタンス
ADInputChannel adAccel(Config::Pin::ACCEL, Config::Adc::AVERAGE_COUNT,
                       transformAccel);
ADInputChannel adBrake(Config::Pin::BRAKE, Config::Adc::AVERAGE_COUNT,
                       transformBrake);