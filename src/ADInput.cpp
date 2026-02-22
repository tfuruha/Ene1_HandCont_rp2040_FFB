#include "ADInput.h"

ADInputChannel::ADInputChannel(uint8_t pin, uint16_t bufferSize,
                               int (*transform)(int))
    : _pin(pin), _bufferSize(bufferSize), _transform(transform),
      _buffer(nullptr), _head(0), _sampleCount(0), _sum(0) {
  if (_bufferSize > 0) {
    _buffer = new int[_bufferSize];
  }
}

ADInputChannel::~ADInputChannel() {
  if (_buffer) {
    delete[] _buffer;
  }
}

void ADInputChannel::Init() {
  pinMode(_pin, INPUT);
  _head = 0;
  _sampleCount = 0;
  _sum = 0;
  if (_buffer) {
    for (uint16_t i = 0; i < _bufferSize; i++) {
      _buffer[i] = 0;
    }
  }
}

void ADInputChannel::getadc() {
  if (!_buffer)
    return;

  int newValue = analogRead(_pin);

  // 合計値から一番古い値を引き、新しい値を足す（差分更新）
  // サンプル数がバッファサイズに達している場合のみ、古い値を引く
  if (_sampleCount >= _bufferSize) {
    _sum -= _buffer[_head];
  } else {
    _sampleCount++;
  }

  _buffer[_head] = newValue;
  _sum += newValue;

  // ヘッドを次に進める
  _head = (_head + 1) % _bufferSize;
}

int ADInputChannel::getvalue() {
  // バッファサイズより入力回数が少ないときは0を返す
  if (_sampleCount < _bufferSize) {
    return 0;
  }

  // 平均値の計算
  int average = (int)((float)_sum / (float)_bufferSize);

  // 変換関数が指定されていれば適用する
  if (_transform) {
    return _transform(average);
  }

  return average;
}

int ADInputChannel::getRawLatest() const {
  if (!_buffer || _sampleCount == 0)
    return 0;
  // _head は次に書き込む位置なので、最新値はその一つ前
  uint16_t latestIdx = (_head == 0) ? (_bufferSize - 1) : (_head - 1);
  return _buffer[latestIdx];
}
