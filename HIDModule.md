# USB HID/FFBモジュール設計書 (HIDModule.md)

## 1. 目的
PCホストに対して、デバイスを標準的なゲームコントローラ（Joystick）およびPID（Physical Interface Device）として認識させ、双方向の低遅延通信を実現する仕様を規定する。

## 2. デバイス構成
- **USBクラス**: HID (Human Interface Device)
- **USBスタック**: `Adafruit_TinyUSB`
- **レポートID**:
  - `ID: 0x01` : 入力レポート（Gamepad） / 出力レポート（Set Effect）
  - `ID: 0x05` : FFB定数力（Set Constant Force）
  - `ID: 0x0A` : FFB制御（Effect Operation）
  - `ID: 0x0D` : FFBゲイン（Device Gain）
- **管理コア**: Core 0 (HID通信およびパケット解析)

## 3. 入力レポート構造 (Controller -> PC)
以下のデータを 2ms 周期 (`Config::Time::HIDREPO_INTERVAL_MS`) で送信する。全軸 16bit の高解像度通信を行う。

### 3.1 軸入力 (Axes)
| 軸名 | 役割 | レポート値 (16bit) | 元データ | 備考 |
| :--- | :--- | :--- | :--- | :--- |
| Steering (X) | ハンドル角度 | -32767 ～ 32767 | MF4015 エンコーダ値 | 符号付き |
| Accelerator (Z) | アクセル量 | 0 ～ 65535 | ADC0 サンプリング値 | 符号なし |
| Brake (Rz) | ブレーキ圧 | 0 ～ 65535 | ADC2 サンプリング値 | 符号なし |

### 3.2 ボタン入力 (Buttons)
- **構成**: 16ボタン（現状2ボタン使用）
- **Button 1**: Shift Up (Config::Pin::SHIFT_UP)
- **Button 2**: Shift Down (Config::Pin::SHIFT_DOWN)

## 4. 出力レポート構造 (PC -> Controller / FFB)
ホストからの PID プロトコルに基づき、Force Feedback 情報を解析する。

### 4.1 対応レポート ID
- **0x01 (Set Effect)**: エフェクトの種類（Constant Force 等）とゲインの設定。
- **0x05 (Set Constant Force)**: 出力する力の大きさ（Magnitude: -32767 ～ 32767）を指定。
- **0x0A (Effect Operation)**: エフェクトの開始 (Start) / 停止 (Stop) を制御。
- **0x0D (Device Gain)**: デバイス全体のマスターゲインの設定。

## 5. モジュール間連携
- `hidwffb.cpp` が Core 0 で稼働し、受信した FFB 命令をミューテックス保護された共有メモリ（`shared_ffb_effects`）へ書き込む。
- Core 1 がこの共有メモリを読み取り、1ms 周期のモーター制御ループへ反映する。
- 物理入力（Steer/Accel/Brake）はその逆の経路で Core 0 へ渡され、ホストへ送信される。
