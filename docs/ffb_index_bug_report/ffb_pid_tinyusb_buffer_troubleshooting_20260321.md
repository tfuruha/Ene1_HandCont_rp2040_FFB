# FFB PID スロット管理 トラブルシューティング記録

**作成日**: 2026-03-21  
**対象プロジェクト**: `Ene1_HandCont_rp2040_FFB-1` (RP2040 + TinyUSB + Adafruit_USBD_HID)  
**テスト環境**: Microsoft Force Editor / DirectInput

---

## 1. 発生した問題

| # | 症状 |
|---|------|
| 1 | `effectBlockIndex` が常に **6** から始まる（期待値: 1）|
| 2 | 2個目のエフェクト追加で `DIERR_DEVICEFULL (0x80040201)` |
| 3 | Microsoft Force Editor に `Couldn't create the effect!` / `Effect is NULL, so can't play!` |

---

## 2. 原因の調査

### 2-1. 初期仮説（外れ）

- スロット管理のビットマップに未初期化領域がある
- `_last_allocated_idx` のリセットタイミングが不正
- `_prepare_pid_pool()` が参照実装の `FreeAllEffects()` に相当するリセットをしていない

### 2-2. 真の根本原因（TinyUSB API 仕様の誤解）

**`Adafruit_USBD_HID` の `get_report_callback` は `buffer` に `reportId` を含めない。**

```cpp
// TinyUSB のコールバック型定義
typedef uint16_t (*get_report_callback_t)(
    uint8_t report_id,          // ← reportId は引数として渡される
    hid_report_type_t report_type,
    uint8_t *buffer,            // ← ここには reportId を入れてはいけない
    uint16_t reqlen);
```

TinyUSB は内部で `reportId` を USB パケット先頭に自動付加してホストへ送信する。  
`buffer` に `reportId` を含む構造体を丸ごとコピーすると、**バイト列が1バイトずれる**。

#### 送信バイト列の比較

**修正前（誤）:**

| USB先頭 (TinyUSB自動) | buffer[0] | buffer[1] | buffer[2] | buffer[3] |
|-----------------------|-----------|-----------|-----------|-----------|
| `0x06` (reportId)     | `0x06` ← reportId の二重 | effectBlockIndex | loadStatus | ramPool(Lo) |

→ PC が受け取る `effectBlockIndex` の位置に **`0x06`** が入り、インデックス=6と誤解釈

**修正後（正）:**

| USB先頭 (TinyUSB自動) | buffer[0]          | buffer[1]  | buffer[2]    | buffer[3]    |
|-----------------------|--------------------|------------|--------------|--------------|
| `0x06` (reportId)     | effectBlockIndex=1 | loadStatus | ramPool(Lo) | ramPool(Hi) |

---

## 3. 適用した修正

### 修正A: PIDPool GET 時に全スロットをリセット

参照実装 (`ArduinoJoystickWithFFBLibrary`) の `FreeAllEffects()` 相当の処理を追加。  
DirectInput は `PIDPool` 取得後にデバイスが全スロット空きである状態を前提で動作する。

```cpp
static void _prepare_pid_pool(uint8_t *buf, uint16_t *len) {
    // 全スロット解放（参照実装の FreeAllEffects 相当）
    memset(_slot_used, 0, sizeof(_slot_used));
    _last_allocated_idx = 0;
    for (int i = 0; i < MAX_EFFECTS; i++) {
        core0_ffb_effects[i] = {0};
    }
    // ... PIDPool 応答を送信
}
```

### 修正B: GET Feature フォールバック割当の廃止

`_prepare_create_new_effect()` (GET Feature 側) にあった `_alloc_slot()` フォールバックを削除。  
スロット割当は `_hid_set_report_cb` の SET Feature 側のみで行う。

> **根拠**: Windows DirectInput は必ず `SET Feature 0x05 → GET Feature 0x06` の順でシーケンスを送る。両方で `_alloc_slot()` を呼ぶと二重消費になる。

### 修正C: Feature GET コールバックの buffer から reportId を除外（主要修正）

全 Feature GET 応答関数で `reportId` を除外したペイロードのみを `buffer` にコピーする。

```cpp
// 修正前（誤）
memcpy(buf, &resp, sizeof(resp));   // reportId を含む全構造体をコピー
*len = sizeof(resp);

// 修正後（正）
uint8_t *payload = (uint8_t *)&resp + 1;  // reportId の次のバイトから
uint16_t payloadLen = sizeof(resp) - 1;
memcpy(buf, payload, payloadLen);
*len = payloadLen;
```

対象関数:
- `_prepare_pid_block_load()`
- `_prepare_create_new_effect()`
- `_prepare_pid_pool()`

---

## 4. 修正後の動作確認（シリアルログ）

```
[FFB] PIDPool GET -> all slots reset        ← 修正A 動作確認
[FFB] alloc slot=1
[FFB] PIDBlockLoad: idx=1 status=1         ← idx=1 をPCへ正しく送信
05 01 FF 12   ← SetConstantForce: effectBlockIndex=1 ✅  (修正前は 06)
01 01 01 ...  ← SetEffect:        effectBlockIndex=1 ✅

[FFB] alloc slot=2
[FFB] PIDBlockLoad: idx=2 status=1         ← idx=2 をPCへ正しく送信
05 02 C3 F3   ← SetConstantForce: effectBlockIndex=2 ✅
0A 01 01 01   ← EffectOperation Idx=1 Start ✅
0A 02 01 01   ← EffectOperation Idx=2 Start ✅
```

---

## 5. 設計上の教訓（次回設計への活用）

### 5-1. TinyUSB の Feature Report GET コールバック仕様

> ⚠️ `get_report_callback_t` の `buffer` には **reportId を含めてはいけない**。  
> TinyUSB がUSBパケット先頭に `reportId` を自動付加する。

```
// 正しいパターン
uint16_t my_get_report_cb(uint8_t report_id, hid_report_type_t type,
                          uint8_t *buffer, uint16_t reqlen) {
    buffer[0] = payload_byte_1;  // reportId の次のフィールドから埋める
    buffer[1] = payload_byte_2;
    return 2;  // ペイロードのバイト数のみ返す
}
```

構造体を使う場合は `sizeof(struct) - 1` / `&struct + 1` でオフセットを取ること。

### 5-2. USB HID PID 初期化シーケンス（DirectInput）

```
① GET Feature 0x07 (PID Pool)         → 全スロット解放してから返す
② SET Feature 0x05 (Create New Effect) → スロット確保 (_last_allocated_idx にセット)
③ GET Feature 0x06 (PID Block Load)   → 確保インデックスを返し、_last_allocated_idx をリセット
④ OUT 0x01 (Set Effect)               → effectBlockIndex で対象スロット指定
⑤ OUT 0x05 (Set Constant Force)       → effectBlockIndex で対象スロット指定
⑥ OUT 0x0A (Effect Operation: Start)  → 再生開始
```

- **① の PIDPool GET は全スロット初期化のタイミング**
- **② SET Feature のみでスロット割当、③ GET Feature でのフォールバック割当は不要**

### 5-3. 参照実装との動作比較の重要性

参照コード (`ArduinoJoystickWithFFBLibrary / PIDReportHandler.cpp`) では:
- `getPIDPool()` → `FreeAllEffects()` を毎回呼ぶ
- スロット管理: `nextEID` の単調増加 + 解放時の巻き戻し

現行実装のビットマップ全探索方式の方が堅牢だが、  
**`getPIDPool()` でのリセット**は参照実装の動作に合わせる必要がある。

### 5-4. デバッグのコツ

シリアルログで `effectBlockIndex` の値を直接確認することが最短経路。

```cpp
// _alloc_slot でのログ
Serial.print("[FFB] alloc slot="); Serial.println(i);

// PIDBlockLoad 送信内容のログ
Serial.print("[FFB] PIDBlockLoad: idx="); Serial.print(resp.effectBlockIndex);
Serial.print(" status="); Serial.println(resp.loadStatus);
```

RAW バイト列と `effectBlockIndex` の値を照合すると、バイトずれを即座に検出できる。

---

## 6. 参考: Feature Report の構造体設計パターン

```cpp
// reportId を含む構造体定義（パース/生成の両用）
typedef struct {
    uint8_t reportId;          // USB PID: 常に先頭
    uint8_t effectBlockIndex;
    uint8_t loadStatus;
    uint16_t ramPoolAvailable;
} __attribute__((packed)) USB_FFB_Feature_PIDBlockLoad_t;

// GET Feature コールバックでの正しい使い方
static void _prepare_pid_block_load(uint8_t *buf, uint16_t *len) {
    USB_FFB_Feature_PIDBlockLoad_t resp;
    resp.reportId         = HID_ID_PID_BLOCK_LOAD;  // 構造体には入れるが...
    resp.effectBlockIndex = allocated_idx;
    resp.loadStatus       = HID_BLOCK_LOAD_SUCCESS;
    resp.ramPoolAvailable = available * 10;

    // buffer へは reportId を除いたペイロードのみコピー
    memcpy(buf, (uint8_t *)&resp + 1, sizeof(resp) - 1);
    *len = sizeof(resp) - 1;
}
```

---

*以上 — 2026-03-21 作成*
