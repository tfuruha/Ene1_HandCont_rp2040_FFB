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
  // map()は入力範囲外の値を外挿するため、先にADC範囲にクランプする
  val = constrain(val, Config::Adc::BRAKE_MIN, Config::Adc::BRAKE_MAX);
#ifdef BRAKE_INVERT
  // BRAKE_INVERT: 踏み込みで値が増えるように反転
  return map(val, Config::Adc::BRAKE_MIN, Config::Adc::BRAKE_MAX,
             Config::Adc::BRAKE_HID_MAX, Config::Adc::BRAKE_HID_MIN);
#else
  return map(val, Config::Adc::BRAKE_MIN, Config::Adc::BRAKE_MAX,
             Config::Adc::BRAKE_HID_MIN, Config::Adc::BRAKE_HID_MAX);
#endif // BRAKE_INVERT
}

// 変換関数：アクセル用（範囲変換）
int transformAccel(int val) {
  // map()は入力範囲外の値を外挿するため、先にADC範囲にクランプする
  val = constrain(val, Config::Adc::ACCEL_MIN, Config::Adc::ACCEL_MAX);
  return map(val, Config::Adc::ACCEL_MIN, Config::Adc::ACCEL_MAX,
             Config::Adc::ACCEL_HID_MIN, Config::Adc::ACCEL_HID_MAX);
}

// AD入力チャンネルインスタンス
ADInputChannel adAccel(Config::Pin::ACCEL, Config::Adc::AVERAGE_COUNT,
                       transformAccel);
ADInputChannel adBrake(Config::Pin::BRAKE, Config::Adc::AVERAGE_COUNT,
                       transformBrake);