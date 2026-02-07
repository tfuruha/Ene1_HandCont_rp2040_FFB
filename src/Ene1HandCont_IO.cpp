/**
 * @file Ene1HandCont_IO.cpp
 * @brief Ene1ハンドルコントローラ 入出力処理
 * @date 2026-01-23
 *
 * デジタル入力（シフトスイッチ）とアナログ入力（アクセル・ブレーキ）の
 * 読み取り処理を提供します。
 *
 * ## 機能
 * - シフトスイッチのチャタリング防止処理
 * - アクセル・ブレーキの移動平均フィルタ処理
 *
 * ## パフォーマンス参考値
 * - 標準モード(digitalRead + analogRead):
 *   - 取込: 260us/回
 * - 高速モード(digitalPinFast + avdweb_AnalogReadFast):
 *   - 取込: 65us/回
 *   - 平均化: 160us/回
 *
 * @see https://github.com/TheFidax/digitalPinFast
 * @see https://github.com/avandalen/avdweb_AnalogReadFast
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

// 変換関数：ブレーキ用（反転処理）
int transformBrake(int val) { return 1024 - val; }

// 変換関数：アクセル用（そのまま）
int transformAccel(int val) { return val; }

// AD入力チャンネルインスタンス
ADInputChannel adAccel(PIN_ACCEL, ADC_AVERAGE_COUNT, transformAccel);
ADInputChannel adBrake(PIN_BRAKE, ADC_AVERAGE_COUNT, transformBrake);

/**
 * @brief 入出力モジュールの初期化
 *
 * ピンモードの設定、チャタリング防止フィルタの初期化、
 * 移動平均バッファの初期化を行います。
 */
void Setup_eHanConIO() {
  // デジタル入力の初期化
  diKeyUp.Init();
  diKeyDown.Init();

  // AD入力チャンネルの初期化
  adAccel.Init();
  adBrake.Init();
}
/**
 * @brief シフトアップボタンの状態確認（デジタルフィルタ付）
 *
 * チャタリング防止のため、連続してBUTTON_DEBOUNCE_THRESHOLD回
 * 同じ状態が続いた場合のみ状態を変更します。
 *
 * @return HIGH: ボタン非押下, LOW: ボタン押下
 */
int chkBtnUP() { return diKeyUp.update(); }

/**
 * @brief シフトダウンボタンの状態確認（デジタルフィルタ付）
 *
 * チャタリング防止のため、連続してBUTTON_DEBOUNCE_THRESHOLD回
 * 同じ状態が続いた場合のみ状態を変更します。
 *
 * @return HIGH: ボタン非押下, LOW: ボタン押下
 */
int chkBtnDown() { return diKeyDown.update(); }

/**
 * @brief ADC計測（アクセル・ブレーキ）
 *
 * アクセルとブレーキのADC値を読み取り、移動平均バッファに格納します。
 * バッファは最新値が[0]になるようにシフトされます。
 */
void getADCAccBreak() {
  adAccel.getadc();
  adBrake.getadc();
}
/**
 * @brief ADC値の移動平均計算
 *
 * 移動平均バッファに格納されたADC値の平均を計算します。
 * サンプル数が不足している場合は0を返します。
 */
void AveADCAccBreak() {
  // クラス化により、個別の取得タイミング（getvalue）で平均計算を行うため、
  // ここでの一括計算は不要になりました。
  // 既存のメインループとの互換性のため、関数体は空（または将来の拡張用）にします。
}
/**
 * @brief アクセルペダルの平均値を取得
 *
 * @return アクセルペダルの移動平均値
 */
int getAccVal() { return adAccel.getvalue(); }

/**
 * @brief ブレーキペダルの平均値を取得
 *
 * @return ブレーキペダルの移動平均値（反転処理済み）
 */
int getBreakVal() { return adBrake.getvalue(); }