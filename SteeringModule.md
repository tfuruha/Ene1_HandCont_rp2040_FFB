# ステアリング制御モジュール設計書 (SteeringModule.md)

## 1. 目的
LKTECH [MF4015 モータ](http://en.lkmotor.cn/ProDetail.aspx?ProId=256)を使用したステアリングの角度取得、および将来的なフォースフィードバック（FFB）実現のためのトルク制御仕様を規定する。

## 2. 通信仕様 (CAN Bus)
- **コントローラ**: MCP2515 (Clock: 16MHz)
- **ビットレート**: 500kbps
- **ノードID**: 1 (MF4015)
- **メッセージ形式**: 標準フレーム (Standard Frame), DLC 8bytes

### 参照資料
- **通信プロトコル仕様書**: [SHANGHAI LINGKONG TECHNOLOGY CO.,LTD CAN PROTOCOL V2.35](http://en.lkmotor.cn/upload/20230706100134f.pdf)
- **CANライブラリ**: [arduino-mcp2515 (GitHub)](https://github.com/autowp/arduino-mcp2515)

## 3. 角度取得機能
20ms周期のメインループでモータから現在のエンコーダ値を取得する。

### 3.1 コマンド
- **送信**: `0x90` (Read Encoder command)
- **受信データ変換**:
  - 16bit エンコーダ値 (0 ～ 65535) を取得。
  - 中心値を 32767 とし、±180度をカバーする。
  - 本機ではステアリングの遊びや剛性を考慮し、±30度（エンコーダ値換算で ±5461）を動作範囲とする。

## 4. トルク制御機能 (将来的なFFB対応)
ゲーム（Assetto Corsa等）からの振動や反力指令をモータのトルク指令に変換する。

### 4.1 コマンド
- **送信コマンド**: `0xA1` (Torque closed loop control command)
- **パラメータ**: `iqControl` (int16_t)
  - 範囲: -2048 ～ 2048 (対応電流: -16.5A ～ 16.5A)
- **実装方針**:
  - ホストPCから受信したFFB強度（0-100%）を、モータの定格電流および許容温度の範囲内で `iqControl` 値にマッピングする。
  - 安全のため、ソフトウェアによる電流制限および緊急停止（Motor Off）機能を備える。

## 5. モータ保護・状態管理
- **状態監視**: サーモセンサによる温度取得、過電流エラーの検知（`0x9A` コマンド）。
- **初期化フロー**:
  1. MCP2515 初期化
  2. モータ電源投入 (`0x88` Motor On)
  3. エンコーダ値取得開始
