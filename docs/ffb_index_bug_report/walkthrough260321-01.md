# FFBインデックス問題 修正 Walkthrough

**実施日**: 2026-03-21  
**対象ファイル**: `src/hidwffb.cpp`

---

## 実施した修正内容

### 対策A: PIDPool GET時に全スロットをリセット

`_prepare_pid_pool()` に全スロット解放処理を追加。  
参照実装 (`ArduinoJoystickWithFFBLibrary`) の `FreeAllEffects()` と同等の動作にした。

**修正前**: PIDPool情報を返すだけ  
**修正後**: `_slot_used` / `_last_allocated_idx` / `core0_ffb_effects` をゼロクリア → PIDPool情報を返す

```diff
 static void _prepare_pid_pool(uint8_t *buf, uint16_t *len) {
+  memset(_slot_used, 0, sizeof(_slot_used));
+  _last_allocated_idx = 0;
+  for (int i = 0; i < MAX_EFFECTS; i++) {
+    core0_ffb_effects[i].active    = false;
+    core0_ffb_effects[i].magnitude = 0;
+    core0_ffb_effects[i].type      = 0;
+    core0_ffb_effects[i].gain      = 0;
+  }
+  Serial.println("[FFB] PIDPool GET -> all slots reset");
   USB_FFB_Feature_PIDPool_t resp;
```

### 対策B: デバッグシリアル出力の追加

`_alloc_slot()` が呼ばれるたびに割当スロット番号または満杯をシリアル出力。  
`_prepare_create_new_effect()` / `_prepare_pid_pool()` にも呼出ログを追加。

```
[FFB] PIDPool GET -> all slots reset
[FFB] CreateNewEffect GET, last_alloc=1
[FFB] alloc slot=1
[FFB] alloc slot=2
```

### 対策C: GET Feature フォールバック割当の廃止

`_prepare_create_new_effect()` (GET Feature 0x05応答) の中で行っていた  
`_alloc_slot()` フォールバック呼出を廃止。  
Windows DirectInput は必ず `SET Feature 0x05 → GET Feature 0x06` の順でシーケンスを送るため、  
`_hid_set_report_cb` (SET Feature 側) のみでスロットを割当てる。

**修正前**: GET Feature でも `_last_allocated_idx == 0` なら `_alloc_slot()` を呼んでいた  
**修正後**: フォールバック割当を削除（ログ出力のみ追加）

---

## ビルド結果

```
Processing pico (platform: ...; board: pico; framework: arduino)
Compiling .pio/build/pico/src/hidwffb.cpp.o
Linking .pio/build/pico/firmware.elf
...
RAM:   [=         ]   5.7% (used 15000 bytes from 262144 bytes)
Flash: [=         ]   7.8% (used 122720 bytes from 1568768 bytes)
========================= [SUCCESS] Took 3.84 seconds =========================
```

ビルドエラーなし。

---

## 動作確認手順（実機テスト）

1. ファームウェアを書き込み、シリアルモニタ（115200bps）を開く
2. Microsoft Force Editor でデバイスに接続する
3. シリアル出力を確認:
   - `[FFB] PIDPool GET -> all slots reset` が最初に出力される
   - `[FFB] alloc slot=1` → 1個目の ConstantForce が slot=1 に割当
   - `[FFB] alloc slot=2` → 2個目の ConstantForce が slot=2 に割当
4. Microsoft Force Editor でエラーなしに2個の ConstantForce が追加できることを確認

---

## 注意事項

- シリアルデバッグ出力（`[FFB] ...`）は本番運用では削除またはマクロガードすること
- `_prepare_create_new_effect()` で SET Feature 側のみ割当とする変更は、  
  Windows 以外の DirectInput 実装（GET Feature のみで割当する環境）では動作しない可能性がある。  
  その場合は対策Cをリバートして対策Aのみを適用すること。
