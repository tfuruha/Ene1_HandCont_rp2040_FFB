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

#include "config.h"
#include <Arduino.h>

// ============================================================================
// ボタン入力処理用変数
// ============================================================================

// チャタリング防止用カウンタ
int cntBuffUp, cntBuffDown;

// 現在のボタン状態
int CurBtnDownStatus, CurBtnUpStatus;

// ============================================================================
// アナログ入力処理用変数
// ============================================================================

// ADCサンプリング数カウンタ
int NumAdcSmp;

// 移動平均用バッファ（最新が[0]）
int AdcAcc[ADC_BUFFER_SIZE];
int AdcBreak[ADC_BUFFER_SIZE];

// 移動平均値
int AveAcc, AveBreak;

/**
 * @brief 入出力モジュールの初期化
 *
 * ピンモードの設定、チャタリング防止フィルタの初期化、
 * 移動平均バッファの初期化を行います。
 */
void Setup_eHanConIO() {
  // ピンモード設定
  pinMode(PIN_SHIFT_UP, INPUT_PULLUP);
  pinMode(PIN_SHIFT_DOWN, INPUT_PULLUP);
  pinMode(PIN_BRAKE, INPUT);
  pinMode(PIN_ACCEL, INPUT);

  // デジタル入力フィルタの初期化
  cntBuffUp = BUTTON_DEBOUNCE_THRESHOLD;
  cntBuffDown = BUTTON_DEBOUNCE_THRESHOLD;
  CurBtnDownStatus = HIGH;
  CurBtnUpStatus = HIGH;

  // ADCバッファの初期化
  for (int i = 0; i < ADC_AVERAGE_COUNT; i++) {
    AdcAcc[i] = 0;
    AdcBreak[i] = 0;
  }
  NumAdcSmp = 0;
}
/**
 * @brief シフトアップボタンの状態確認（デジタルフィルタ付）
 *
 * チャタリング防止のため、連続してBUTTON_DEBOUNCE_THRESHOLD回
 * 同じ状態が続いた場合のみ状態を変更します。
 *
 * @return HIGH: ボタン非押下, LOW: ボタン押下
 */
int chkBtnUP() {
  int iBtn = digitalRead(PIN_SHIFT_UP);

  if (iBtn > 0) { // HIGH (ボタン非押下)
    cntBuffUp++;
    if (cntBuffUp > BUTTON_DEBOUNCE_THRESHOLD) {
      cntBuffUp = BUTTON_DEBOUNCE_THRESHOLD;
      CurBtnUpStatus = HIGH;
    }
  } else { // LOW (ボタン押下)
    cntBuffUp--;
    if (cntBuffUp < 0) {
      cntBuffUp = 0;
      CurBtnUpStatus = LOW;
    }
  }
  return CurBtnUpStatus;
}

/**
 * @brief シフトダウンボタンの状態確認（デジタルフィルタ付）
 *
 * チャタリング防止のため、連続してBUTTON_DEBOUNCE_THRESHOLD回
 * 同じ状態が続いた場合のみ状態を変更します。
 *
 * @return HIGH: ボタン非押下, LOW: ボタン押下
 */
int chkBtnDown() {
  int iBtn = digitalRead(PIN_SHIFT_DOWN);

  if (iBtn > 0) { // HIGH (ボタン非押下)
    cntBuffDown++;
    if (cntBuffDown > BUTTON_DEBOUNCE_THRESHOLD) {
      cntBuffDown = BUTTON_DEBOUNCE_THRESHOLD;
      CurBtnDownStatus = HIGH;
    }
  } else { // LOW (ボタン押下)
    cntBuffDown--;
    if (cntBuffDown < 0) {
      cntBuffDown = 0;
      CurBtnDownStatus = LOW;
    }
  }
  return CurBtnDownStatus;
}

/**
 * @brief ADC計測（アクセル・ブレーキ）
 *
 * アクセルとブレーキのADC値を読み取り、移動平均バッファに格納します。
 * バッファは最新値が[0]になるようにシフトされます。
 */
void getADCAccBreak() {
  // バッファをシフト（i に i-1 を代入）
  for (int i = ADC_AVERAGE_COUNT; i > 0; i--) {
    AdcAcc[i] = AdcAcc[i - 1];
    AdcBreak[i] = AdcBreak[i - 1];
  }

  // 最新値を[0]に格納
  AdcAcc[0] = analogRead(PIN_ACCEL);
  AdcBreak[0] = analogRead(PIN_BRAKE);
  NumAdcSmp++;
}
/**
 * @brief ADC値の移動平均計算
 *
 * 移動平均バッファに格納されたADC値の平均を計算します。
 * サンプル数が不足している場合は0を返します。
 */
void AveADCAccBreak() {
  if (NumAdcSmp > ADC_AVERAGE_COUNT - 1) {
    NumAdcSmp = ADC_AVERAGE_COUNT;
    int SumAdcAcc = 0;
    int SumAdcBreak = 0;

    // 移動平均の計算
    for (int i = 0; i < ADC_AVERAGE_COUNT; i++) {
      SumAdcAcc += AdcAcc[i];
      SumAdcBreak += AdcBreak[i];
    }

    AveAcc = (int)((float)SumAdcAcc / (float)ADC_AVERAGE_COUNT);
    AveBreak = (int)((float)SumAdcBreak / (float)ADC_AVERAGE_COUNT);
  } else {
    // サンプル数不足の場合は0を返す
    AveAcc = 0;
    AveBreak = 0;
  }

  // サンプル取得数の上限処理
  if (NumAdcSmp > ADC_BUFFER_SIZE) {
    NumAdcSmp = ADC_BUFFER_SIZE;
  }
}
/**
 * @brief アクセルペダルの平均値を取得
 *
 * @return アクセルペダルの移動平均値
 */
int getAccVal() { return AveAcc; }

/**
 * @brief ブレーキペダルの平均値を取得
 *
 * ブレーキは踏み込むとADC値が下がる特性のため、
 * 1024から減算して反転処理を行います。
 *
 * @return ブレーキペダルの移動平均値（反転処理済み）
 */
int getBreakVal() { return 1024 - AveBreak; }