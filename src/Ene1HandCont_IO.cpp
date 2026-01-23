// filename: Ene1HandCont_IO.cpp
// Ene1HandController IO
//  6: S-Up Bottun   :DI Active Low
//  7: S-Down Bottun :DI Active Low
// A0: Break         :Analog PullUp Hi-R -> Lo-R
// A1: Accel         :Analog 1-4V
// fast mode(digitalPinFast + avdweb_AnalogReadFast)
// 取込    65us/回
// 平均化 160us/回
// normal(Arduino Standard digitalRead + analogRead)
// 取込   260us/回
// digitalPinFast: https://github.com/TheFidax/digitalPinFast
// avdweb_AnalogReadFast: https://github.com/avandalen/avdweb_AnalogReadFast
#include <Arduino.h>

// ボタンのチャタリング防止処理変数
const int ButtonTh = 4;
int cntBuffUp, cntBuffDown;
int CurBtnDownStatus, CurBtnUpStatus;

#define pinShiftUp 6
#define pinShiftDown 7
// digitalPinFast pinShiftUp(6);
// digitalPinFast pinShiftDown(7);
#define pinBreakIn A0
#define pinAccelIn A1

// アナログ移動平均用
#define NUMAVEBUFF 20
#define NUMAVE 8
int NumAdcSmp;            // ADC サンプリング数
int AdcAcc[NUMAVEBUFF];   // 移動平均用バッファ 最新が0
int AdcBreak[NUMAVEBUFF]; // 移動平均用バッファ 最新が0
int AveAcc, AveBreak;     // 移動平均値

void Setup_eHanConIO() {
  // define pin mode
  pinMode(pinShiftUp, INPUT_PULLUP);
  pinMode(pinShiftDown, INPUT_PULLUP);
  // pinShiftUp.pinModeFast(INPUT_PULLUP);
  // pinShiftDown.pinModeFast(INPUT_PULLUP);
  pinMode(pinBreakIn, INPUT);
  pinMode(pinAccelIn, INPUT);

  // Digital-In Filter
  cntBuffUp = ButtonTh;
  cntBuffDown = ButtonTh;
  CurBtnDownStatus = HIGH;
  CurBtnUpStatus = HIGH;

  // Adcの設定(バッファの初期化)
  for (int i = 0; i < NUMAVE; i++) {
    AdcAcc[i] = 0;
    AdcBreak[i] = 0;
  }
  NumAdcSmp = 0;
}
// UP ボタンの状態確認(デジタルフィルタ付)
int chkBtnUP() {
  // int iBtn = pinShiftUp.digitalReadFast();
  int iBtn = digitalRead(pinShiftUp);

  if (iBtn > 0) { // HIGH
    cntBuffUp++;
    if (cntBuffUp > ButtonTh) {
      cntBuffUp = ButtonTh;
      CurBtnUpStatus = HIGH;
    }
  } else { // iBtn ==0;
    cntBuffUp--;
    if (cntBuffUp < 0) {
      cntBuffUp = 0;
      CurBtnUpStatus = LOW;
    }
  }
  return CurBtnUpStatus;
}

// DOWN ボタンの状態確認(デジタルフィルタ付)
int chkBtnDown() {
  // int iBtn = pinShiftDown.digitalReadFast();
  int iBtn = digitalRead(pinShiftDown);
  if (iBtn > 0) { // HIGH
    cntBuffDown++;
    if (cntBuffDown > ButtonTh) {
      cntBuffDown = ButtonTh;
      CurBtnDownStatus = HIGH;
    }
  } else { // iBtn ==0;
    cntBuffDown--;
    if (cntBuffDown < 0) {
      cntBuffDown = 0;
      CurBtnDownStatus = LOW;
    }
  }
  return CurBtnDownStatus;
}

// ADC計測(変換は無し)
void getADCAccBreak() {
  // iにi-1を代入
  for (int i = NUMAVE; i > 0; i--) {
    AdcAcc[i] = AdcAcc[i - 1];
    AdcBreak[i] = AdcBreak[i - 1];
  }
  // 0に変換値を入れる
  // AdcAcc[0]   = analogReadFast(pinAccelIn);
  // AdcBreak[0] = analogReadFast(pinBreakIn);
  AdcAcc[0] = analogRead(pinAccelIn);
  AdcBreak[0] = analogRead(pinBreakIn);
  NumAdcSmp++;
}
// 平均化
void AveADCAccBreak() {
  if (NumAdcSmp > NUMAVE - 1) {
    NumAdcSmp = NUMAVE;
    int SumAdcAcc = 0;
    int SumAdcBreak = 0; // int : signed 32bit
    for (int i = 0; i < NUMAVE; i++) {
      SumAdcAcc = AdcAcc[i] + SumAdcAcc;
      SumAdcBreak = AdcBreak[i] + SumAdcBreak;
    }
    AveAcc = (int)((float)SumAdcAcc / (float)NUMAVE);
    AveBreak = (int)((float)SumAdcBreak / (float)NUMAVE);
  } else {
    AveAcc = 0;
    AveBreak = 0;
  }
  // サンプル取得数の上限処理
  if (NumAdcSmp > NUMAVEBUFF) {
    NumAdcSmp = NUMAVEBUFF;
  }
}
//
int getAccVal() { return AveAcc; }
int getBreakVal() {
  //  return AveBreak;
  return 1024 - AveBreak;
}