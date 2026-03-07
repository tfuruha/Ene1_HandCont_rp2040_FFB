/**
 * @file ffb_engine.cpp
 * @brief FFB (Force Feedback) エフェクト演算エンジン実装
 *
 * USB PID 仕様に基づく各エフェクト種別のトルク計算を実装する。
 * hidwffb (USB HID 層) とは独立し、純粋な演算ロジックのみを担う。
 */

#include "ffb_engine.h"
#include <Arduino.h> // micros()
#include <math.h>    // sinf(), floorf(), M_PI

// ============================================================================
// Condition 系エフェクト計算
// (Spring / Damper / Inertia / Friction)
// ============================================================================

int16_t ffb_calc_condition_force(const FFB_Shared_State_t &eff,
                                 int16_t steer_hid, float vel_norm,
                                 float accel_norm) {
  // HID 座標系 (-32767..32767) → PID 座標系 (-10000..10000) へ変換
  float pos = (float)steer_hid * 10000.0f / 32767.0f;
  float force = 0.0f;

  switch (eff.type) {

  case HID_ET_SPRING: {
    // 中心点からの変位
    float dx = pos - (float)eff.cpOffset;
    // 不感帯処理: 中心付近では力を発生させない
    float half_db = (float)eff.deadBand * 0.5f;
    if (dx > half_db)
      dx -= half_db;
    else if (dx < -half_db)
      dx += half_db;
    else
      dx = 0.0f;
    // 変位方向に応じた係数選択 (positive coeff → 求心力)
    float coeff =
        (dx >= 0.0f) ? (float)eff.positiveCoeff : (float)eff.negativeCoeff;
    force = -coeff * dx / 10000.0f;
    // 飽和処理
    float sat_pos = (float)eff.positiveSaturation;
    float sat_neg = (float)eff.negativeSaturation;
    if (force > sat_pos)
      force = sat_pos;
    if (force < -sat_neg)
      force = -sat_neg;
    break;
  }

  case HID_ET_DAMPER: {
    // 速度に比例した制動力
    float coeff = (vel_norm >= 0.0f) ? (float)eff.positiveCoeff
                                     : (float)eff.negativeCoeff;
    force = -coeff * vel_norm / 10000.0f;
    float sat_pos = (float)eff.positiveSaturation;
    float sat_neg = (float)eff.negativeSaturation;
    if (force > sat_pos)
      force = sat_pos;
    if (force < -sat_neg)
      force = -sat_neg;
    break;
  }

  case HID_ET_INERTIA: {
    // 加速度に比例した慣性力
    float coeff = (accel_norm >= 0.0f) ? (float)eff.positiveCoeff
                                       : (float)eff.negativeCoeff;
    force = -coeff * accel_norm / 10000.0f;
    float sat_pos = (float)eff.positiveSaturation;
    float sat_neg = (float)eff.negativeSaturation;
    if (force > sat_pos)
      force = sat_pos;
    if (force < -sat_neg)
      force = -sat_neg;
    break;
  }

  case HID_ET_FRICTION: {
    // 速度が閾値を超えた場合のみ方向固定の摩擦力
    float dir = (vel_norm > 10.0f) ? 1.0f : (vel_norm < -10.0f) ? -1.0f : 0.0f;
    float coeff =
        (dir >= 0.0f) ? (float)eff.positiveCoeff : (float)eff.negativeCoeff;
    force = -coeff * dir;
    float sat_pos = (float)eff.positiveSaturation;
    float sat_neg = (float)eff.negativeSaturation;
    if (force > sat_pos)
      force = sat_pos;
    if (force < -sat_neg)
      force = -sat_neg;
    break;
  }

  default:
    break;
  }

  // 四捨五入して int16_t に変換
  return (int16_t)(force + (force >= 0.0f ? 0.5f : -0.5f));
}

// ============================================================================
// Periodic 系エフェクト計算
// (Sine / Square / Triangle / Sawtooth Up / Sawtooth Down)
// ============================================================================

int16_t ffb_calc_periodic_force(const FFB_Shared_State_t &eff) {
  if (eff.periodicPeriod == 0) {
    // 周期未指定の場合は DC オフセットのみ返す
    return (int16_t)eff.periodicOffset;
  }

  uint32_t elapsed_us = micros() - eff.startTimeUs;
  uint32_t period_us = (uint32_t)eff.periodicPeriod * 1000UL; // ms → µs

  // 現在位相 phi [0.0, 1.0)
  float phi = (float)(elapsed_us % period_us) / (float)period_us;
  // 初期位相オフセット (0..35999 centideg → 0..1.0)
  phi += (float)eff.periodicPhase / 36000.0f;
  phi -= floorf(phi); // [0, 1) に正規化

  float waveform = 0.0f;
  switch (eff.type) {
  case HID_ET_SINE:
    waveform = sinf(phi * 2.0f * (float)M_PI);
    break;
  case HID_ET_SQUARE:
    waveform = (phi < 0.5f) ? 1.0f : -1.0f;
    break;
  case HID_ET_TRIANGLE:
    // phi: 0→0.25→0.75→1.0 で +1→ピーク→-1→0 の三角波
    if (phi < 0.25f)
      waveform = 4.0f * phi;
    else if (phi < 0.75f)
      waveform = 2.0f - 4.0f * phi;
    else
      waveform = 4.0f * phi - 4.0f;
    break;
  case HID_ET_SAW_UP:
    waveform = 2.0f * phi - 1.0f;
    break;
  case HID_ET_SAW_DOWN:
    waveform = 1.0f - 2.0f * phi;
    break;
  default:
    return 0;
  }

  // output = periodicOffset + periodicMagnitude × waveform
  float output =
      (float)eff.periodicOffset + (float)eff.periodicMagnitude * waveform;

  // ±10000 にクランプ
  if (output > 10000.0f)
    output = 10000.0f;
  if (output < -10000.0f)
    output = -10000.0f;

  return (int16_t)(output + (output >= 0.0f ? 0.5f : -0.5f));
}

// ============================================================================
// FFBEngine クラス実装
// ============================================================================

FFBEngine::FFBEngine() : _prev_steer(0), _prev_vel_norm(0.0f) {}

void FFBEngine::reset() {
  _prev_steer = 0;
  _prev_vel_norm = 0.0f;
}

int32_t FFBEngine::update(const FFB_Shared_State_t *effects,
                          int16_t steer_hid) {
  // ステアリング差分から速度・加速度を推定 (メンバ変数で状態保持)
  float vel_norm = ((float)(steer_hid - _prev_steer)) / 32767.0f * 10000.0f;
  float accel_norm = vel_norm - _prev_vel_norm;
  _prev_steer = steer_hid;
  _prev_vel_norm = vel_norm;

  int32_t total_force = 0;

  for (int i = 0; i < MAX_EFFECTS; i++) {
    if (!effects[i].active)
      continue;

    int16_t eff_force = 0;
    switch (effects[i].type) {

    case HID_ET_CONSTANT:
      eff_force = effects[i].magnitude;
      break;

    case HID_ET_SPRING:
    case HID_ET_DAMPER:
    case HID_ET_INERTIA:
    case HID_ET_FRICTION:
      eff_force =
          ffb_calc_condition_force(effects[i], steer_hid, vel_norm, accel_norm);
      break;

    case HID_ET_SINE:
    case HID_ET_SQUARE:
    case HID_ET_TRIANGLE:
    case HID_ET_SAW_UP:
    case HID_ET_SAW_DOWN:
      eff_force = ffb_calc_periodic_force(effects[i]);
      break;

    default:
      break;
    }

    // エフェクト個別の gain (SET_EFFECT 0x01 の Gain フィールド 0..255) を適用
    total_force += (int32_t)eff_force * effects[i].gain / 255;
  }

  // ±10000 にクランプして返す
  if (total_force > 10000)
    total_force = 10000;
  if (total_force < -10000)
    total_force = -10000;

  return total_force;
}
