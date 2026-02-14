/**
 * @file MCP2515_Wrapper.cpp
 * @brief MCP2515 CANコントローラのラッパークラス実装
 * @date 2026-01-23
 *
 * このファイル内でのみMCP2515ライブラリをインクルードし、
 * 具体的な通信処理を実装します。
 */

#include "MCP2515_Wrapper.h"
#include <SPI.h>
#include <mcp2515.h> // MCP2515ライブラリ（このファイル内でのみインクルード）

/**
 * @brief コンストラクタ
 *
 * MCP2515インスタンスを生成し、各ピンを設定します。
 *
 * @param cs CSピン番号
 * @param sck SPI SCKピン番号
 * @param mosi SPI MOSIピン番号
 * @param miso SPI MISOピン番号
 * @param interrupt MCP2515 INTピン番号
 */
MCP2515_Wrapper::MCP2515_Wrapper(uint8_t cs, uint8_t sck, uint8_t mosi,
                                 uint8_t miso, uint8_t interrupt)
    : csPin(cs), sckPin(sck), mosiPin(mosi), misoPin(miso), intPin(interrupt),
      mcp2515(nullptr), rxFrame(nullptr) {
  // MCP2515インスタンスを動的に生成
  mcp2515 = new MCP2515(csPin);

  // 受信フレーム用バッファを確保
  rxFrame = new can_frame();
}

/**
 * @brief デストラクタ
 *
 * MCP2515インスタンスと受信バッファを解放します。
 */
MCP2515_Wrapper::~MCP2515_Wrapper() {
  if (mcp2515 != nullptr) {
    delete mcp2515;
    mcp2515 = nullptr;
  }

  if (rxFrame != nullptr) {
    delete rxFrame;
    rxFrame = nullptr;
  }
}

/**
 * @brief CANコントローラの初期化
 *
 * MCP2515をリセットし、ボーレートとクロック周波数を設定して
 * ノーマルモードで動作を開始します。
 *
 * @return true: 初期化成功, false: 初期化失敗
 */
bool MCP2515_Wrapper::begin() {
  if (mcp2515 == nullptr) {
    return false;
  }

  // 最後に発生したエラーをクリア (OK = 0)
  lastError = MCP2515::ERROR_OK;

  // SPIの初期化 (RP2040耳のコア固有の設定)
  SPI.setRX(misoPin);
  SPI.setTX(mosiPin);
  SPI.setSCK(sckPin);
  SPI.begin();

  // INTピンの初期化 (外部プルアップを想定しているが、安全のため入力設定)
  if (intPin != 255) {
    pinMode(intPin, INPUT_PULLUP);
  }

  // MCP2515をリセット
  mcp2515->reset();

  // ボーレートとクロック周波数を設定
  // CAN_500KBPS: 500kbps, MCP_16MHZ: 16MHz クロック
  MCP2515::ERROR result = mcp2515->setBitrate(CAN_500KBPS, MCP_16MHZ);
  if (result != MCP2515::ERROR_OK) {
    lastError = static_cast<uint8_t>(result);
    return false;
  }

  // ノーマルモードに設定
  result = mcp2515->setNormalMode();
  if (result != MCP2515::ERROR_OK) {
    lastError = static_cast<uint8_t>(result);
    return false;
  }

  return true;
}

/**
 * @brief CANフレームの送信
 *
 * @param id CAN識別子 (11bit標準フレーム)
 * @param len データ長 (0-8バイト)
 * @param data 送信データへのポインタ
 * @return true: 送信成功, false: 送信失敗
 */
bool MCP2515_Wrapper::sendFrame(uint32_t id, uint8_t len, const uint8_t *data) {
  if (mcp2515 == nullptr || data == nullptr) {
    return false;
  }

  // データ長の範囲チェック
  if (len > 8) {
    len = 8;
  }

  // CANフレームを構築
  can_frame frame;
  frame.can_id = id;
  frame.can_dlc = len;

  // データをコピー
  for (uint8_t i = 0; i < len; i++) {
    frame.data[i] = data[i];
  }

  // フレームを送信
  MCP2515::ERROR result = mcp2515->sendMessage(&frame);

  return (result == MCP2515::ERROR_OK);
}

/**
 * @brief CANフレームの受信
 *
 * @param id 受信したCAN識別子を格納する変数への参照
 * @param len 受信したデータ長を格納する変数への参照
 * @param data 受信データを格納するバッファへのポインタ
 * @return true: 受信成功, false: 受信データなし
 */
bool MCP2515_Wrapper::readFrame(uint32_t &id, uint8_t &len, uint8_t *data) {
  if (mcp2515 == nullptr || rxFrame == nullptr || data == nullptr) {
    return false;
  }

  // フレームを受信
  MCP2515::ERROR result = mcp2515->readMessage(rxFrame);

  if (result == MCP2515::ERROR_OK) {
    // 受信成功：データをコピー
    id = rxFrame->can_id;
    len = rxFrame->can_dlc;

    // データ長の範囲チェック
    if (len > 8) {
      len = 8;
    }

    // データをコピー
    for (uint8_t i = 0; i < len; i++) {
      data[i] = rxFrame->data[i];
    }

    return true;
  }

  return false;
}

/**
 * @brief 受信バッファの確認
 *
 * @return true: 受信データあり, false: 受信データなし
 */
bool MCP2515_Wrapper::available() {
  if (mcp2515 == nullptr) {
    return false;
  }

  // INTピンが設定されている場合はピンの状態を確認 (Active Low)
  if (intPin != 255) {
    return (digitalRead(intPin) == LOW);
  }

  // 受信バッファをチェック (フォールバック: SPI通信)
  return mcp2515->checkReceive();
}
