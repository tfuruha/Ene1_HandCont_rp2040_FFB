/**
 * @file MF4015_Driver.h
 * @brief LKTECH MF4015 モータードライバクラス
 * @date 2026-01-23
 *
 * CANInterfaceを通じてMF4015モーターを制御します。
 */

#ifndef MF4015_DRIVER_H
#define MF4015_DRIVER_H

#include <Arduino.h>
#include <CANInterface.h>

class MF4015_Driver {
public:
  /**
   * @brief モーターステータス構造体
   */
  struct Status {
    uint16_t encoder;      ///< エンコーダ位置 (0-16383)
    int16_t speed;         ///< 回転速度
    int16_t torqueCurrent; ///< トルク電流
    int8_t temperature;    ///< モーター温度
    uint8_t errorState;    ///< エラー状態
  };

  /**
   * @brief コンストラクタ
   * @param canInterface 通信に使用するCANインターフェース
   * @param motorCanId モーターのCAN ID (デフォルト 0x141)
   */
  MF4015_Driver(CANInterface *canInterface, uint32_t motorCanId = 0x141);

  // --- モーター制御 ---
  void enable();  ///< モーターON (0x88)
  void disable(); ///< モーターOFF (0x80)
  void stop();    ///< モーター停止 (0x81)

  /**
   * @brief トルク指令の送信
   * @param torque トルク値 (-2048 ～ 2048)
   */
  void setTorque(int16_t torque);

  /**
   * @brief エンコーダ読み取りリクエストの送信
   */
  void requestEncoder();

  // --- データ受信・解析 ---
  /**
   * @brief 受信したCANフレームがこのモーターのものか判定し、解析する
   * @param id CAN ID
   * @param len データ長
   * @param data データ配列
   * @return true: このモーターのデータとして処理した, false: ID不一致
   */
  bool parseFrame(uint32_t id, uint8_t len, const uint8_t *data);

  // --- ゲッター ---
  uint16_t getEncoderValue() const { return status.encoder; }

  /**
   * @brief センターオフセットおよび範囲制限を適用したステアリング値を取得
   * @return int16_t 処理済みのステアリング値 (左負, 右正)
   */
  int16_t getSteerValue() const;

  const Status &getStatus() const { return status; }

private:
  CANInterface *can; ///< 外部から注入されたCANインターフェース
  uint32_t canId;    ///< ターゲットモーターのID
  Status status;     ///< 現在のステータス情報

  // コマンド定数 (内部用)
  static constexpr uint8_t CMD_MOTOR_OFF = 0x80;
  static constexpr uint8_t CMD_MOTOR_ON = 0x88;
  static constexpr uint8_t CMD_MOTOR_STOP = 0x81;
  static constexpr uint8_t CMD_TORQUE_CTRL = 0xA1;
  static constexpr uint8_t CMD_READ_ENC = 0x90;

  void sendCommand(uint8_t cmd, const uint8_t *data = nullptr, uint8_t len = 0);
};

#endif // MF4015_DRIVER_H
