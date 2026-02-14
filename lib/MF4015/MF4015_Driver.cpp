/**
 * @file MF4015_Driver.cpp
 * @brief MF4015 モータードライバの実装
 */

#include "MF4015_Driver.h"
#include "config.h"

MF4015_Driver::MF4015_Driver(CANInterface *canInterface, uint32_t motorCanId)
    : can(canInterface), canId(motorCanId) {
  status = {0, 0, 0, 0, 0};
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

void MF4015_Driver::setTorque(int16_t torque) {
  // 範囲制限 (-2048 ～ 2048)
  if (torque < -2048)
    torque = -2048;
  if (torque > 2048)
    torque = 2048;

  uint8_t data[8] = {0};
  data[0] = torque & 0xFF;        // Low byte
  data[1] = (torque >> 8) & 0xFF; // High byte

  // トルク指令 (0xA1)
  // プロトコル上、データ[4], [5] にトルク値を入れる場合があるが、
  // 旧コードのロジックを忠実に再現
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
  }

  return true;
}

int16_t MF4015_Driver::getSteerValue() const {
  // 1. センターオフセットを適用 (生値 - センター)
  // int32_t を使うことでマイナス方向の計算を安全に行う
  int32_t relativePos = (int32_t)status.encoder - MF4015_ANGLE_CENTER;

  // 2. 範囲制限 (クランプ処理)
  if (relativePos < MF4015_ANGLE_MIN) {
    relativePos = MF4015_ANGLE_MIN;
  } else if (relativePos > MF4015_ANGLE_MAX) {
    relativePos = MF4015_ANGLE_MAX;
  }

  return (int16_t)relativePos;
}
