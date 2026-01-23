# RP2040 移行およびマルチコア実装計画 (2026-01-22 23:17)

この計画では、Arduino Pro Microベースの既存プロジェクトをRP2040（Raspberry Pi Pico等）へ移行し、マルチコア機能を活用した高精度な制御基盤を構築します。

## 構成概要
- **Core 0**: USB HID PID (Physical Interface Device) 通信の制御。Adafruit_TinyUSB を使用。
- **Core 1**: 1ms周期の厳密な制御ループ。センサ取得、フィルタリング、将来的なトルク制御を担当。
- **共有データ**: `shared_data.h` に定義された構造体を介してコア間でデータを授受。

## 変更内容

### 共通
#### [NEW] [shared_data.h](file:///d:/PlatformIO_Project/Ene1_HandCont_rp2040_FFB/include/shared_data.h)
- Core 0 と Core 1 で共有する情報の構造体 `SharedData` を定義します。
- ステアリング角度、アクセル、ブレーキ、ボタン入力、およびFFB関連のプレースホルダを含めます。

### メインロジック
#### [MODIFY] [main.cpp](file:///d:/PlatformIO_Project/Ene1_HandCont_rp2040_FFB/src/main.cpp)
- **Core 0 (`setup()` / `loop()`)**:
    - `Adafruit_TinyUSB` の初期化。
    - HID PID（フォースフィードバック対応）のレポートデスクリプタを最小構成で実装。
    - USBタスクの実行とホストへのデータ送信。
- **Core 1 (`setup1()` / `loop1()`)**:
    - `micros()` を用いた1ms周期（1000us）の厳密なタイミング制御。
    - センサ読み取り等のスケルトンを配置。

## 検証計画

### 1. ビルド検証
- PlatformIO にて、ターゲット環境 `env:pico` でエラーなくコンパイルできることを確認。
    - `pio run`

### 2. マルチコア動作検証
- 各コアが期待通りに起動していることをシリアル出力で確認。
- Core 1 の周期（1ms）が維持されているか、`micros()` の差分を確認。

### 3. USB認識検証
- Windows端末に接続した際、デバイスマネージャーまたは「ゲームコントローラの設定」にて、PID（フォースフィードバック）機能を有するコントローラとして認識されることを確認。
