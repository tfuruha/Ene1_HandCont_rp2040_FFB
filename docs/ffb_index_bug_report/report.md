# FFB エフェクト・インデックス問題 調査レポート

**日付**: 2026-03-21  
**対象プロジェクト**: `Ene1_HandCont_rp2040_FFB-1`  
**比較プロジェクト**: `ArduinoJoystickWithFFBLibrary-master / FFBlib_Example`

---

## 1. 観察された問題

| # | 問題 | 参照実装 | 現行実装 |
|---|------|----------|----------|
| 1 | `effectBlockIndex` が 6 から始まる | 1 から開始 | 6 から開始 |
| 2 | 2個目の `ConstantForce` で `DIERR_DEVICEFULL (0x80040201)` | 1, 2 が正常割当 | 2個目を受け付けない |

---

## 2. 両実装のスロット管理の比較

### 2-1. 参照実装 (`PIDReportHandler.cpp`)

```cpp
// 初期化
nextEID = 1;                              // ← 1-indexed スタート

// スロット確保
uint8_t GetNextFreeEffect() {
    if (nextEID == MAX_EFFECTS) return 0; // 上限チェック (MAX=14)
    uint8_t id = nextEID++;               // 現在値を返してインクリメント
    while (g_EffectStates[nextEID].state != 0) {
        if (nextEID >= MAX_EFFECTS) break;
        nextEID++;
    }
    g_EffectStates[id].state = MEFFECTSTATE_ALLOCATED;
    return id;
}

// 解放
void FreeEffect(uint8_t id) {
    g_EffectStates[id].state = 0;
    if (id < nextEID) nextEID = id;  // ← 空きが出たら nextEID を巻き戻す
}

// PIDPool GET が呼ばれたとき FreeAllEffects() で nextEID=1 にリセット
uint8_t *getPIDPool() {
    FreeAllEffects();   // ← 毎回リセット
    ...
}
```

**配列サイズ**: `g_EffectStates[MAX_EFFECTS + 1]`（=15要素, 0は未使用, 1〜14が有効）

### 2-2. 現行実装 (`hidwffb.cpp`)

```cpp
static uint8_t _last_allocated_idx = 0;        // 直前に割り当てたスロット
static bool _slot_used[MAX_EFFECTS + 1] = {false}; // 0未使用, 1〜10が有効

static uint8_t _alloc_slot() {
    for (uint8_t i = 1; i <= MAX_EFFECTS; i++) {
        if (!_slot_used[i]) {
            _slot_used[i] = true;
            return i;
        }
    }
    return 0; // 空きなし
}
```

---

## 3. 根本原因の分析

### 原因①: PID Pool レポートの GET タイミングでスロットがリセットされていない

DirectInput の初期化シーケンスは以下の順序で進みます:

```
① GET Feature 0x07 (PID Pool)         → デバイス容量確認
② SET Feature 0x05 (Create New Effect) → スロット確保要求
③ GET Feature 0x06 (PID Block Load)   → 割当インデックスの取得
④ OUT 0x01 (Set Effect)               → エフェクト設定
⑤ OUT 0x05 (Set Constant Force)       → 力の設定
⑥ OUT 0x0A (Effect Operation: Start)  → 再生開始
```

参照実装では **① の時点で `FreeAllEffects()` → `nextEID = 1`** とリセットします。  
現行実装の `_prepare_pid_pool()` は単純にプール情報を返すだけで、スロット状態をリセットしません。

この差異が直接的な原因**ではない**可能性もありますが、初期化後の状態が異なります。

### 原因②: Feature SET / GET の順序と `_last_allocated_idx` のリセットタイミング

現行の Create New Effect / PID Block Load フローを追うと:

```
SET Feature 0x05 (Create New Effect)
  → _last_allocated_idx == 0 ならば _alloc_slot() を呼ぶ
  → _last_allocated_idx = 割り当てたスロット番号

GET Feature 0x06 (PID Block Load)
  → _last_allocated_idx を返す
  → _last_allocated_idx = 0 にリセット  ← 次の割当に備える
```

**問題**: Windows DirectInput は **`SET Feature 0x05` → `GET Feature 0x06`** というシーケンスを想定しているが、一部の実装では **`GET Feature 0x06` だけ**を送ってくる場合もある（`_hid_get_report_cb` の中で `_last_allocated_idx == 0` のときに `_alloc_slot()` を呼ぶフォールバックあり）。

> ⚠️ **インデックスが6になる直接原因の推定**

初期化シーケンス中、DirectInput は Device Control (Reset) → Device Gain → Create New Effect という順でコマンドを送ります。  
観察された現象からは、**Device Reset / Gain 処理後に `_last_allocated_idx` が 0 にリセットされず**（あるいは複数回 `_alloc_slot()` が呼ばれ）、既に 5 スロット分が消費されてから 1個目の ConstantForce が割当を受けている疑いがあります。

シリアルモニタの RAW データ出力が実装されているので、以下のデータを採取することで確認できます:

- デバイス接続後から最初の `ConstantForce` 設定までに受信する全パケット
- 特に `0x05 Feature SET` が何回呼ばれているか
- `_prepare_create_new_effect` の呼び出し回数と返却インデックス

### 原因③: `_prepare_create_new_effect` の Feature GET でのフォールバック割当

```cpp
// _hid_get_report_cb の中
case HID_ID_CREATE_NEW_EFFECT:   // 0x05
    _prepare_create_new_effect(buffer, &len);
    break;

// _prepare_create_new_effect の中
if (_last_allocated_idx == 0) {
    _last_allocated_idx = _alloc_slot();  // ← GET Feature でも割当が走る
}
```

**注意**: `HID_ID_CREATE_NEW_EFFECT (0x05)` と `HID_ID_SET_CONSTANT_FORCE (0x05)` は**同じ Report ID**です。  
Output Report と Feature Report は `report_type` で区別していますが、初期化中に GET Feature 0x05 が複数回呼ばれた場合、毎回この分岐は通りません（`_last_allocated_idx != 0` なら skip）。しかし GET Feature 0x05 と Output 0x05 の混在タイミングが問題になる可能性があります。

---

## 4. 「Device is full」エラーの原因

Microsoft Force Editor で2個目の ConstantForce を追加しようとすると `DIERR_DEVICEFULL` になります。

```
GET Feature 0x06 (PID Block Load) の応答:
  effectBlockIndex = 0
  loadStatus = 2 (HID_BLOCK_LOAD_FULL)
```

この状態になる理由:

1. インデックス 1〜5 が見えない形で消費されている（原因①②）
2. 1個目の ConstantForce がインデックス 6 を使用
3. 2個目の ConstantForce を割当時に、残り 4 スロット（7〜10）があるはずなのに 0 が返る

→ `_alloc_slot()` が 0 を返すということは `_slot_used[1..10]` が全て `true` になっている

---

## 5. 参照実装との構造的な違いまとめ

| 項目 | 参照実装 | 現行実装 |
|------|----------|----------|
| MAX_EFFECTS | 14 | 10 |
| 配列サイズ | `[MAX_EFFECTS + 1]` = 15要素 | `[MAX_EFFECTS + 1]` = 11要素 |
| スロット管理方式 | 単調増加 + 解放時巻戻し | ビットマップ全探索 |
| PIDPool GET 時 | `FreeAllEffects()` 呼出でリセット | リセットなし |
| Create New Effect | GET Feature でのみ割当 | SET/GET 両方で割当（フォールバック有） |
| インデックス開始値 | 1 | 1（のはずだが6になっている） |

---

## 6. 対応策

### 対策A: PIDPool GET 時にスロットをリセット（参照実装と同じ動作に合わせる）

```cpp
static void _prepare_pid_pool(uint8_t *buf, uint16_t *len) {
    // ← 追加: 参照実装の FreeAllEffects() 相当
    memset(_slot_used, 0, sizeof(_slot_used));
    _last_allocated_idx = 0;
    for (int i = 0; i < MAX_EFFECTS; i++) {
        core0_ffb_effects[i] = {0};
    }
    // 既存
    USB_FFB_Feature_PIDPool_t resp;
    resp.reportId = HID_ID_PID_POOL;
    ...
}
```

**理由**: DirectInput は PIDPool 取得時にデバイスが全スロット空きの状態を期待している可能性が高い。

### 対策B: デバッグ出力でシーケンスを確認する

シリアルモニタで以下を追加して原因特定を確認します:

```cpp
static uint8_t _alloc_slot() {
    for (uint8_t i = 1; i <= MAX_EFFECTS; i++) {
        if (!_slot_used[i]) {
            _slot_used[i] = true;
            Serial.print("[ALLOC] slot="); Serial.println(i);  // ← 追加
            return i;
        }
    }
    Serial.println("[ALLOC] FULL!");  // ← 追加
    return 0;
}
```

`_prepare_pid_pool()` と `_prepare_create_new_effect()` にも呼出ログを追加してシーケンスを追跡します。

### 対策C: Feature GET の `_prepare_create_new_effect` でのフォールバック割当を廃止

```cpp
static void _prepare_create_new_effect(uint8_t *buf, uint16_t *len) {
    // フォールバック割当は廃止 → SET Feature 側のみで割当
    // if (_last_allocated_idx == 0) {
    //     _last_allocated_idx = _alloc_slot();
    // }
    USB_FFB_Feature_CreateNewEffect_t resp;
    resp.reportId = HID_ID_CREATE_NEW_EFFECT;
    resp.effectType = 0;
    resp.byteCount = 0;
    memcpy(buf, &resp, sizeof(resp));
    *len = sizeof(resp);
}
```

**理由**: Windows DirectInput は必ず `SET Feature 0x05 → GET Feature 0x06` の順でシーケンスを送るため、GET Feature 側での割当は不要かつ誤動作の原因になりうる。

---

## 7. 推奨対応手順

1. **対策B（デバッグ追加）** → シリアルモニタで実際のシーケンスと `_alloc_slot()` 呼び出し回数を確認
2. **対策A** を試して、インデックスが 1 から始まるかを確認
3. **対策C** を組み合わせて、フォールバック割当を排除
4. Microsoft Force Editor で 2 個の ConstantForce が正常に動作することを確認

---

## 8. 参考: `GetNextFreeEffect()` のバグ（参照実装）

参照実装の `GetNextFreeEffect()` には以下のバグがあります:

```cpp
uint8_t id = nextEID++;  // id を取得してインクリメント
while (g_EffectStates[nextEID].state != 0) {  // ← nextEID が変わった後のチェック
    if (nextEID >= MAX_EFFECTS) break;
    nextEID++;
}
```

`nextEID++ (postfix)` で `id` に旧値が代入され `nextEID` は次に進む。その後 `while` で次に空いているスロットを探す。この実装は「割り当てた後、次の空きを事前にポイント」する方式なので、現行のビットマップ全探索の方が実は堅牢です。ただし `FreeEffect()` で `nextEID` を巻き戻す処理があり、**解放後の再利用** が正しく動作します。

---

*以上*
