#ifndef AD_INPUT_H
#define AD_INPUT_H

#include <Arduino.h>

/**
 * @brief AD変換値をチャンネルごとに管理するクラス
 *
 * リングバッファを用いた移動平均処理と、物理量への変換機能を提供します。
 */
class ADInputChannel {
public:
  /**
   * @brief コンストラクタ
   * @param pin ピン番号
   * @param bufferSize 移動平均バッファサイズ
   * @param transform
   * 物理量への変換関数ポインタ（デフォルトはそのままの値を返す）
   */
  ADInputChannel(uint8_t pin, uint16_t bufferSize,
                 int (*transform)(int) = nullptr);

  /**
   * @brief デストラクタ（バッファの解放）
   */
  ~ADInputChannel();

  /**
   * @brief 初期化（ピンモード設定、バッファクリア）
   */
  void Init();

  /**
   * @brief AD変換値を取得し、リングバッファに格納する
   * 同期性を優先するため、このメソッドでは変換処理は行いません。
   */
  void getadc();

  /**
   * @brief 移動平均を計算し、物理量に変換した値を返す
   * @return 物理量に変換された値。サンプル数が不足している場合は0。
   */
  int getvalue();

  /**
   * @brief バッファ内の最新のAD変換生値を返す（デバッグ用）
   * @return 最後にgetadc()で取得した生のAD変換値。サンプルなしの場合は0。
   */
  int getRawLatest() const;

private:
  uint8_t _pin;
  uint16_t _bufferSize;
  int (*_transform)(int);

  int *_buffer;
  uint16_t _head;
  uint16_t _sampleCount;
  int32_t _sum; // 高速化のため合計値を保持（リングバッファ更新時に差分更新）
};

#endif // AD_INPUT_H
