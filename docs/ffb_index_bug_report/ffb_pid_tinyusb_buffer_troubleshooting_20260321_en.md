# FFB PID Slot Management Troubleshooting Record

**Date**: March 21, 2026  
**Target Project**: `Ene1_HandCont_rp2040_FFB-1` (RP2040 + TinyUSB + Adafruit_USBD_HID)  
**Test Environment**: Microsoft Force Editor / DirectInput

---

## 1. Issue Description

| # | Symptom |
|---|---------|
| 1 | `effectBlockIndex` always starts at **6** (Expected: 1) |
| 2 | Adding a second effect results in `DIERR_DEVICEFULL (0x80040201)` |
| 3 | Microsoft Force Editor shows: `Couldn't create the effect!` / `Effect is NULL, so can't play!` |

---

## 2. Root Cause Investigation

### 2-1. Initial Hypotheses (Incorrect)

- Uninitialized areas in the slot management bitmap.
- Incorrect reset timing for `_last_allocated_idx`.
- `_prepare_pid_pool()` is missing the equivalent slot reset behavior found in the reference implementation's `FreeAllEffects()`.

### 2-2. Actual Root Cause (Misunderstanding of TinyUSB API Specifications)

**The `get_report_callback` in `Adafruit_USBD_HID` requires that the `reportId` is NOT included within `buffer`.**

```cpp
// TinyUSB callback type definition
typedef uint16_t (*get_report_callback_t)(
    uint8_t report_id,          // ← reportId is passed as an argument
    hid_report_type_t report_type,
    uint8_t *buffer,            // ← reportId must NOT be placed inside this buffer
    uint16_t reqlen);
```

TinyUSB automatically adds the `reportId` to the beginning of the USB packet before sending it to the host.
If the struct copied to `buffer` includes the `reportId`, **the entire byte sequence shifts by 1 byte**.

#### Transmission Byte Sequence Comparison

**Before Fix (Incorrect):**

| USB Head (TinyUSB Auto) | buffer[0] | buffer[1] | buffer[2] | buffer[3] |
|-------------------------|-----------|-----------|-----------|-----------|
| `0x06` (reportId)       | `0x06` ← Duplicate reportId | effectBlockIndex | loadStatus | ramPool(Lo) |

→ The Host OS interprets `0x06` as the `effectBlockIndex`, mistakenly assuming index=6.

**After Fix (Correct):**

| USB Head (TinyUSB Auto) | buffer[0]          | buffer[1]  | buffer[2]    | buffer[3]    |
|-------------------------|--------------------|------------|--------------|--------------|
| `0x06` (reportId)       | effectBlockIndex=1 | loadStatus | ramPool(Lo)  | ramPool(Hi)  |

---

## 3. Applied Fixes

### Fix A: Reset All Slots on PIDPool GET

Added logic equivalent to `FreeAllEffects()` from our reference implementation (`ArduinoJoystickWithFFBLibrary`).
DirectInput relies on the device having all slots clear after fetching the `PIDPool`.

```cpp
static void _prepare_pid_pool(uint8_t *buf, uint16_t *len) {
    // Free all slots (equivalent to FreeAllEffects in reference impl)
    memset(_slot_used, 0, sizeof(_slot_used));
    _last_allocated_idx = 0;
    for (int i = 0; i < MAX_EFFECTS; i++) {
        core0_ffb_effects[i] = {0};
    }
    // ... Send PIDPool response
}
```

### Fix B: Remove Fallback Allocation in GET Feature

Removed the fallback `_alloc_slot()` call in `_prepare_create_new_effect()` (the GET Feature side).
Slot allocation will strictly be handled only on the SET Feature side in `_hid_set_report_cb`.

> **Reasoning**: Windows DirectInput predictably sends a fixed sequence of `SET Feature 0x05 → GET Feature 0x06`. Allocating in both places results in double consumption of slots.

### Fix C: Omit `reportId` from the GET Feature Callback `buffer` (Main Fix)

In all Feature GET response functions, only copy the payload excluding the `reportId` into `buffer`.

```cpp
// Before fix (Incorrect)
memcpy(buf, &resp, sizeof(resp));   // Copies full struct including reportId
*len = sizeof(resp);

// After fix (Correct)
uint8_t *payload = (uint8_t *)&resp + 1;  // Start from the byte following reportId
uint16_t payloadLen = sizeof(resp) - 1;
memcpy(buf, payload, payloadLen);
*len = payloadLen;
```

Updated functions:
- `_prepare_pid_block_load()`
- `_prepare_create_new_effect()`
- `_prepare_pid_pool()`

---

## 4. Verification After Fixes (Serial Logs)

```
[FFB] PIDPool GET -> all slots reset        ← Fix A verified
[FFB] alloc slot=1
[FFB] PIDBlockLoad: idx=1 status=1         ← idx=1 successfully sent to PC
05 01 FF 12   ← SetConstantForce: effectBlockIndex=1 ✅  (Was 06 before fix)
01 01 01 ...  ← SetEffect:        effectBlockIndex=1 ✅

[FFB] alloc slot=2
[FFB] PIDBlockLoad: idx=2 status=1         ← idx=2 successfully sent to PC
05 02 C3 F3   ← SetConstantForce: effectBlockIndex=2 ✅
0A 01 01 01   ← EffectOperation Idx=1 Start ✅
0A 02 01 01   ← EffectOperation Idx=2 Start ✅
```

---

## 5. Architectural Takeaways (Notes for Future Designs)

### 5-1. TinyUSB Feature Report GET Callback Specification

> ⚠️ The `buffer` in `get_report_callback_t` **must not include the reportId**.
> TinyUSB automatically prepends the `reportId` to the beginning of the USB packet.

```cpp
// Correct Pattern
uint16_t my_get_report_cb(uint8_t report_id, hid_report_type_t type,
                          uint8_t *buffer, uint16_t reqlen) {
    buffer[0] = payload_byte_1;  // Fill from the field exactly after reportId
    buffer[1] = payload_byte_2;
    return 2;  // Return the byte count of the payload ONLY
}
```

If utilizing `struct` objects, always determine the correctly shifted offset via `sizeof(struct) - 1` or `&struct + 1`.

### 5-2. USB HID PID Initialization Sequence (DirectInput)

```
① GET Feature 0x07 (PID Pool)         → Clear all slots and then return
② SET Feature 0x05 (Create New Effect) → Allocate a slot (set to _last_allocated_idx)
③ GET Feature 0x06 (PID Block Load)   → Return the allocated index; reset _last_allocated_idx
④ OUT 0x01 (Set Effect)               → Specify target slot via effectBlockIndex
⑤ OUT 0x05 (Set Constant Force)       → Specify target slot via effectBlockIndex
⑥ OUT 0x0A (Effect Operation: Start)  → Begin playback
```

- **① The PIDPool GET serves as the timing mechanism to reset all slots.**
- **② Slot allocation occurs ONLY on SET Feature. No fallback allocation is needed on ③ GET Feature.**

### 5-3. Importance of Behavioral Comparison with Reference Implementation

In our baseline code (`ArduinoJoystickWithFFBLibrary / PIDReportHandler.cpp`):
- `getPIDPool()` -> Repeatedly calls `FreeAllEffects()`
- Slot Management: Monotonically increasing `nextEID`, rewinding upon release

While our bitmap full-search method is structurally more robust,
our **`getPIDPool()` reset behavior** needs to align perfectly with the reference logic.

### 5-4. Debugging Tips

Directly visualizing the value of `effectBlockIndex` from the Serial Monitor provides the shortest path to solving similar issues.

```cpp
// Log string at slot allocation
Serial.print("[FFB] alloc slot="); Serial.println(i);

// Log output for PIDBlockLoad contents
Serial.print("[FFB] PIDBlockLoad: idx="); Serial.print(resp.effectBlockIndex);
Serial.print(" status="); Serial.println(resp.loadStatus);
```

By comparing raw byte sequences against the expected `effectBlockIndex`, we were able to quickly pinpoint the 1-byte misalignment.

---

## 6. Reference: Structural Design Pattern for Feature Reports

```cpp
// Struct definition including reportId (used for both parsing/generating)
typedef struct {
    uint8_t reportId;          // USB PID: Always the first element
    uint8_t effectBlockIndex;
    uint8_t loadStatus;
    uint16_t ramPoolAvailable;
} __attribute__((packed)) USB_FFB_Feature_PIDBlockLoad_t;

// Correct usage within the GET Feature callback
static void _prepare_pid_block_load(uint8_t *buf, uint16_t *len) {
    USB_FFB_Feature_PIDBlockLoad_t resp;
    resp.reportId         = HID_ID_PID_BLOCK_LOAD;  // Explicitly set within struct, BUT...
    resp.effectBlockIndex = allocated_idx;
    resp.loadStatus       = HID_BLOCK_LOAD_SUCCESS;
    resp.ramPoolAvailable = available * 10;

    // Buffer should ONLY contain the payload copied past the reportId byte
    memcpy(buf, (uint8_t *)&resp + 1, sizeof(resp) - 1);
    *len = sizeof(resp) - 1;
}
```

---

*End — Created March 21, 2026*
