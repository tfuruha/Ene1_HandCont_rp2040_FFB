# 基盤設計ファイル作成 実装計画

## 概要

Ene1_HandCont_rp2040_FFBプロジェクトの基盤設計を行い、以下の3つのヘッダーファイルを新規作成します：

1. **include/config.h**: ピンアサインと定数の集約
2. **include/shared_data.h**: Core 0とCore 1の共有データ構造体（既存ファイルの更新）
3. **include/CANInterface.h**: CAN通信の抽象インターフェース

これにより、ハードコードされた値を排除し、モジュール間の依存関係を明確化します。

## ユーザーレビュー必要項目

> [!IMPORTANT]
> **ピンアサインの確認**
> 
> SpecSummary20260118.txtに記載されているピンアサインと、既存コード（Ene1HandCont_IO.cpp、mf4015.cpp）に記載されているピン番号に以下の差異があります：
> 
> | 信号名 | SpecSummary20260118.txt | 既存コード |
> |--------|------------------------|-----------|
> | S-Up Button | Pin 25 | Pin 6 |
> | S-Down Button | Pin 24 | Pin 7 |
> | CAN_CS | Pin 17 | Pin 10 |
> 
> **仕様書を正とする**という指示に従い、config.hには**SpecSummary20260118.txtの値（Pin 25, 24, 17）を採用**します。

## 提案する変更内容

### コンフィギュレーション

#### [NEW] [config.h](file:///d:/PlatformIO_Project/Ene1_HandCont_rp2040_FFB/include/config.h)

SpecSummary20260118.txtに基づき、すべてのピンアサインと定数を集約した設定ファイルを作成します。

**主な定義内容:**
- **SPIピン**: CS(17), SCK(18), TX(19), RX(16)
- **デジタル入力ピン**: S-Up(25), S-Down(24)
- **アナログ入力ピン**: Brake(ADC0/26), Accel(ADC1/27)
- **CAN通信定数**: ボーレート(500kbps), クロック(8MHz), MF4015 ID(0x141)
- **タイミング定数**: サンプリング周期(2ms), メイン周期(20ms)
- **ADC範囲**: アクセル(160-840), ブレーキ(100-820)
- **チャタリング防止**: ButtonTh(4)
- **移動平均**: バッファサイズ(20), サンプル数(8)

---

### 共有データ構造

#### [MODIFY] [shared_data.h](file:///d:/PlatformIO_Project/Ene1_HandCont_rp2040_FFB/include/shared_data.h)

既存のSharedData構造体を維持しつつ、コメントを充実化します。

**変更内容:**
- 既存の構造体定義は維持（互換性確保）
- コメントを日本語化・詳細化
- 将来の拡張に備えたフィールド（デバイス設定値など）の追加は今回は見送り

---

### CAN通信抽象化

#### [NEW] [CANInterface.h](file:///d:/PlatformIO_Project/Ene1_HandCont_rp2040_FFB/include/CANInterface.h)

CAN通信を抽象化するための純粋仮想クラス（インターフェース）を定義します。

**主な機能:**
- `virtual bool begin() = 0`: CAN初期化
- `virtual bool sendFrame(uint32_t id, uint8_t len, const uint8_t* data) = 0`: フレーム送信
- `virtual bool readFrame(uint32_t& id, uint8_t& len, uint8_t* data) = 0`: フレーム受信（将来の拡張用）

**設計意図:**
- MCP2515以外のCANコントローラへの移植を容易にする
- テスト時のモック実装を可能にする
- 依存性注入パターンの適用

## 検証計画

### コンパイル検証

以下のコマンドでプロジェクトがエラーなくビルドできることを確認します：

```bash
cd d:\PlatformIO_Project\Ene1_HandCont_rp2040_FFB
pio run
```

### 手動検証

1. **config.hの確認**: すべての定数がSpecSummary20260118.txtと一致していることを目視確認
2. **shared_data.hの確認**: 既存の構造体定義が変更されていないことを確認
3. **CANInterface.hの確認**: 純粋仮想関数が正しく定義されていることを確認

### 今後の統合作業

> [!NOTE]
> 本実装計画では**ファイルの作成のみ**を行います。既存のコード（Ene1HandCont_IO.cpp、mf4015.cpp等）を新しいconfig.hを使用するように修正する作業は、別途実施する必要があります。
