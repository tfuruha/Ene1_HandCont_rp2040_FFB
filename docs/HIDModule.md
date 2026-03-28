# USB HID/FFBモジュール設計書 (HIDModule.md)

最終更新: 2026-03-21

---

## 1. 目的

PCホストに対して、デバイスを標準的なゲームコントローラ（Joystick）および  
PID（Physical Interface Device）として認識させ、双方向の低遅延通信を実現する。

---

## 2. デバイス構成

| 項目 | 内容 |
| :--- | :--- |
| USBクラス | HID (Human Interface Device) |
| USBスタック | `Adafruit_TinyUSB` (Adafruit_USBD_HID) |
| HIDディスクリプタ | `hid_pid_descriptor.h` (USB PID Spec v1.0 準拠) |
| 管理コア | Core 0 (HID通信・FFBパケット解析) |
| 同時管理エフェクト数 | `MAX_EFFECTS = 10` |
| FFB受信バッファ | `HID_FFB_REPORT_SIZE = 64` バイト |

---

## 3. レポートID 一覧

### 3.1 Input Reports (Device → Host)

| Report ID | 用途 | 型定義 | 送信周期 |
| :--- | :--- | :--- | :--- |
| `0x01` | Joystick 入力 (Steer/Accel/Brake/Buttons) | `custom_gamepad_report_t` | 1ms |
| `0x02` | PID State Report (エフェクト再生状態) | — (rawペイロード2バイト) | イベント時 |

### 3.2 Output Reports (Host → Device / FFBコマンド)

| Report ID | 用途 | 型定義 | パーサ処理 |
| :--- | :--- | :--- | :--- |
| `0x01` | Set Effect (エフェクト種別・ゲイン) | `USB_FFB_Report_SetEffect_t` | `core0_ffb_effects[idx].type / gain` |
| `0x02` | Set Envelope | `USB_FFB_Report_SetEnvelope_t` | (未実装) |
| `0x03` | Set Condition (Spring/Damper/Inertia/Friction) | `USB_FFB_Report_SetCondition_t` | `cpOffset / Coeff / Saturation / deadBand` |
| `0x04` | Set Periodic (Sine/Square等) | `USB_FFB_Report_SetPeriodic_t` | `periodicMagnitude / Offset / Phase / Period` |
| `0x05` | Set Constant Force | `USB_FFB_Report_SetConstantForce_t` | `core0_ffb_effects[idx].magnitude` |
| `0x06` | Set Ramp Force | — | (未実装) |
| `0x0A` | Effect Operation (Start/Stop) | `USB_FFB_Report_EffectOperation_t` | `active / startTimeUs` |
| `0x0B` | PID Block Free (スロット解放) | `USB_FFB_Report_PIDBlockFree_t` | `_free_slot()` + 全パラメータクリア |
| `0x0C` | PID Device Control (Reset/Stop All) | `USB_FFB_Report_DeviceControl_t` | 全スロット解放 + `_slot_used[]` クリア |
| `0x0D` | Device Gain (マスターゲイン) | `USB_FFB_Report_DeviceGain_t` | `core0_global_gain` |

### 3.3 Feature Reports (Host ↔ Device / 初期化シーケンス)

| Report ID | 方向 | 用途 | 型定義 | 実装関数 |
| :--- | :--- | :--- | :--- | :--- |
| `0x05` | SET (Host→Device) | Create New Effect (スロット確保要求) | `USB_FFB_Feature_CreateNewEffect_t` | `_hid_set_report_cb` |
| `0x05` | GET (Device→Host) | Create New Effect 応答 | `USB_FFB_Feature_CreateNewEffect_t` | `_prepare_create_new_effect()` |
| `0x06` | GET (Device→Host) | PID Block Load (スロット番号の返却) | `USB_FFB_Feature_PIDBlockLoad_t` | `_prepare_pid_block_load()` |
| `0x07` | GET (Device→Host) | PID Pool (デバイス容量情報) | `USB_FFB_Feature_PIDPool_t` | `_prepare_pid_pool()` |

> ⚠️ **TinyUSB 実装上の注意**  
> `get_report_callback` の `buffer` 引数には **reportId を含めてはいけない**。  
> TinyUSBが reportId をUSBパケット先頭へ自動付加するため、  
> 構造体から `reportId` を除いたペイロード部分 (`&resp + 1`, `sizeof(resp) - 1`) のみコピーすること。

---

## 4. 入力レポート詳細 (Joystick / ID:0x01)

送信周期: 1ms (`Config::Time::HIDREPO_INTERVAL_MS`)

### 4.1 軸入力 (Axes)

| フィールド | 軸 | 値域 | 元データ | 備考 |
| :--- | :--- | :--- | :--- | :--- |
| `steer` | X (0x30) | −32767 ～ 32767 | MF4015 エンコーダ値 | 符号付き 16bit |
| `dummy_y` | Y (0x31) | −32767 ～ 32767 | (固定値 0) | Microsoft Force Editor 認識用 Y軸 |
| `accel` | Z (0x32) | 0 ～ 65535 | ADC0 サンプリング値 | 符号なし 16bit |
| `brake` | Rz (0x35) | 0 ～ 65535 | ADC2 サンプリング値 | 符号なし 16bit |

### 4.2 ボタン入力

- **構成**: 16ボタン（現状2ボタン使用）
- **Button 1**: Shift Up (`Config::Pin::SHIFT_UP`)
- **Button 2**: Shift Down (`Config::Pin::SHIFT_DOWN`)

---

## 5. FFBエフェクト スロット管理

### 5.1 スロット方式

方式: **ビットマップ全探索 (フリーリスト方式)**  
配列: `_slot_used[MAX_EFFECTS + 1]` (インデックス 0 は未使用、1〜10 が有効)

| 関数 | 処理 |
| :--- | :--- |
| `_alloc_slot()` | インデックス 1 から順に空きを探し、最初の空きを確保して返す |
| `_free_slot(idx)` | 指定インデックスを解放 (`_slot_used[idx] = false`) |
| `_available_slots()` | 空きスロット数を返す (PID Pool 応答の `ramPoolAvailable` 計算用) |

### 5.2 エフェクト作成シーケンス (DirectInput)

```text
① GET Feature 0x07 (PID Pool)          → 全スロット解放 → デバイス容量を返す
② SET Feature 0x05 (Create New Effect) → _alloc_slot() → _last_allocated_idx にセット
③ GET Feature 0x06 (PID Block Load)    → _last_allocated_idx を返す → 0 にリセット
④ OUT 0x01 (Set Effect)                → effectBlockIndex でスロット指定
⑤ OUT 0x05 (Set Constant Force)        → magnitude を設定
⑥ OUT 0x0A (Effect Operation: Start)   → active=true / startTimeUs 記録
```

### 5.3 スロットリセットのタイミング

| トリガー | 処理 |
| :--- | :--- |
| GET Feature 0x07 (PID Pool) | 全スロット解放 (`memset(_slot_used,0)` + `core0_ffb_effects` クリア) |
| OUT 0x0C DeviceControl Reset/StopAll | 全スロット解放 + 全エフェクト停止 |
| OUT 0x0B PID Block Free | 指定インデックスのスロット解放 |

---

## 6. 対応エフェクト種別

| HID_ET値 | エフェクト名 | 使用レポート | `FFB_Shared_State_t` フィールド |
| :--- | :--- | :--- | :--- |
| `0x01` | Constant Force | 0x05 SetConstantForce | `magnitude` |
| `0x03` | Square | 0x04 SetPeriodic | `periodicMagnitude / Offset / Phase / Period` |
| `0x04` | Sine | 0x04 SetPeriodic | 同上 |
| `0x05` | Triangle | 0x04 SetPeriodic | 同上 |
| `0x06` | Sawtooth Up | 0x04 SetPeriodic | 同上 |
| `0x07` | Sawtooth Down | 0x04 SetPeriodic | 同上 |
| `0x08` | Spring | 0x03 SetCondition | `cpOffset / positiveCoeff / negativeCoeff / ...` |
| `0x09` | Damper | 0x03 SetCondition | 同上 |
| `0x0A` | Inertia | 0x03 SetCondition | 同上 |
| `0x0B` | Friction | 0x03 SetCondition | 同上 |

---

## 7. モジュール間連携 (Core 0 ↔ Core 1)

```text
[Core 0]                              [Core 1]
hidwffb.cpp                           ffb_engine.cpp / main.cpp
  ↓ FFBコマンド受信・解析              ↑ 1ms モーター制御ループ
  ↓ core0_ffb_effects[] を更新        |
  ↓ ffb_core0_update_shared()  ─────→ shared_ffb_effects[] (mutex保護)
                                      ↓ ffb_core1_update_shared()
  ↑ hidwffb_send_report()      ←───── shared_input_report
  | Steer/Accel/Brake/Buttons を送信
```

- 共有変数: `shared_ffb_effects[MAX_EFFECTS]`, `shared_global_gain`, `shared_input_report`
- 排他制御: `ffb_shared_mutex` (pico/mutex.h)
- タイムアウト: `mutex_enter_timeout_ms(..., 1)` (1ms でデッドロック回避)

---

## 8. 公開 API 一覧 (`hidwffb.h`)

| 関数 | 用途 |
| :--- | :--- |
| `hidwffb_begin(poll_ms)` | USB HID 初期化・コールバック登録 |
| `hidwffb_is_mounted()` | USBマウント確認 |
| `hidwffb_wait_for_mount()` | マウント待機 (ブロッキング) |
| `hidwffb_ready()` | 送信可能状態確認 |
| `hidwffb_send_report(report)` | Joystick 入力レポート送信 |
| `hidwffb_sends_pid_state(idx, playing)` | PID State 送信 |
| `hidwffb_get_ffb_data(buffer)` | FFB 生データ取得フラグ確認 |
| `PID_ParseReport(buffer, size)` | FFB レポートパース・共有変数更新 |
| `ffb_shared_memory_init()` | mutex 初期化 |
| `ffb_core0_update_shared(info)` | Core0 → 共有メモリへPID解析結果を書き込み |
| `ffb_core0_get_input_report(input)` | 共有メモリ → Core0 へ物理入力を読み出し |
| `ffb_core1_update_shared(input, effects)` | 共有メモリ → Core1 へFFB読出 & 物理入力書込 |
