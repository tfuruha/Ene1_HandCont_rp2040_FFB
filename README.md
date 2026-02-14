# ene1用ハンドルコントローラ (Ene1 Steering Controller)

**Work In Progress**
PC用レーシングシミュレーション（Assetto Corsa等）で使用することを目的とした、自作ハンドルコントローラの組込みコントローラプロジェクトです。

## 概要
RP2040を核とし、デュアルコアアーキテクチャを活用した高精度・低遅延なステアリングデバイスです。CAN通信で制御するブラシレスモータ（MF4015）をステアリングセンサ兼アクチュエータとして使用し、Force Feedback (FFB) に対応します。

## 主な機能
- **高精度ステアリング**: 1ms周期の制御ループによる高精度な角度取得とトルク制御。
- **高解像度入力**: アクセル、ブレーキ、ステアリングの全軸で 16bit (-32767 〜 32767) の高解像度通信を実現。
- **FFB (Force Feedback)**: USB PID プロトコルに対応し、PCからの反力を定常トルク指令としてモーターへ反映。
- **デュアルコア最適化**: Core 0 で USB/PID 管理、Core 1 で 1ms の高速センサー/モーター制御を完全に分離。

## ハードウェア構成
- **マイコン**: RP2040 (AE-RP2040等)
- **CANコントローラ**: MCP2515 (SPI接続, Clock: 16MHz)
- **ステアリングモータ**: [LKTECH MF4015](http://en.lkmotor.cn/ProDetail.aspx?ProId=256)
- **入力デバイス**: 電圧出力型スロットル(アクセル)、薄膜面圧センサ(ブレーキ)、パドルシフトスイッチ。

### ピンアサイン (config.h)
| ピン番号 | 信号名 | 機能 |
| :--- | :--- | :--- |
| GP1 | CAN_INT | MCP2515 受信割込信号 (ポーリング監視) |
| GP2 | SPI_SCK | MCP2515 SPI クロック |
| GP3 | SPI_TX | MCP2515 SPI MOSI |
| GP4 | SPI_RX | MCP2515 SPI MISO |
| GP5 | CAN_CS | MCP2515 SPI チップセレクト |
| GP14 | S-Up | 左パドル (Digital Input, Active Low) |
| GP15 | S-Down | 右パドル (Digital Input, Active Low) |
| GP26 | Accel | ADC0 (アナログ入力) |
| GP28 | Brake | ADC2 (アナログ入力) |

## 開発環境
- **PlatformIO**: PlatformIO Core, version 6.1.19
- **Board**: `pico` (Raspberry Pi Pico)
- **Core**: `earlephilhower` 版 Arduino-Pico
- **主要ライブラリ**:
  - `Adafruit TinyUSB`: USB HID / FFB 通信
  - `arduino-mcp2515`: CAN コントローラ制御

## プロジェクト構造と詳細設計
詳細な設計仕様については、以下のドキュメントを参照してください。

- [全体設計書 (SystemDesign.md)](./SystemDesign.md)
- [入出力モジュール設計書 (InputOutputModule.md)](./InputOutputModule.md)
- [ステアリング制御モジュール設計書 (SteeringModule.md)](./SteeringModule.md)
- [HID/FFBモジュール設計書 (HIDModule.md)](./HIDModule.md)

## 参照資料
- [LKTECH CAN PROTOCOL V2.35](http://en.lkmotor.cn/upload/20230706100134f.pdf)
- [Adafruit TinyUSB Library](https://github.com/adafruit/Adafruit_TinyUSB_Arduino)
- [arduino-mcp2515 Library](https://github.com/autowp/arduino-mcp2515)
