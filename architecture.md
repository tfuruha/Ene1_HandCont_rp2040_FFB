# システム設計書：FFB対応ハンドルコントローラ

*   **作成日:** 2026-01-23
*   **最終更新日:** 2026-01-23

## 1. アーキテクチャ概要

本システムは、RP2040のデュアルコア性能を最大限に活用し、高精度なモーター制御と安定したUSB通信を両立させる設計を採用している。

### 1.1 コア役割分担
*   **Core 0 (通信/管理コア):**
    *   USB HID (TinyUSB) の維持。
    *   ホストPCからの FFB (Force Feedback) 信号の受信と解析。
    *   `sharedData.targetTorque` の更新。
*   **Core 1 (制御/計測コア):**
    *   1ms (1000us) 固定周期の制御ループ実行。
    *   CANバス (MCP2515) を通じた MF4015 モーターへのコマンド送信。
    *   センサー（ステアリングエンコーダ、アクセル、ブレーキ）の状態取得。

## 2. ソフトウェア・スタック

### 2.1 依存ライブラリ
*   **Core:** `Arduino-Pico` (earlephilhower版)
*   **USB:** `Adafruit_TinyUSB`
*   **CAN:** `autowp-mcp2515` (Wrapper経由で隠蔽)
*   **FileSystem:** `LittleFS` (設定保存)

### 2.2 モジュール構造
*   **`MCP2515_Wrapper` (lib/CAN_Bus):**
    *   `CANInterface` を実装し、MCP2515固有の実装を隔離。
*   **`MF4015_Driver` (lib/MF4015):**
    *   `CANInterface` 経由でモーターを操作。特定のハードウェアに依存しない。
*   **`ConfigManager` (src):**
    *   LittleFS を用いた `SharedData` の永続化管理。

## 3. データ共有 (Shared Memory)

Core間で共有される `SharedData` 構造体（`include/shared_data.h`）を介して通信を行う。

| フィールド | 方向 | 説明 |
| :--- | :--- | :--- |
| `steeringAngle` | C1 → C0 | 現在のハンドル読取位置 |
| `accelerator` | C1 → C0 | アクセルペダル値 |
| `brake` | C1 → C0 | ブレーキペダル値 |
| `buttons` | C1 → C0 | ボタン状態（シフト等） |
| `targetTorque` | C0 → C1 | ホストからのFFB目標値 |

## 4. 通信プロトコル仕様

### 4.1 CAN (Motor)
*   **レート:** 500kbps / 8MHz Clock
*   **ID:** 0x141 (MF4015)
*   **制御:** トルク制御 (0xA1コマンド) を主に使用

### 4.2 USB HID (PC)
*   **レポート周期:** 20ms
*   **HIDタイプ:** ゲームパッド/ジョイスティック
*   **軸:** X (Steering), Y (Accel), Z (Brake)
*   **ボタン:** 2個 (Button 0: Up, Button 1: Down)
