# システム設計書：FFB対応ハンドルコントローラ

*   **作成日:** 2026-01-23
*   **最終更新日:** 2026-01-23

## 1. アーキテクチャ概要

本システムは、RP2040のデュアルコア性能を最大限に活用し、高精度なモーター制御と安定したUSB通信を両立させる設計を採用している。

### 1.1 コア役割分担
*   **Core 0 (通信/管理コア):**
    *   USB HID (TinyUSB) の維持。
    *   ホストPCからの FFB (Force Feedback) 信号の受信と解析。
    *   解析結果（PID情報）を共有メモリへ更新 (`ffb_core0_update_shared`)。
*   **Core 1 (制御/計測コア):**
    *   1ms (1000us) 固定周期の制御ループ実行。
    *   センサー（ステアリングエンコーダ、アクセル、ブレーキ）の状態取得とPC向け入力レポートの更新 (`ffb_core1_update_shared`)。
    *   `FFBEngine` によるエフェクト合力演算と `sharedData.targetTorque` の算出。
    *   CANバス (MCP2515) を通じた MF4015 モーターへのコマンド送信。

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

Core間のデータ共有は目的に応じて以下の仕組みで行われる。

### 3.1 HID / FFB 共有メモリ (`hidwffb.h`)
物理入力およびFFBエフェクト情報は、ミューテックスを用いた専用の共有メモリ機構で安全に受け渡しされる。
*   **PC向け入力:** Core 1 が `custom_gamepad_report_t` を更新し、Core 0 が送信。
*   **PCからのFFB:** Core 0 がPID解析結果を書き込み、Core 1 が `FFB_Shared_State_t` の配列として読み出し。

### 3.2 システムステータス (`include/shared_data.h`)
`SharedData` 構造体はシステム全体のステータス確認用として機能する。

| フィールド | 方向 | 説明 |
| :--- | :--- | :--- |
| `targetTorque` | C1 → C0/その他 | C1で算出されたモータートルク指令値（監視用） |
| `core1LoopCount` | C1 → C0/その他 | C1が正常動作しているか確認するカウンタ |
| `lastCore1Micros` | C1 → C0/その他 | C1の最終実行時刻（タイムアウト検出用） |

## 4. 通信プロトコル仕様

### 4.1 CAN (Motor)
*   **レート:** 500kbps / 16MHz Clock
*   **ID:** 0x141 (MF4015)
*   **制御:** トルク制御 (0xA1コマンド) を主に使用

### 4.2 USB HID (PC)
*   **レポート周期:** 20ms
*   **HIDタイプ:** ゲームパッド/ジョイスティック
*   **軸:** X (Steering), Y (Accel), Z (Brake)
*   **ボタン:** 2個 (Button 0: Up, Button 1: Down)
