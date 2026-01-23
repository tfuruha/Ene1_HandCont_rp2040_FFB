# USB HID通信モジュール設計書 (HIDModule.md)

## 1. 目的
PCホストに対して、デバイスを標準的なゲームコントローラ（Joystick）として認識させ、低遅延で操作情報を送信する仕様を規定する。

## 2. デバイス構成
- **USBクラス**: HID (Human Interface Device)
- **ライブラリ**: `Adafruit_TinyUSB`
- **レポートID**: デフォルトを使用
- **管理コア**: Core 0

## 3. レポート構造 (送信データ)
以下のデータを20ms周期でホストに送信する。

### 3.1 軸入力 (Axes)
| 軸名 | 役割 | レポート値 | 元データレンジ |
| :--- | :--- | :--- | :--- |
| Steering | ハンドル角度 | -127 ～ 127 | -5461 ～ 5461 (±30度) |
| Accelerator | アクセル量 | -127 ～ 127 | 0 ～ 65535 |
| Brake | ブレーキ圧 | -127 ～ 127 | 0 ～ 65535 |

### 3.2 ボタン入力 (Buttons)
| ボタン番号 | 役割 | ピン番号 | 備考 |
| :--- | :--- | :--- | :--- |
| Button 0 | Shift Up | GPIO 25 | Active Low, 内部プルアップ |
| Button 1 | Shift Down | GPIO 24 | Active Low, 内部プルアップ |

## 4. 受信 (PC -> Controller)
- **FFB目標トルク**: Core 0 にてパケット受信後、`sharedData.targetTorque` (-2048～2048) を更新する。

## 5. 特記事項
- **ライブラリ変更**: 当初検討していた `mheironimus/Joystick` から、デュアルコア環境との親和性が高い `Adafruit_TinyUSB` へ変更した。
