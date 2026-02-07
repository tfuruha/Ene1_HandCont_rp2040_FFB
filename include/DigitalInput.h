#ifndef DIGITAL_INPUT_H
#define DIGITAL_INPUT_H

#include <Arduino.h>

/**
 * @brief デジタル入力をチャンネルごとに管理するクラス
 *
 * チャタリング防止（デバウンス）処理をカプセル化します。
 */
class DigitalInputChannel {
public:
  /**
   * @brief コンストラクタ
   * @param pin ピン番号
   * @param threshold デバウンス用の連続一致サンプル数
   */
  DigitalInputChannel(uint8_t pin, int threshold);

  /**
   * @brief 初期化（ピンモードの設定、内部状態のリセット）
   */
  void Init();

  /**
   * @brief ピンの状態を読み取り、デバウンス処理を更新する
   * @return 確定した現在の論理状態（HIGH/LOW）
   */
  int update();

  /**
   * @brief 確定済みの現在の論理状態を取得する
   * @return HIGH/LOW
   */
  int getState() const { return _currentStatus; }

private:
  uint8_t _pin;
  int _threshold;
  int _counter;
  int _currentStatus;
};

#endif // DIGITAL_INPUT_H
