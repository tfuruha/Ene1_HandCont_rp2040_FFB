# 入力出力モジュール設計書 (InputOutputModule.md)

## 1. 目的
アナログセンサ（アクセル・ブレーキ）およびデジタルスイッチ（シフト）の信号を精度よく、かつノイズに強く取り込むための仕様を規定する。

## 2. 物理構成
- **ピンアサイン**:
  - Config::Pin::SHIFT_UP (14): Shift Up (内部プルアップ)
  - Config::Pin::SHIFT_DOWN (15): Shift Down (内部プルアップ)
  - Config::Pin::ACCEL (26): Accel (スロットル)
  - Config::Pin::BRAKE (28): Brake (面圧センサ)

## 3. デジタル入力処理 (シフトスイッチ)
スイッチのチャタリングおよびノイズによる誤動作を防ぐため、デジタルフィルタを実装する。

### 3.1 アルゴリズム
- **サンプリング**: 周期実行 (`Config::Time::SAMPLING_INTERVAL_MS` ms周期)。
- **確定条件**: `DigitalInputChannel` クラスを使用。状態が変化するためには、あらかじめ設定された閾値（`Config::Input::BUTTON_DEBOUNCE_THRESHOLD = 4`）と同じ回数連続でその状態が検出される必要がある。
- **実装**:
  - `HIGH`（非押下）が検出されるとカウンタをインクリメント（最大閾値まで）。閾値に達すると状態を `HIGH` に確定。
  - `LOW`（押下）が検出されるとカウンタをデクリメント（最小0まで）。0に達すると状態を `LOW` に確定。
  - 閾値に達するまでは前回の確定状態を維持する。

## 4. アナログ入力処理 (アクセル・ブレーキ)
センサの個体差、回路ノイズ、および物理的な操作特性を吸収し、USB HIDレポートに適した形式に変換する。

### 4.1 信号処理フロー
1. **サンプリング**: 周期実行で `analogRead` を実行。
2. **平滑化 (`ADInputChannel`)**:
   - `Config::Adc::AVERAGE_COUNT = 8` サンプルの移動平均を算出。
   - リングバッファを使用し、毎回のサンプリング時に合計値を差分更新。
3. **キャリブレーションとスケーリング**:
   - 出力範囲: USB HIDの仕様変更に伴い、16bit 符号なし範囲（0 ～ 65535）に変換。
   - **アクセル (Z軸)**:
     - 入力範囲: `Config::Adc::ACCEL_MIN (180)` - `Config::Adc::ACCEL_MAX (890)`
     - 変換: `map(val, min, max, Config::Adc::ACCEL_HID_MIN, Config::Adc::ACCEL_HID_MAX)` を行い、`constrain` で範囲内に制限。
   - **ブレーキ (Rz軸)**:
     - 入力範囲: `Config::Adc::BRAKE_MIN (100)` - `Config::Adc::BRAKE_MAX (820)`
     - 変換: `BRAKE_INVERT` が有効な場合、`map(val, min, max, Config::Adc::BRAKE_HID_MAX, Config::Adc::BRAKE_HID_MIN)` を用いて反転。踏み込み量 0 ～ 65535 に変換。

### 4.2 特徴
- 制御ループ（Core 1）で他の処理を妨げないよう、入力処理は非ブロッキングかつ軽量に実装されている。
- 全ての定数は `config.h` の `Config::Adc` または `Config::Pin` 名前空間に集約され、メンテナンス性を高めている。
