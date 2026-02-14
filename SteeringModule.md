# ステアリング制御モジュール設計書 (SteeringModule.md)

## 1. 目的
LKTECH [MF4015 モータ](http://en.lkmotor.cn/ProDetail.aspx?ProId=256)を使用したステアリングの角度取得、およびForce Feedback（FFB）制御のためのソフトウェアアーキテクチャと通信仕様を規定する。

## 2. システム構成とクラス設計
依存性の逆転と抽象化を導入し、特定の通信デバイスに依存しない設計を採用している。

### 2.1 使用クラス
- **`CANInterface` (抽象インターフェース)**:
  - CAN通信の基本操作（`sendFrame`, `readFrame`, `available`）を規定。上位レイヤーの実装をハードウェアから隔離する。
- **`MCP2515_Wrapper` (通信層)**:
  - `CANInterface` の具体的な実装クラス。
  - SPIピン（SCK, TX, RX）およびINTピンを管理。
  - **INTピン監視**: MCP2515の受信バッファをGPIO (INTピン) ポーリングで監視することで、SPI通信回数を削減し、Core 1 の制御ループ負荷を最小化している。
- **`MF4015_Driver` (ドライバ層)**:
  - `CANInterface` を利用して LKTECH プロトコルを実装。
  - モーターの状態（エンコーダ、速度、電流、温度）を保持。
  - 指令に応じたトルク制御コマンドの生成・送信を担当。

## 3. 通信仕様 (CAN Bus)
- **ビットレート**: 500kbps (16MHz Clock)
- **ノードID**: 0x141 (デフォルト ID: 1)
- **周期**: 1ms (`STEAR_CONT_INTERVAL_US`) - RP2040 Core 1 にて実行

## 4. 角度取得機能
1ms周期でモータへ要求を送信、または応答パケットを解析してエンコーダ値を更新する。

### 4.1 信号処理
- **解像度**: 16bit (0 ～ 65535) 
- **動作範囲**: 
  - 本機では ±30度（±5461）を有効範囲とする。
  - 取得した値は `-32767` ～ `32767` の 16bit 符号付き整数にスケーリングされ、USB HID（Z軸）として Core 0 へ渡される。

## 5. トルク制御機能 (FFB)
ホストPC（Assetto Corsa等）からの FFB 命令に基づき、モータへトルク電流指令を送信する。

### 5.1 コマンドと制限
- **送信コマンド**: `0xA1` (Torque closed loop control command)
- **トルク指令値**: int16_t (-2048 ～ 2048)
  - 対応電流: -16.5A ～ 16.5A (MF4015仕様)
- **安全設計**:
  - `sharedData.targetTorque` を介して常時更新。
  - 通信途絶やエラー検知時には自動的に `stop()` または `disable()` を実行。

## 6. 制御フロー (Core 1)
1. `canWrapper.available()` で受信確認（INTピン監視）。
2. データがあれば `canWrapper.readFrame()` で取得し、`mfMotor.parseFrame()` で解析・更新。
3. 共有メモリから取得した目標トルクに基づき、`mfMotor.setTorque()` で指令値を送信。
4. 最新のエンコーダ値を共有メモリへ書き戻し、Core 0 経由でPCへ送信。
