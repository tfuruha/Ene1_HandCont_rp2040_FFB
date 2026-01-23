/**
 * @file mf4015.h
 * @brief LKTECH MF4015 ステアリングモーター制御ライブラリ ヘッダー
 * @date 2026-01-23
 *
 * MF4015ステアリングモーターをCAN通信で制御するための関数を提供します。
 */

#ifndef MF4015_H
#define MF4015_H

#include <Arduino.h>

// ============================================================================
// 公開API
// ============================================================================

/**
 * @brief モーターOFFコマンド
 */
void MF_MotorOff();

/**
 * @brief モーターONコマンド
 */
void MF_MotorOn();

/**
 * @brief モーター停止コマンド
 */
void MF_MotorStop();

/**
 * @brief エンコーダ読み取りコマンド
 */
void MF_ReadEncode();

/**
 * @brief モーター状態1読み取りコマンド
 */
void MF_ReadStat1();

/**
 * @brief モーター状態2読み取りコマンド
 */
void MF_ReadStat2();

/**
 * @brief モーターエラー状態クリアコマンド
 */
void MF_ClearErr();

/**
 * @brief トルククローズドループ制御コマンド
 *
 * @param iTorquVal トルク電流指令値 (-2048 ～ 2048)
 */
void MF_SetTorque(int16_t iTorquVal);

/**
 * @brief 最後に受信したエンコーダ値を取得
 *
 * @return エンコーダ値
 */
uint16_t getEncVal();

/**
 * @brief CAN受信バッファをチェックし、エンコーダ値を更新
 *
 * @return true: メッセージ受信成功, false: 受信データなし
 */
bool chk_MF_rxBuffer();

#endif // MF4015_H
