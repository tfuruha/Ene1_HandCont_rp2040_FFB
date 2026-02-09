#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include "hidwffb.h"

/**
 * @file shared_data.h
 * @brief Core 0 と Core 1 で共有するデータ構造体の定義
 * @date 2026-02-08
 *
 * RP2040のデュアルコア構成において、Core間でデータを安全に共有するための構造体です。
 * 物理入力およびFFB情報の詳細は hidwffb.h の構造体定義を参照してください。
 */

/**
 * @struct SharedData
 * @brief Core 0 と Core 1
 * で共有する制御用データ（HIDレポート外のシステム状態等）
 */
struct SharedData {
  // ========================================================================
  // システムステータス
  // ========================================================================

  /**
   * @brief Core 1 の目標トルク値 (MF4015ドライバへ渡す値)
   * Core 0 が PID 解析結果から算出し、Core 1 が参照する
   */
  volatile int16_t targetTorque;

  /**
   * @brief Core 1 のループカウンタ
   *
   * Core 1が正常に動作しているかを確認するためのカウンタ。
   * 毎ループごとにインクリメントされる。
   */
  volatile uint32_t core1LoopCount;

  /**
   * @brief Core 1 の最終実行時刻 (マイクロ秒)
   *
   * Core 1が最後にデータを更新した時刻 (micros() の値)。
   * Core 0側でタイムアウト検出に使用可能。
   */
  volatile uint32_t lastCore1Micros;
};

/**
 * @brief グローバル共有インスタンスの extern 宣言
 *
 * 実体は main.cpp で定義されます。
 * 各モジュールはこのグローバルインスタンスを通じてデータを共有します。
 */
extern SharedData sharedData;

#endif // SHARED_DATA_H
