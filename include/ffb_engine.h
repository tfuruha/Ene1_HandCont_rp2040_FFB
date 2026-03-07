/**
 * @file ffb_engine.h
 * @brief FFB (Force Feedback) エフェクト演算エンジン
 *
 * USB PID 仕様に基づくエフェクト種別ごとのトルク計算を担う。
 * Core1 の制御ループから呼び出すことを想定しており、
 * hidwffb (USB HID I/O層) や control (物理制御層) とは独立。
 *
 * @par 責務の分離方針
 * - hidwffb.cpp  : USB PID プロトコル受信・解析・共有メモリ I/O  (Core0)
 * - ffb_engine.cpp: FFB エフェクト演算ロジック                    (Core1 向け)
 * - control.cpp  : モーター制御・物理フィードバック               (Core1)
 * - main.cpp     : タイミング制御・グルーコード                   (Core0/1)
 */

#ifndef FFB_ENGINE_H
#define FFB_ENGINE_H

#include "hidwffb.h" // FFB_Shared_State_t, HID_ET_*, MAX_EFFECTS
#include <stdint.h>

// ============================================================================
// 純粋計算関数 (状態なし)
// ============================================================================

/**
 * @brief Condition 系エフェクトのトルク値を計算する
 *        (Spring / Damper / Inertia / Friction)
 *
 * USB PID 仕様の Condition 計算式:
 *   force = -coeff × displacement  (positive coeff → 求心力)
 *
 * 不感帯・飽和・正負方向で異なる係数を正しく適用する。
 *
 * @param eff        エフェクト共有状態 (FFB_Shared_State_t)
 * @param steer_hid  ハンドル位置 (HID 単位 -32767..32767)
 * @param vel_norm   正規化速度   (-10000..10000 スケール / ループ周期)
 * @param accel_norm 正規化加速度 (-10000..10000 スケール / ループ周期²)
 * @return           エフェクト力 (-10000..10000, PID 単位)
 */
int16_t ffb_calc_condition_force(const FFB_Shared_State_t &eff,
                                 int16_t steer_hid, float vel_norm,
                                 float accel_norm);

/**
 * @brief Periodic 系エフェクトのトルク値を計算する
 *        (Sine / Square / Triangle / Sawtooth Up / Sawtooth Down)
 *
 * エフェクト開始時刻 (startTimeUs) からの経過時間と periodicPeriod から
 * 現在位相 phi [0, 1) を計算し、各波形の瞬時値を返す。
 *
 * @param eff  エフェクト共有状態
 * @return     エフェクト力 (-10000..10000, PID 単位)
 */
int16_t ffb_calc_periodic_force(const FFB_Shared_State_t &eff);

// ============================================================================
// FFBEngine クラス (全エフェクト合算 + 速度/加速度状態管理)
// ============================================================================

/**
 * @brief FFB エフェクト演算クラス
 *
 * ステアリング位置の差分から速度・加速度を推定し、全エフェクトを合算する。
 * 状態（前回値）をメンバ変数で保持し、外部から reset() で初期化可能。
 *
 * @note PhysicalEffect (control.h) と同じ設計パターン。
 *       Core1 の制御周期ごとに update() を 1 回呼び出すこと。
 */
class FFBEngine {
public:
  FFBEngine();

  /**
   * @brief 全アクティブエフェクトを合算した FFB トルク値を返す
   *
   * Core1 の制御周期ごとに 1 回呼び出すこと。
   * 内部で速度・加速度を推定してから Condition/Periodic エフェクトを合算する。
   *
   * @param effects    共有エフェクト配列 (要素数 MAX_EFFECTS)
   * @param steer_hid  現在のハンドル位置 (HID 単位 -32767..32767)
   * @return           合算エフェクト力 (-10000..10000, PID 単位)
   */
  int32_t update(const FFB_Shared_State_t *effects, int16_t steer_hid);

  /**
   * @brief 内部状態をリセットする
   *
   * HID_DC_DEVICE_RESET 受信時など、速度/加速度の推定値を
   * 初期化する際に呼び出す。
   */
  void reset();

private:
  int16_t _prev_steer;  ///< 前回ハンドル位置 (HID 単位)
  float _prev_vel_norm; ///< 前回正規化速度
};

#endif // FFB_ENGINE_H
