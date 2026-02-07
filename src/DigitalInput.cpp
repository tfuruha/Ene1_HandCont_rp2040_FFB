#include "DigitalInput.h"

DigitalInputChannel::DigitalInputChannel(uint8_t pin, int threshold)
    : _pin(pin), _threshold(threshold), _counter(threshold),
      _currentStatus(HIGH) {}

void DigitalInputChannel::Init() {
  pinMode(_pin, INPUT_PULLUP);
  _counter = _threshold;
  _currentStatus = HIGH;
}

int DigitalInputChannel::update() {
  int iBtn = digitalRead(_pin);

  if (iBtn > 0) { // HIGH (ボタン非押下)
    _counter++;
    if (_counter > _threshold) {
      _counter = _threshold;
      _currentStatus = HIGH;
    }
  } else { // LOW (ボタン押下)
    _counter--;
    if (_counter < 0) {
      _counter = 0;
      _currentStatus = LOW;
    }
  }
  return _currentStatus;
}
