#ifndef CAN_INTERFACE_H
#define CAN_INTERFACE_H

#include <Arduino.h>

/**
 * @file CANInterface.h
 * @brief CAN通信を抽象化するためのインターフェースクラス
 * @date 2026-01-23
 *
 * このインターフェースを実装することで、異なるCANコントローラ
 * (MCP2515, MCP2517FD, 内蔵CANなど)への移植を容易にします。
 * また、テスト時のモック実装も可能になります。
 */

/**
 * @class CANInterface
 * @brief CAN通信の抽象インターフェース
 *
 * 純粋仮想関数のみを持つ抽象クラスです。
 * 実装クラスはこのインターフェースを継承し、各関数を実装してください。
 */
class CANInterface {
public:
  /**
   * @brief 仮想デストラクタ
   */
  virtual ~CANInterface() {}

  /**
   * @brief CANコントローラの初期化
   *
   * ボーレート、クロック周波数などの設定を行い、
   * CANコントローラを動作可能な状態にします。
   *
   * @return true: 初期化成功, false: 初期化失敗
   */
  virtual bool begin() = 0;

  /**
   * @brief CANフレームの送信
   *
   * 指定されたCAN IDとデータを持つフレームを送信します。
   *
   * @param id CAN識別子 (11bit標準フレームまたは29bit拡張フレーム)
   * @param len データ長 (0-8バイト)
   * @param data 送信データへのポインタ (len バイト分)
   * @return true: 送信成功, false: 送信失敗
   */
  virtual bool sendFrame(uint32_t id, uint8_t len, const uint8_t *data) = 0;

  /**
   * @brief CANフレームの受信
   *
   * 受信バッファにフレームがあれば読み取ります。
   *
   * @param id 受信したCAN識別子を格納する変数への参照
   * @param len 受信したデータ長を格納する変数への参照
   * @param data 受信データを格納するバッファへのポインタ
   * (最低8バイト確保すること)
   * @return true: 受信成功, false: 受信データなし
   */
  virtual bool readFrame(uint32_t &id, uint8_t &len, uint8_t *data) = 0;

  /**
   * @brief 受信バッファの確認
   *
   * 受信バッファに未読のフレームがあるかを確認します。
   *
   * @return true: 受信データあり, false: 受信データなし
   */
  virtual bool available() = 0;
};

#endif // CAN_INTERFACE_H
