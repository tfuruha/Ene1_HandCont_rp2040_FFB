#ifndef MCP2515_WRAPPER_H
#define MCP2515_WRAPPER_H

#include <CANInterface.h> // includeディレクトリから参照

// 前方宣言（MCP2515の詳細をヘッダーから隠蔽）
class MCP2515;
struct can_frame;

/**
 * @file MCP2515_Wrapper.h
 * @brief MCP2515 CANコントローラのラッパークラス
 * @date 2026-01-23
 *
 * CANInterfaceを実装し、MCP2515ライブラリへの依存を隔離します。
 * このクラスを使用することで、上位レイヤーはMCP2515の実装詳細を知る必要がありません。
 *
 * ## 設計意図
 * - 依存性の逆転:
 * 上位レイヤーはCANInterfaceに依存し、具体的な実装には依存しない
 * - テスト容易性: モックCANInterfaceを簡単に作成できる
 * - 移植性: 別のCANコントローラへの移植が容易
 */

/**
 * @class MCP2515_Wrapper
 * @brief MCP2515 CANコントローラのラッパークラス
 *
 * CANInterfaceを継承し、MCP2515固有の実装を提供します。
 */
class MCP2515_Wrapper : public CANInterface {
private:
  MCP2515 *mcp2515;   ///< MCP2515インスタンスへのポインタ
  uint8_t csPin;      ///< CSピン番号
  can_frame *rxFrame; ///< 受信フレーム用バッファ

public:
  /**
   * @brief コンストラクタ
   *
   * @param cs CSピン番号
   */
  explicit MCP2515_Wrapper(uint8_t cs);

  /**
   * @brief デストラクタ
   *
   * MCP2515インスタンスを解放します。
   */
  ~MCP2515_Wrapper();

  /**
   * @brief CANコントローラの初期化
   *
   * MCP2515をリセットし、ボーレートとクロック周波数を設定して
   * ノーマルモードで動作を開始します。
   *
   * 設定値:
   * - ボーレート: CAN_500KBPS (500kbps)
   * - クロック: MCP_16MHZ (16MHz)
   *
   * @return true: 初期化成功, false: 初期化失敗
   */
  bool begin() override;

  /**
   * @brief CANフレームの送信
   *
   * 指定されたCAN IDとデータを持つフレームを送信します。
   *
   * @param id CAN識別子 (11bit標準フレーム)
   * @param len データ長 (0-8バイト)
   * @param data 送信データへのポインタ (len バイト分)
   * @return true: 送信成功, false: 送信失敗
   */
  bool sendFrame(uint32_t id, uint8_t len, const uint8_t *data) override;

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
  bool readFrame(uint32_t &id, uint8_t &len, uint8_t *data) override;

  /**
   * @brief 受信バッファの確認
   *
   * 受信バッファに未読のフレームがあるかを確認します。
   *
   * @return true: 受信データあり, false: 受信データなし
   */
  bool available() override;
};

#endif // MCP2515_WRAPPER_H
