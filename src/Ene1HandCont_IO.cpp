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

DigitalInputChannel diKeyUp(PIN_SHIFT_UP, BUTTON_DEBOUNCE_THRESHOLD);
DigitalInputChannel diKeyDown(PIN_SHIFT_DOWN, BUTTON_DEBOUNCE_THRESHOLD);

// ============================================================================
// アナログ入力処理用クラス
// ============================================================================

// 変換関数：ブレーキ用（範囲変換と反転処理）
int transformBrake(int val) {
  // config.h の BRAKE_ADC_MIN/MAX 範囲を -32767..32767 に変換
  // BRAKE_INVERT が true の場合は反転（踏み込みで値が増えるように）
  // return val; // for debug
  int mapped = map(val, BRAKE_ADC_MIN, BRAKE_ADC_MAX, 32767, -32767);
  return constrain(mapped, -32767, 32767);
}

// 変換関数：アクセル用（範囲変換）
int transformAccel(int val) {
  // config.h の ACCEL_ADC_MIN/MAX 範囲を -32767..32767 に変換
  // return val; // for debug
  int mapped = map(val, ACCEL_ADC_MIN, ACCEL_ADC_MAX, -32767, 32767);
  return constrain(mapped, -32767, 32767);
}

// AD入力チャンネルインスタンス
ADInputChannel adAccel(PIN_ACCEL, ADC_AVERAGE_COUNT, transformAccel);
ADInputChannel adBrake(PIN_BRAKE, ADC_AVERAGE_COUNT, transformBrake);