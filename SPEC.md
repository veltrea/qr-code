# FileMaker QR/バーコード読取プラグイン 仕様書

- 文書バージョン: 1.0
- 作成日: 2026-06-13
- 対象: v1.0(画像デコードのみ)

---

## 1. 概要

FileMaker Pro のコンテナフィールドに格納された画像から、QRコードおよび各種バーコードを読み取り、文字列として返す外部関数プラグイン。

### 1.1 動作環境

| 項目 | 内容 |
|---|---|
| 対応製品 | FileMaker Pro 19 以降(macOS) |
| 対応アーキテクチャ | Apple Silicon (arm64) + Intel (x86_64) ユニバーサルバイナリ |
| 非対応 | FileMaker Go(iOS/iPadOS はプラグイン自体が動作しない) |
| Windows 版 | v2 以降に延期(本仕様書の範囲外) |
| FileMaker Server | 動作可能な設計とするが、v1 では検証対象外 |

### 1.2 v1 スコープ

**含む:**
- コンテナフィールド内の画像(JPEG / PNG / GIF / BMP)からのデコード
- 単一コード読取・複数コード一括読取
- 主要シンボロジー対応(下記 3.3 参照)
- エラー情報の取得関数

**含まない(将来バージョン):**
- バーコード/QR の生成(v2)
- Windows 対応(v2)
- カメラからのライブ読取(対象外)
- 文字 OCR(対象外)

---

## 2. 公開関数仕様

接頭辞は `QRB_` とする。関数 ID は一度決めたら変更禁止(スクリプト互換が壊れるため)。

### 2.1 QRB_Version

| 項目 | 内容 |
|---|---|
| 関数 ID | 100 |
| 引数 | なし |
| 戻り値 | バージョン文字列(例: `"1.0.0"`) |

プラグインの導入確認・バージョン分岐用。

### 2.2 QRB_Decode ( container )

| 項目 | 内容 |
|---|---|
| 関数 ID | 101 |
| 引数 | container: コンテナフィールド(画像) |
| 戻り値 | 最初に検出されたコードのテキスト。検出なしは空文字 `""` |

- 複数コードが存在する場合、ZXing が最初に返した 1 件のみ返す。
- 失敗理由(画像展開失敗・未検出など)は `QRB_LastError` で取得する。

### 2.3 QRB_DecodeAll ( container )

| 項目 | 内容 |
|---|---|
| 関数 ID | 102 |
| 引数 | container: コンテナフィールド(画像) |
| 戻り値 | JSON 配列文字列。検出なしは `[]` |

戻り値の形式:

```json
[
  { "text": "読取結果1", "format": "QRCode" },
  { "text": "読取結果2", "format": "Code128" }
]
```

- `format` は ZXing-C++ の `ToString(BarcodeFormat)` の値をそのまま使う(QRCode, Code128, EAN13, DataMatrix など)。
- JSON 生成は手組みではなくライブラリ(nlohmann/json 推奨)を使い、エスケープ漏れを防ぐ。
- FileMaker 側では `JSONGetElement` でそのままパースできることを受入条件とする。

### 2.4 QRB_LastError

| 項目 | 内容 |
|---|---|
| 関数 ID | 103 |
| 引数 | なし |
| 戻り値 | 直近の関数呼び出しのエラー情報文字列。エラーなしは空文字 |

形式: `"コード: メッセージ"`(例: `"2: 画像のデコードに失敗しました"`)

| コード | 意味 |
|---|---|
| 0 | 正常(空文字を返す) |
| 1 | コンテナが空、または対応外のストリーム型 |
| 2 | 画像データの展開失敗(破損など) |
| 3 | コード未検出 |
| 9 | 内部エラー(例外捕捉) |

- エラー状態はスレッドローカル変数に保持する(FileMaker Server のマルチスレッド評価を考慮)。

---

## 3. 技術仕様

### 3.1 構成ライブラリ

| 役割 | ライブラリ | 入手方法 | リンク方式 |
|---|---|---|---|
| プラグイン枠組み | FileMaker Plug-in SDK | Claris 公式サイト | FMWrapper.framework |
| バーコードデコード | ZXing-C++ (zxing-cpp) | GitHub: zxing-cpp/zxing-cpp | 静的リンク |
| 画像展開 | stb_image.h | GitHub: nothings/stb | ヘッダのみ |
| JSON 生成 | nlohmann/json | GitHub: nlohmann/json | ヘッダのみ |

### 3.2 処理フロー(デコード)

1. `fmx::DataVect` から第 1 引数の `fmx::BinaryData` を取得
2. ストリーム型を優先順で探索: `JPEG` → `PNGf` → `GIFf` → `BMPf` → `FILE`
   - `FILE` の場合は拡張子に依存せず stb_image の自動判別に委ねる
3. 該当ストリームのバイト列を取り出す
4. `stbi_load_from_memory` でグレースケール(1ch)展開
5. `ZXing::ImageView`(`ImageFormat::Lum`)に包む
6. `ZXing::ReadBarcodes(view, options)` を呼ぶ
   - `options.setTryHarder(true)`
   - `options.setTryRotate(true)`
   - `options.setTryInvert(true)`
7. 結果を fmx::Data(テキスト)として返す。文字コードは UTF-8 で `fmx::Text::SetText` する

### 3.3 対応シンボロジー(v1)

ZXing-C++ のデフォルト全形式を有効にする。主要なもの:

- 2次元: QRCode, MicroQRCode, DataMatrix, Aztec, PDF417
- 1次元: Code128, Code39, Code93, EAN-13, EAN-8, UPC-A, UPC-E, ITF, Codabar

形式の絞り込み引数は v1 では設けない(必要になれば v1.1 でオプション引数追加)。

### 3.4 プラグイン識別情報

| 項目 | 値 | 備考 |
|---|---|---|
| プラグイン ID(クリエータコード) | `QRBc` | 4 文字。**確定後は変更禁止** |
| バンドル名 | `QRBarcodeReader.fmplugin` | |
| Bundle Identifier | `com.veltrea.qrbarcodereader` | 必要に応じて変更可 |
| 関数接頭辞 | `QRB_` | |

### 3.5 文字コード

- FileMaker とのやり取りは UTF-8 / UTF-16 変換を fmx::Text の API に任せる。独自変換を実装しない。
- QR の中身が Shift_JIS でエンコードされている場合、ZXing が文字コード推定を行う。推定失敗時はそのままのバイト解釈で返し、既知の制限として README に記載する。

---

## 4. エラー処理方針

- 外部関数のエントリポイントは全体を `try/catch` で囲み、例外を FileMaker プロセスへ漏らさない(漏れると FileMaker ごと落ちる)。
- 例外捕捉時はエラーコード 9 を設定し、戻り値は空文字または `[]`。
- `QRB_Decode` / `QRB_DecodeAll` の呼び出し開始時にエラー状態をクリアする。

---

## 5. 受入条件(テスト項目)

1. `QRB_Version` が `"1.0.0"` を返す
2. QR 1 個の PNG 画像で `QRB_Decode` が正しい文字列を返す
3. 日本語(UTF-8)を含む QR が文字化けせず返る
4. QR と Code128 が混在する画像で `QRB_DecodeAll` が 2 要素の JSON を返し、`JSONGetElement` でパースできる
5. 15 度程度傾いた QR 画像が読める(TryRotate の確認)
6. 白黒反転 QR が読める(TryInvert の確認)
7. コードなし画像で `QRB_Decode` が `""`、`QRB_LastError` が `"3: ..."` を返す
8. 空のコンテナで `QRB_LastError` が `"1: ..."` を返す
9. Apple Silicon ネイティブ動作と Rosetta 動作(x86_64)の両方でロード・動作する
10. 改行を含む QR の内容が改行込みで正しく返る

---

## 6. ロードマップ

| バージョン | 内容 |
|---|---|
| v1.0 | 本仕様(macOS・デコードのみ) |
| v1.1 | シンボロジー絞り込みオプション、検出座標の返却 |
| v2.0 | 生成機能 `QRB_Generate( text ; type ; size )` → PNG をコンテナ返却、Windows 対応 |
