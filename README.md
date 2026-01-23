# ene1用ハンドルコントローラ (Ene1 Steering Controller)

PC用レーシングシミュレーション（Assetto Corsa等）で使用することを目的とした、自作ハンドルコントローラの組込みコントローラプロジェクトです。

## 概要
Arduino Pro Micro (ATmega32U4) をコアとし、CAN通信で制御するブラシレスモータ（MF4015）をステアリングセンサ兼アクチュエータとして使用します。標準的なUSB HID (Joystick) として認識されるため、多くのレーシングゲームでそのまま利用可能です。

## 主な機能
- **ステアリング**: CAN通信を介した高精度な角度取得（LKTECH MF4015使用）
- **アクセル/ブレーキ**: アナログセンサ（スロットル/面圧センサ）による入力
- **シフト**: デジタル入力によるパドルシフト対応
- **将来的な拡張性**: モータトルク制御コマンドを実装済みで、将来的なFFB（フォースフィードバック）への対応が可能

## ハードウェア構成
- **マイコン**: Arduino Pro Micro (16MHz/5V)
- **CANコントローラ**: MCP2515 (SPI接続, Clock: 8MHz)
- **ステアリングモータ**: [LKTECH MF4015](http://en.lkmotor.cn/ProDetail.aspx?ProId=256)
- **アクセル**: 0.8V - 4.2V 電圧出力型スロットル
- **ブレーキ**: [薄膜圧力センサー](https://www.amazon.co.jp/dp/B07NKQWY3P)

### ピンアサイン
| ピン番号 | 信号名 | 機能 |
| :--- | :--- | :--- |
| D6 | S-Up Button | デジタル入力 (Active Low) |
| D7 | S-Down Button | デジタル入力 (Active Low) |
| A0 | Brake | アナログ入力 (面圧センサ) |
| A1 | Accel | アナログ入力 (電圧入力) |
| D10 | CAN_CS | MCP2515 CS信号 |

## 開発環境
- **PlatformIO**: Core `6.1.13` 以前/以降
- **開発基板**: `sparkfun_promicro16`
- **フレームワーク**: Arduino

## プロジェクト構造と詳細設計
詳細な設計仕様については、以下のドキュメントを参照してください。

- [全体設計書 (SystemDesign.md)](./SystemDesign.md)
- [入出力モジュール設計書 (InputOutputModule.md)](./InputOutputModule.md)
- [ステアリング制御モジュール設計書 (SteeringModule.md)](./SteeringModule.md)
- [HID送信モジュール設計書 (HIDModule.md)](./HIDModule.md)

## 参照資料
- [LKTECH CAN PROTOCOL V2.35](http://en.lkmotor.cn/upload/20230706100134f.pdf)
- [Joystick Library](https://github.com/MHeironimus/ArduinoJoystickLibrary)
- [arduino-mcp2515 Library](https://github.com/autowp/arduino-mcp2515)
