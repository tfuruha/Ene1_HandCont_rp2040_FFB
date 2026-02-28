/**
 * @file hidwffb.cpp
 * @brief HID Joystick + FFB制御モジュール実装 (USB PID仕様準拠)
 */

#include "hidwffb.h"
#include "hid_pid_descriptor.h" // HIDレポートディスクリプタ (USB PID仕様準拠)
#include <stddef.h>
#include <string.h>

// ============================================================================
// モジュール内部状態
//   ※ HIDレポートディスクリプタは hid_pid_descriptor.h を参照
// ============================================================================

static Adafruit_USBD_HID _usb_hid;

static uint8_t _ffb_data[HID_FFB_REPORT_SIZE];
static volatile bool _ffb_updated = false;

static pid_debug_info_t _pid_debug = {0, false, 0, 0, false};
static FFB_Shared_State_t core0_ffb_effects[MAX_EFFECTS];
static uint8_t core0_global_gain = 255;
static uint8_t _next_block_index = 1; ///< 次に割り当てるスロット (1-based)
static uint8_t _last_allocated_idx =
    0; ///< 直前に割り当てたスロット (PID Block Load応答用)

// ============================================================================
// Feature Report レスポンス生成
// ============================================================================

/**
 * @brief Create New Effect GET Feature応答
 *
 * DirectInputは実装により SETまたは GETどちらでブロック生成を伝えるため、
 * どちらで呼ばれてもブロックを割り当てる。
 */
static void _prepare_create_new_effect(uint8_t *buf, uint16_t *len) {
  // GET Feature 0x05 でのブロック割当 (SET
  // Featureで未割当の場合のフォールバック)
  if (_last_allocated_idx == 0) {
    if (_next_block_index <= MAX_EFFECTS) {
      _last_allocated_idx = _next_block_index;
      _next_block_index++;
    }
  }
  USB_FFB_Feature_CreateNewEffect_t resp;
  resp.reportId = HID_ID_CREATE_NEW_EFFECT;
  resp.effectType = 0;
  resp.byteCount = 0;
  memcpy(buf, &resp, sizeof(resp));
  *len = sizeof(resp);
}

/**
 * @brief PID Block Load GET Feature応答
 *        Create New Effectにより割り当てたスロット情報を返す。
 *        読み出し後は _last_allocated_idx をリセットし、次回の割当に備える。
 */
static void _prepare_pid_block_load(uint8_t *buf, uint16_t *len) {
  USB_FFB_Feature_PIDBlockLoad_t resp;
  resp.reportId = HID_ID_PID_BLOCK_LOAD;
  if (_last_allocated_idx > 0 && _last_allocated_idx <= MAX_EFFECTS) {
    resp.effectBlockIndex = _last_allocated_idx;
    resp.loadStatus = HID_BLOCK_LOAD_SUCCESS;
    resp.ramPoolAvailable =
        (uint16_t)(MAX_EFFECTS - (_next_block_index - 1)) * 10;
  } else {
    resp.effectBlockIndex = 0;
    resp.loadStatus = HID_BLOCK_LOAD_FULL;
    resp.ramPoolAvailable = 0;
  }
  // 読み出し後はリセット (次回 Create New Effect で新たに割り当てる)
  _last_allocated_idx = 0;
  memcpy(buf, &resp, sizeof(resp));
  *len = sizeof(resp);
}

static void _prepare_pid_pool(uint8_t *buf, uint16_t *len) {
  USB_FFB_Feature_PIDPool_t resp;
  resp.reportId = HID_ID_PID_POOL;
  resp.ramPoolSize = MAX_EFFECTS * 10;
  resp.simultaneousEffects = MAX_EFFECTS;
  resp.memoryManagement = 0; // Device managed = 0, shared params = 0
  memcpy(buf, &resp, sizeof(resp));
  *len = sizeof(resp);
}

// ============================================================================
// Adafruit_USBD_HID コールバック (setReportCallback で登録)
// ============================================================================

/**
 * @brief Feature Report Get コールバック
 *        DirectInput がFFB初期化時に問い合わせる Feature Report に応答する
 */
static uint16_t _hid_get_report_cb(uint8_t report_id,
                                   hid_report_type_t report_type,
                                   uint8_t *buffer, uint16_t reqlen) {
  (void)reqlen;
  if (report_type != HID_REPORT_TYPE_FEATURE)
    return 0;

  uint16_t len = 0;
  switch (report_id) {
  case HID_ID_CREATE_NEW_EFFECT:
    _prepare_create_new_effect(buffer, &len);
    break;
  case HID_ID_PID_BLOCK_LOAD:
    _prepare_pid_block_load(buffer, &len);
    break;
  case HID_ID_PID_POOL:
    _prepare_pid_pool(buffer, &len);
    break;
  default:
    break;
  }
  return len;
}

/**
 * @brief HID Output / Feature Report 受信コールバック (SET方向)
 *
 * DirectInput のFFBエフェクト作成シーケンス:
 *   1. Host  →  Device: SET Feature 0x05 (Create New Effect)
 *                         ↑ここでスロット割当し _last_allocated_idx に記録
 *   2. Host  ←  Device: GET Feature 0x06 (PID Block Load)
 *                         ↑_hid_get_report_cb が _last_allocated_idx を返す
 *   3. Host  →  Device: Output 0x01 (Set Effect)
 *   4. Host  →  Device: Output 0x05 (Set Constant Force etc.)
 *   5. Host  →  Device: Output 0x0A (Effect Operation: Start)
 */
static void _hid_set_report_cb(uint8_t report_id, hid_report_type_t report_type,
                               uint8_t const *buffer, uint16_t bufsize) {
  // --- Feature SET: Create New Effect (0x05) ---
  // Output 0x05 (Set Constant Force) と同一IDだが、report_typeで区別する
  if (report_type == HID_REPORT_TYPE_FEATURE) {
    if (report_id == HID_ID_CREATE_NEW_EFFECT) {
      // SET Feature でのブロック割当
      // 未割当の場合のみ割り当てる (GET側で割り当て済みの場合は上書きしない)
      if (_last_allocated_idx == 0) {
        if (_next_block_index <= MAX_EFFECTS) {
          _last_allocated_idx = _next_block_index;
          _next_block_index++;
        }
      }
    }
    // その他のFeature SETは無視 (PID Block LoadはGET専用)
    return;
  }

  // --- Output SET: FFB コマンド ---
  if (report_type != HID_REPORT_TYPE_OUTPUT)
    return;

  uint8_t temp_buf[HID_FFB_REPORT_SIZE];
  temp_buf[0] = report_id;
  uint16_t copy_size =
      (bufsize < HID_FFB_REPORT_SIZE - 1) ? bufsize : HID_FFB_REPORT_SIZE - 1;
  memcpy(&temp_buf[1], buffer, copy_size);

  PID_ParseReport(temp_buf, copy_size + 1);

  memcpy(_ffb_data, temp_buf,
         (copy_size + 1 < HID_FFB_REPORT_SIZE) ? copy_size + 1
                                               : HID_FFB_REPORT_SIZE);
  _ffb_updated = true;
}

// ============================================================================
// 公開 API
// ============================================================================

void hidwffb_begin(uint8_t poll_interval_ms) {
  _usb_hid.setPollInterval(poll_interval_ms);
  _usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  _usb_hid.setReportCallback(_hid_get_report_cb, _hid_set_report_cb);
  _usb_hid.begin();
}

bool hidwffb_is_mounted(void) { return TinyUSBDevice.mounted(); }

void hidwffb_wait_for_mount(void) {
  while (!TinyUSBDevice.mounted()) {
    delay(1);
  }
}

bool hidwffb_ready(void) {
  return TinyUSBDevice.mounted() && !TinyUSBDevice.suspended() &&
         _usb_hid.ready();
}

bool hidwffb_send_report(custom_gamepad_report_t *report) {
  if (!hidwffb_ready())
    return false;
  return _usb_hid.sendReport(HID_ID_GAMEPAD_INPUT, report,
                             sizeof(custom_gamepad_report_t));
}

bool hidwffb_sends_pid_state(uint8_t effectBlockIdx, bool playing) {
  if (!hidwffb_ready())
    return false;
  uint8_t status = (1 << 1); // Actuators Enabled
  uint8_t effectState =
      (uint8_t)(playing ? 1 : 0) | (uint8_t)((effectBlockIdx & 0x7F) << 1);
  uint8_t payload[2] = {status, effectState};
  return _usb_hid.sendReport(HID_ID_PID_STATE, payload, sizeof(payload));
}

bool hidwffb_get_ffb_data(uint8_t *buffer) {
  if (!_ffb_updated)
    return false;
  memcpy(buffer, _ffb_data, HID_FFB_REPORT_SIZE);
  _ffb_updated = false;
  return true;
}

void hidwffb_clear_ffb_flag(void) { _ffb_updated = false; }

// ============================================================================
// PID レポートパーサ
// ============================================================================

void PID_ParseReport(uint8_t const *buffer, uint16_t bufsize) {
  if (buffer == NULL || bufsize == 0)
    return;

  uint8_t reportId = buffer[0];
  _pid_debug.lastReportId = reportId;

  switch (reportId) {
  case HID_ID_SET_EFFECT: {
    if (bufsize >= 1) {
      USB_FFB_Report_SetEffect_t *rep = (USB_FFB_Report_SetEffect_t *)buffer;
      uint8_t idx = rep->effectBlockIndex - 1;
      if (idx < MAX_EFFECTS) {
        core0_ffb_effects[idx].type = rep->effectType;
        core0_ffb_effects[idx].gain = rep->gain;
      }
      _pid_debug.isConstantForce = (rep->effectType == HID_ET_CONSTANT);
      _pid_debug.magnitude = rep->gain;
      _pid_debug.updated = true;
    }
    break;
  }
  case HID_ID_SET_CONDITION: {
    if (bufsize >= 1) {
      USB_FFB_Report_SetCondition_t *rep =
          (USB_FFB_Report_SetCondition_t *)buffer;
      uint8_t idx = rep->effectBlockIndex - 1;
      if (idx < MAX_EFFECTS) {
        core0_ffb_effects[idx].cpOffset = rep->cpOffset;
        core0_ffb_effects[idx].positiveCoeff = rep->positiveCoefficient;
      }
      _pid_debug.updated = true;
    }
    break;
  }
  case HID_ID_SET_PERIODIC: {
    if (bufsize >= 1) {
      USB_FFB_Report_SetPeriodic_t *rep =
          (USB_FFB_Report_SetPeriodic_t *)buffer;
      uint8_t idx = rep->effectBlockIndex - 1;
      if (idx < MAX_EFFECTS) {
        core0_ffb_effects[idx].periodicMagnitude = rep->magnitude;
        core0_ffb_effects[idx].periodicOffset = rep->offset;
        core0_ffb_effects[idx].periodicPhase = rep->phase;
        core0_ffb_effects[idx].periodicPeriod = rep->period;
      }
      _pid_debug.updated = true;
    }
    break;
  }
  case HID_ID_SET_CONSTANT_FORCE: {
    if (bufsize >= sizeof(USB_FFB_Report_SetConstantForce_t)) {
      USB_FFB_Report_SetConstantForce_t *rep =
          (USB_FFB_Report_SetConstantForce_t *)buffer;
      uint8_t idx = rep->effectBlockIndex - 1;
      if (idx < MAX_EFFECTS) {
        core0_ffb_effects[idx].magnitude = rep->magnitude;
      }
      _pid_debug.magnitude = rep->magnitude;
      _pid_debug.updated = true;
    }
    break;
  }
  case HID_ID_EFFECT_OPERATION: {
    if (bufsize >= sizeof(USB_FFB_Report_EffectOperation_t)) {
      USB_FFB_Report_EffectOperation_t *rep =
          (USB_FFB_Report_EffectOperation_t *)buffer;
      uint8_t idx = rep->effectBlockIndex - 1;
      if (idx < MAX_EFFECTS) {
        if (rep->operation == HID_OP_START || rep->operation == HID_OP_SOLO)
          core0_ffb_effects[idx].active = true;
        if (rep->operation == HID_OP_STOP)
          core0_ffb_effects[idx].active = false;
      }
      _pid_debug.operation = rep->operation;
      _pid_debug.effectBlockIndex = rep->effectBlockIndex;
      _pid_debug.updated = true;
    }
    break;
  }
  case HID_ID_PID_BLOCK_FREE: {
    if (bufsize >= sizeof(USB_FFB_Report_PIDBlockFree_t)) {
      USB_FFB_Report_PIDBlockFree_t *rep =
          (USB_FFB_Report_PIDBlockFree_t *)buffer;
      uint8_t idx = rep->effectBlockIndex - 1;
      if (idx < MAX_EFFECTS) {
        core0_ffb_effects[idx].active = false;
        core0_ffb_effects[idx].magnitude = 0;
        core0_ffb_effects[idx].type = 0;
      }
    }
    break;
  }
  case HID_ID_DEVICE_CONTROL: {
    if (bufsize >= sizeof(USB_FFB_Report_DeviceControl_t)) {
      USB_FFB_Report_DeviceControl_t *rep =
          (USB_FFB_Report_DeviceControl_t *)buffer;
      if (rep->control == HID_DC_STOP_ALL_EFFECTS ||
          rep->control == HID_DC_DEVICE_RESET) {
        for (int i = 0; i < MAX_EFFECTS; i++) {
          core0_ffb_effects[i].active = false;
          core0_ffb_effects[i].magnitude = 0;
        }
        _next_block_index = 1;
      }
    }
    break;
  }
  case HID_ID_DEVICE_GAIN: {
    if (bufsize >= sizeof(USB_FFB_Report_DeviceGain_t)) {
      USB_FFB_Report_DeviceGain_t *rep = (USB_FFB_Report_DeviceGain_t *)buffer;
      core0_global_gain = rep->deviceGain;
      _pid_debug.deviceGain = core0_global_gain;
      _pid_debug.updated = true;
    }
    break;
  }
  default:
    break;
  }
}

bool hidwffb_get_pid_debug_info(pid_debug_info_t *info) {
  if (!_pid_debug.updated)
    return false;
  if (info != NULL) {
    memcpy(info, &_pid_debug, sizeof(pid_debug_info_t));
  }
  _pid_debug.updated = false;
  return true;
}

// ============================================================================
// Core間共有メモリ
// ============================================================================
FFB_Shared_State_t shared_ffb_effects[MAX_EFFECTS];
volatile uint8_t shared_global_gain = 255;
custom_gamepad_report_t shared_input_report;
mutex_t ffb_shared_mutex;

void ffb_shared_memory_init() { mutex_init(&ffb_shared_mutex); }

void ffb_core0_update_shared(pid_debug_info_t *info) {
  if (mutex_enter_timeout_ms(&ffb_shared_mutex, 1)) {
    for (int i = 0; i < MAX_EFFECTS; i++) {
      shared_ffb_effects[i] = core0_ffb_effects[i];
      if (info != NULL) {
        shared_ffb_effects[i].isCallBackTest = info->updated;
      }
    }
    shared_global_gain = core0_global_gain;
    mutex_exit(&ffb_shared_mutex);
  }
}

void ffb_core1_update_shared(custom_gamepad_report_t *new_input,
                             FFB_Shared_State_t *local_effects_dest) {
  if (mutex_enter_timeout_ms(&ffb_shared_mutex, 1)) {
    shared_input_report = *new_input;
    for (int i = 0; i < MAX_EFFECTS; i++) {
      local_effects_dest[i] = shared_ffb_effects[i];
    }
    mutex_exit(&ffb_shared_mutex);
  }
}

void hidwffb_loopback_test_sync(custom_gamepad_report_t *new_input,
                                FFB_Shared_State_t *local_effects_dest) {
  ffb_core1_update_shared(new_input, local_effects_dest);
#ifdef CALLBACK_TEST_ENABLE
  if (local_effects_dest[0].isCallBackTest) {
    new_input->steer = local_effects_dest[0].magnitude;
    new_input->accel = local_effects_dest[0].gain;
    new_input->brake = (uint16_t)shared_global_gain;
  } else {
    new_input->steer =
        local_effects_dest[0].active ? local_effects_dest[0].magnitude : 0;
    new_input->accel = 0;
    new_input->brake = 0;
  }
  if (mutex_enter_timeout_ms(&ffb_shared_mutex, 1)) {
    shared_input_report = *new_input;
    mutex_exit(&ffb_shared_mutex);
  }
#endif
}

void ffb_core0_get_input_report(custom_gamepad_report_t *dest) {
  if (mutex_enter_timeout_ms(&ffb_shared_mutex, 1)) {
    *dest = shared_input_report;
    mutex_exit(&ffb_shared_mutex);
  }
}
