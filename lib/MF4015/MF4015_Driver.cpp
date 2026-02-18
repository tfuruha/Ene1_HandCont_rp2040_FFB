/**
 * @file MF4015_Driver.cpp
 * @brief MF4015 モータードライバの実装
 * @note プロトコル抜粋はMotorDriveProtocol.mdを参照
 */

#include "MF4015_Driver.h"
#include "config.h"
#include <cstdint>

MF4015_Driver::MF4015_Driver(CANInterface *canInterface, uint32_t motorCanId)
    : can(canInterface), canId(motorCanId) {
  status = {0, 0, 0, 0, 0};
  isTorqueLimited = false;
  encoderUpdated = false;
}

void MF4015_Driver::enable() {
  uint8_t data[8] = {0};
  sendCommand(CMD_MOTOR_ON, data, 7);
}

void MF4015_Driver::disable() {
  uint8_t data[8] = {0};
  sendCommand(CMD_MOTOR_OFF, data, 7);
}

void MF4015_Driver::stop() {
  uint8_t data[8] = {0};
  sendCommand(CMD_MOTOR_STOP, data, 7);
}

void MF4015_Driver::requestEncoder() {
  uint8_t data[8] = {0};
  sendCommand(CMD_READ_ENC, data, 7);
}

void MF4015_Driver::clearError() {
  uint8_t data[8] = {0};
  sendCommand(CMD_CLEAR_ERR, data, 7);
}

void MF4015_Driver::requestStatus1() {
  uint8_t data[8] = {0};
  sendCommand(CMD_READ_STAT1, data, 7);
}

void MF4015_Driver::setTorque(int16_t torque) {
  // アプリ座標とモータ座標が逆位相なので、反転させる
  torque = -1 * torque;
  // 1. 範囲制限判定 (ヒステリシス付き)
  // status.encoder から直接生の相対位置を計算
  int32_t rawPos = Config::Steer::ANGLE_CENTER - (int32_t)status.encoder;

  if (isTorqueLimited) {
    // 制限中の場合：境界からヒステリシス分内側に戻ったら解除
    if (rawPos >= (Config::Steer::ANGLE_MIN + HYSTERESIS_WIDTH) &&
        rawPos <= (Config::Steer::ANGLE_MAX - HYSTERESIS_WIDTH)) {
      isTorqueLimited = false;
    }
  } else {
    // 動作中の場合：境界を超えたら制限開始
    if (rawPos < Config::Steer::ANGLE_MIN ||
        rawPos > Config::Steer::ANGLE_MAX) {
      isTorqueLimited = true;
    }
  }

  // 制限中はトルクを0にする
  if (isTorqueLimited) {
    torque = 0;
  }

  // 2. 出力範囲制限 (Config値に基づいたクランプ)
  if (torque < Config::Steer::TORQUE_MIN)
    torque = Config::Steer::TORQUE_MIN;
  if (torque > Config::Steer::TORQUE_MAX)
    torque = Config::Steer::TORQUE_MAX;

  uint8_t data[8] = {0};
  data[0] = torque & 0xFF;        // Low byte
  data[1] = (torque >> 8) & 0xFF; // High byte

  // トルク指令 (0xA1)
  uint8_t frameData[8] = {CMD_TORQUE_CTRL, 0, 0, 0, data[0], data[1], 0, 0};

  if (can) {
    can->sendFrame(canId, 8, frameData);
  }
}

void MF4015_Driver::sendCommand(uint8_t cmd, const uint8_t *data, uint8_t len) {
  if (!can)
    return;

  uint8_t frameData[8] = {0};
  frameData[0] = cmd;
  if (data && len > 0) {
    for (uint8_t i = 0; i < len && i < 7; i++) {
      frameData[i + 1] = data[i];
    }
  }
  can->sendFrame(canId, 8, frameData);
}

bool MF4015_Driver::parseFrame(uint32_t id, uint8_t len, const uint8_t *data) {
  if (id != canId || len < 8)
    return false;

  uint8_t cmd = data[0];

  if (cmd == CMD_READ_ENC) {
    // エンコーダ読み取り応答
    // data[2]: Low, data[3]: High (14bit or 16bit depending on motor)
    status.encoder = data[2] | (data[3] << 8);
    encoderUpdated = true;
  } else if (cmd == 0xA0 || cmd == CMD_TORQUE_CTRL) {
    // 制御コマンドの応答 (現在のステータスが返ってくる)
    // data[1]: 温度
    // data[2,3]: トルク電流
    // data[4,5]: スピード
    // data[6,7]: エンコーダ位置
    status.temperature = (int8_t)data[1];
    status.torqueCurrent = (int16_t)(data[2] | (data[3] << 8));
    status.speed = (int16_t)(data[4] | (data[5] << 8));
    status.encoder = (uint16_t)(data[6] | (data[7] << 8));
  } else if (cmd == CMD_READ_STAT1) {
    // 状態1の応答: data[7] がエラー状態フラグ(低電圧、過熱)
    // bit 0: Voltage state | 0 Normal/ 1 UnderVoltage Potect
    // bit 3: Temperature state | 0 Normal/ 1 OverTemperature Potect
    // その他bitはinvalid
    status.errorState = data[7];
  }

  return true;
}

int16_t MF4015_Driver::getSteerValue() const {
  // 1. センターオフセットを適用 (生値 - センター)
  // int32_t を使うことでマイナス方向の計算を安全に行う
  // HIDレポートはモータ座標に対して逆位相なので、反転させる
  int32_t relativePos = Config::Steer::ANGLE_CENTER - (int32_t)status.encoder;

  // 2. 範囲制限 (クランプ処理)
  if (relativePos < Config::Steer::ANGLE_MIN) {
    relativePos = Config::Steer::ANGLE_MIN;
  } else if (relativePos > Config::Steer::ANGLE_MAX) {
    relativePos = Config::Steer::ANGLE_MAX;
  }

  return (int16_t)relativePos;
}
