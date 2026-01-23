#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <Arduino.h>

/**
 * @brief Core 0 と Core 1 で共有するデータの構造体
 * 
 * Core 1 (制御コア) がデータを書き込み、Core 0 (通信コア) が読み取ってUSB経由で送信することを想定。
 */
struct SharedData {
    // 入力データ (Core 1 -> Core 0)
    volatile int16_t steeringAngle;   // ステアリング角度 (-32768 to 32767)
    volatile uint16_t accelerator;    // アクセル値
    volatile uint16_t brake;          // ブレーキ値
    volatile uint32_t buttons;        // ボタンビットマスク (bit0: Up, bit1: Down)

    // FFBデータ (Core 0 -> Core 1) - 将来の拡張用
    volatile int16_t targetTorque;    // ホストから受信した目標トルク
    volatile uint8_t ffbEffectStatus; // FFBエフェクトの状態
    
    // システムステータス
    volatile uint32_t core1LoopCount; // Core 1 のループカウンタ (動作確認用)
    volatile uint32_t lastCore1Micros; // Core 1 の最終実行時刻 (us)
};

// グローバル共有インスタンスの extern 宣言
extern SharedData sharedData;

#endif // SHARED_DATA_H
