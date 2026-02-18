***

# LK-TECH CAN Protocol Command Reference (MotorDriveProtocol.md)
SHANGHAI LINGKONG TECHNOLOGY CO.,LTD CAN PROTOCOL V2.35 に基づき、プロジェクトで使用しているコマンドの仕様をMarkdown形式でまとめたもの。

## 目次
- [共通仕様](#共通仕様)
- [1. CMD_MOTOR_OFF (0x80)](#1-cmd_motor_off-0x80)
- [2. CMD_MOTOR_ON (0x88)](#2-cmd_motor_on-0x88)
- [3. CMD_MOTOR_STOP (0x81)](#3-cmd_motor_stop-0x81)
- [4. CMD_TORQUE_CTRL (0xA1)](#4-cmd_torque_ctrl-0xa1)
- [5. CMD_READ_ENC (0x90)](#5-cmd_read_enc-0x90)
- [6. CMD_READ_STAT1 (0x9A)](#6-cmd_read_stat1-0x9a)
- [7. CMD_CLEAR_ERR (0x9B)](#7-cmd_clear_err-0x9b)

## 共通仕様（Single motor command）
*   **CAN ID**: `0x140 + ID(1~32)`
*   **Frame Format**: Standard Frame, Data Frame
*   **DLC**: 8 Bytes
*   **Endian**: Little Endian (下位バイトが先)

---

## 1. CMD_MOTOR_OFF (0x80)
**概要**:
モーターをON状態からOFF状態へ切り替えます。モーターの回転数と以前のコマンドをクリアします。LEDは「常時点灯」から「ゆっくり点滅」に変わります。モーターはコマンドに応答しますが、動作は実行しません。

### 送信パケット (Host -> Driver)
| Byte | Name | Value / Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | Command | `0x80` | Command Byte |
| 1 | NULL | `0x00` | - |
| 2 | NULL | `0x00` | - |
| 3 | NULL | `0x00` | - |
| 4 | NULL | `0x00` | - |
| 5 | NULL | `0x00` | - |
| 6 | NULL | `0x00` | - |
| 7 | NULL | `0x00` | - |

### 受信パケット (Driver -> Host)
ホスト送信コマンドと同一の内容が返信されます。

---

## 2. CMD_MOTOR_ON (0x88)
**概要**:
モーターをOFF状態からON状態へ切り替えます。LEDは「ゆっくり点滅」から「常時点灯」に変わります。このコマンド送信後、モーター制御が可能になります。

### 送信パケット (Host -> Driver)
| Byte | Name | Value / Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | Command | `0x88` | Command Byte |
| 1 | NULL | `0x00` | - |
| 2 | NULL | `0x00` | - |
| 3 | NULL | `0x00` | - |
| 4 | NULL | `0x00` | - |
| 5 | NULL | `0x00` | - |
| 6 | NULL | `0x00` | - |
| 7 | NULL | `0x00` | - |

### 受信パケット (Driver -> Host)
ホスト送信コマンドと同一の内容が返信されます。

---

## 3. CMD_MOTOR_STOP (0x81)
**概要**:
モーターを停止させますが、モーターの状態（ステート）はクリアしません。再度コマンドを送信することで制御可能です。

### 送信パケット (Host -> Driver)
| Byte | Name | Value / Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | Command | `0x81` | Command Byte |
| 1 | NULL | `0x00` | - |
| 2 | NULL | `0x00` | - |
| 3 | NULL | `0x00` | - |
| 4 | NULL | `0x00` | - |
| 5 | NULL | `0x00` | - |
| 6 | NULL | `0x00` | - |
| 7 | NULL | `0x00` | - |

### 受信パケット (Driver -> Host)
ホスト送信コマンドと同一の内容が返信されます。

---

## 4. CMD_TORQUE_CTRL (0xA1)
**概要**:
トルク電流（Torque Current）の閉ループ制御を行います。
`iqControl` は `int16_t` で、範囲は -2048 ~ 2048 です。実際の電流値への換算はモーターモデルに依存します（例: MFシリーズは -16.5A~16.5A、MGシリーズは -33A~33A）。

### 送信パケット (Host -> Driver)
| Byte | Name | Value / Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | Command | `0xA1` | Command Byte |
| 1 | NULL | `0x00` | - |
| 2 | NULL | `0x00` | - |
| 3 | NULL | `0x00` | - |
| 4 | iqControl_L | `int16_t` (Low) | トルク電流指令値 (下位) |
| 5 | iqControl_H | `int16_t` (High) | トルク電流指令値 (上位) |
| 6 | NULL | `0x00` | - |
| 7 | NULL | `0x00` | - |

### 受信パケット (Driver -> Host)
| Byte | Name | Value / Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | Command | `0xA1` | Command Byte |
| 1 | Temperature | `int8_t` | モーター温度 (1℃/LSB) |
| 2 | iq_L | `int16_t` (Low) | 現在のトルク電流値 (下位) |
| 3 | iq_H | `int16_t` (High) | 現在のトルク電流値 (上位) |
| 4 | Speed_L | `int16_t` (Low) | モーター速度 (1dps/LSB) |
| 5 | Speed_H | `int16_t` (High) | モーター速度 (上位) |
| 6 | Encoder_L | `uint16_t` (Low) | エンコーダ現在位置 (下位) |
| 7 | Encoder_H | `uint16_t` (High) | エンコーダ現在位置 (上位) |

---

## 5. CMD_READ_ENC (0x90)
**概要**:
現在のエンコーダ位置情報を読み取ります。補正後のエンコーダ値、生のエンコーダ値、オフセット値が含まれます。

### 送信パケット (Host -> Driver)
| Byte | Name | Value / Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | Command | `0x90` | Command Byte |
| 1 | NULL | `0x00` | - |
| 2 | NULL | `0x00` | - |
| 3 | NULL | `0x00` | - |
| 4 | NULL | `0x00` | - |
| 5 | NULL | `0x00` | - |
| 6 | NULL | `0x00` | - |
| 7 | NULL | `0x00` | - |

### 受信パケット (Driver -> Host)
| Byte | Name | Value / Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | Command | `0x90` | Command Byte |
| 1 | NULL | `0x00` | - |
| 2 | Encoder_L | `uint16_t` (Low) | エンコーダ値 (Raw - Offset) |
| 3 | Encoder_H | `uint16_t` (High) | エンコーダ値 (上位) |
| 4 | Raw_L | `uint16_t` (Low) | 生のエンコーダ値 (0~16383) |
| 5 | Raw_H | `uint16_t` (High) | 生のエンコーダ値 (上位) |
| 6 | Offset_L | `uint16_t` (Low) | エンコーダオフセット値 (ゼロ点) |
| 7 | Offset_H | `uint16_t` (High) | エンコーダオフセット値 (上位) |

---

## 6. CMD_READ_STAT1 (0x9A)
**概要**:
モーターの状態1（温度、電圧、エラーステート）を読み取ります。

### 送信パケット (Host -> Driver)
| Byte | Name | Value / Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | Command | `0x9A` | Command Byte |
| 1 | NULL | `0x00` | - |
| 2 | NULL | `0x00` | - |
| 3 | NULL | `0x00` | - |
| 4 | NULL | `0x00` | - |
| 5 | NULL | `0x00` | - |
| 6 | NULL | `0x00` | - |
| 7 | NULL | `0x00` | - |

### 受信パケット (Driver -> Host)
| Byte | Name | Value / Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | Command | `0x9A` | Command Byte |
| 1 | Temperature | `int8_t` | モーター温度 (1℃/LSB) |
| 2 | NULL | `0x00` | - |
| 3 | Voltage_L | `uint16_t` (Low) | 電圧 (0.1V/LSB) |
| 4 | Voltage_H | `uint16_t` (High) | 電圧 (上位) |
| 5 | NULL | `0x00` | - |
| 6 | NULL | `0x00` | - |
| 7 | ErrorState | `uint8_t` | エラーステート (Bit 0: Voltage, Bit 3: Temp) |

---

## 7. CMD_CLEAR_ERR (0x9B)
**概要**:
現在のアラーム（エラーステート）をクリアします。ただし、モーターの状態が正常に戻っていない場合、エラーはクリアされません。
※注意: 受信パケットのコマンドバイトは `0x9B` ではなく `0x9A` (Read State 1と同じ形式) が返されます。

### 送信パケット (Host -> Driver)
| Byte | Name | Value / Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | Command | `0x9B` | Command Byte |
| 1 | NULL | `0x00` | - |
| 2 | NULL | `0x00` | - |
| 3 | NULL | `0x00` | - |
| 4 | NULL | `0x00` | - |
| 5 | NULL | `0x00` | - |
| 6 | NULL | `0x00` | - |
| 7 | NULL | `0x00` | - |

### 受信パケット (Driver -> Host)
| Byte | Name | Value / Type | Description |
| :--- | :--- | :--- | :--- |
| 0 | Command | `0x9A` | **注意: レスポンスは 0x9A** |
| 1 | Temperature | `int8_t` | モーター温度 (1℃/LSB) |
| 2 | NULL | `0x00` | - |
| 3 | Voltage_L | `uint16_t` (Low) | 電圧 (0.1V/LSB) |
| 4 | Voltage_H | `uint16_t` (High) | 電圧 (上位) |
| 5 | NULL | `0x00` | - |
| 6 | NULL | `0x00` | - |
| 7 | ErrorState | `uint8_t` | エラーステート |