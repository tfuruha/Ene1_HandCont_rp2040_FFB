/**
 * @file mf4015.cpp
 * @brief LKTECH MF4015 ステアリングモーター制御ライブラリ
 * @date 2026-01-23
 *
 * MF4015ステアリングモーターをCAN通信で制御するためのライブラリです。
 * CANInterfaceを通じて通信を行い、具体的なCAN実装には依存しません。
 *
 * ## 主な機能
 * - モーターのON/OFF制御
 * - エンコーダ値の読み取り
 * - トルク制御（FFB用）
 * - モーター状態の読み取り
 *
 * ## 参考資料
 * - LKTECH CAN Protocol V2.35: http://en.lkmotor.cn/upload/20230706100134f.pdf
 */

#include "CANInterface.h"
#include "config.h"
#include <Arduino.h>

// ============================================================================
// グローバル変数
// ============================================================================

// CANインターフェース（main.cppで初期化されたインスタンスを受け取る）
CANInterface *canBus = nullptr;

// エンコーダ値（最後に受信した値）
uint16_t EncValue = 0;

// ============================================================================
// LKTECHプロトコル コマンド定義
// ============================================================================

#define CmdMotorOff 0x80  // Motor off command
#define CmdMotorOn 0x88   // Motor on command
#define CmdMotorStop 0x81 // Motor stop command
#define CmdOpnLoop 0xA0   // Open loop control command
#define CmdClsTrqu 0xA1   // Torque closed loop control command
#define CmdReadEnc 0x90   // Read encoder command
#define CmdReadStat1 0x9A // Read motor state 1 and error state command
#define CmdClearErr 0x9B  // Clear motor error state
#define CmdReadStat2 0x9C // Read motor state 2 command

// ============================================================================
// 内部ヘルパー関数
// ============================================================================

/**
 * @brief データ[1]～[7]が0x00のコマンドを送信
 *
 * @param cmd コマンドバイト
 */
static void MF_Cmd00(uint8_t cmd) {
  if (canBus == nullptr) {
    return;
  }

  uint8_t data[8] = {cmd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  canBus->sendFrame(Config::Steer::CAN_ID, 8, data);
}

/**
 * @brief 受信データからエンコーダ値を抽出（Read Encoder応答用）
 *
 * @param data 受信データ配列
 * @return エンコーダ値
 */
static uint16_t getEncValue(const uint8_t *data) {
  uint8_t EncLow = data[2];
  uint8_t EncHi = data[3];
  return (EncHi * 256) + EncLow;
}

/**
 * @brief 受信データからエンコーダ値を抽出（Power Command応答用）
 *
 * @param data 受信データ配列
 * @return エンコーダ値
 */
static uint16_t getEncValPow(const uint8_t *data) {
  uint8_t EncLow = data[6];
  uint8_t EncHi = data[7];
  return (EncHi * 256) + EncLow;
}

// ============================================================================
// 公開API
// ============================================================================

/**
 * @brief モーターOFFコマンド
 */
void MF_MotorOff() { MF_Cmd00(CmdMotorOff); }

/**
 * @brief モーターONコマンド
 */
void MF_MotorOn() { MF_Cmd00(CmdMotorOn); }

/**
 * @brief モーター停止コマンド
 */
void MF_MotorStop() { MF_Cmd00(CmdMotorStop); }

/**
 * @brief エンコーダ読み取りコマンド
 */
void MF_ReadEncode() { MF_Cmd00(CmdReadEnc); }

/**
 * @brief モーター状態1読み取りコマンド
 */
void MF_ReadStat1() { MF_Cmd00(CmdReadStat1); }

/**
 * @brief モーター状態2読み取りコマンド
 */
void MF_ReadStat2() { MF_Cmd00(CmdReadStat2); }

/**
 * @brief モーターエラー状態クリアコマンド
 */
void MF_ClearErr() { MF_Cmd00(CmdClearErr); }

/**
 * @brief トルククローズドループ制御コマンド
 *
 * MF4015モーターのトルク電流出力を制御します。
 * Force Feedback実装時に使用します。
 *
 * @param iTorquVal トルク電流指令値 (MF4015_TORQUE_MIN ～ MF4015_TORQUE_MAX)
 *                  MF4015の場合: -2048 ～ 2048 → 実トルク電流 -16.5A ～ 16.5A
 *
 * @note MFシリーズとMGシリーズで実トルク電流範囲が異なります。
 *       - MF: -16.5A ～ 16.5A
 *       - MG: -33A ～ 33A
 */
void MF_SetTorque(int16_t iTorquVal) {
  if (canBus == nullptr) {
    return;
  }

  // トルク値の範囲制限
  if (iTorquVal < Config::Steer::TORQUE_MIN)
    iTorquVal = Config::Steer::TORQUE_MIN;
  else if (iTorquVal > Config::Steer::TORQUE_MAX)
    iTorquVal = Config::Steer::TORQUE_MAX;

  // int16_tをバイト分割
  uint8_t CurrentLowByte = (uint8_t)(iTorquVal & 0x00FF);
  uint8_t CurrentHiByte = (uint8_t)((iTorquVal >> 8) & 0x00FF);

  // CANフレーム構築
  uint8_t data[8] = {CmdClsTrqu,     0x00,          0x00, 0x00,
                     CurrentLowByte, CurrentHiByte, 0x00, 0x00};

  canBus->sendFrame(Config::Steer::CAN_ID, 8, data);
}

/**
 * @brief 最後に受信したエンコーダ値を取得
 *
 * @return エンコーダ値
 */
uint16_t getEncVal() { return EncValue; }

/**
 * @brief CAN受信バッファをチェックし、エンコーダ値を更新
 *
 * @return true: メッセージ受信成功, false: 受信データなし
 */
bool chk_MF_rxBuffer() {
  if (canBus == nullptr) {
    return false;
  }

  uint32_t id;
  uint8_t len;
  uint8_t data[8];

  // CANフレームを受信
  if (canBus->readFrame(id, len, data)) {
    // MF4015からの応答かチェック
    if (id == Config::Steer::CAN_ID && len == 8) {
      // コマンドタイプに応じてエンコーダ値を抽出
      if (data[0] == CmdReadEnc) {
        EncValue = getEncValue(data);
      } else if (data[0] == CmdOpnLoop || data[0] == CmdClsTrqu) {
        EncValue = getEncValPow(data);
      }
      return true;
    }
  }

  return false;
}
