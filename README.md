# QRBarcodeReader

FileMaker Pro (macOS / Windows) 用のQRコード・バーコード読取外部関数プラグインです。
コンテナフィールド内の画像から高精度・高速にQRコードおよび各種バーコードを検出し、テキストとして返します。

## 主な機能

- **単一デコード (`QRB_Decode`)**: 画像から最初に検出したコードの内容を返します。
- **一括デコード (`QRB_DecodeAll`)**: 画像に含まれるすべてのコードを検出し、JSON配列形式で返します。
- **エラー取得 (`QRB_LastError`)**: 直前の処理のエラー（空コンテナ、画像デコード失敗、未検出など）を詳細に取得できます。
- **マルチスレッド対応**: スレッドローカルでエラー情報を保持し、FileMaker Serverでの並列実行も考慮した設計。
- **クロスプラットフォーム**: macOS (Apple Silicon / Intel ユニバーサルバイナリ) および Windows (64bit) の両対応。

---

## 動作環境

| 項目 | 対応仕様 |
|---|---|
| 対応OS | macOS 11 以降, Windows 10 / 11 (64bit) |
| 対応製品 | Claris FileMaker Pro 19 以降 |
| アーキテクチャ | Mac (arm64 / x86_64) , Windows (x64) |

---

## 導入方法

### macOS の場合

1. ビルド済みの `QRBarcodeReader.fmplugin` を以下のフォルダにコピーします。
   ```bash
   mkdir -p ~/Library/"Application Support"/FileMaker/Extensions
   cp -R QRBarcodeReader.fmplugin ~/Library/"Application Support"/FileMaker/Extensions/
   ```
2. FileMaker Pro を再起動します。
3. **環境設定 > プラグイン** で `QRBarcodeReader` にチェックが入っていることを確認します。

### Windows の場合

1. ビルド済みの `QRBarcodeReader.fmx64` を以下のフォルダにコピーします。
   - ユーザー個別インストールの場合:
     `%LOCALAPPDATA%\FileMaker\Extensions\`
     （例: `C:\Users\<ユーザー名>\AppData\Local\FileMaker\Extensions\`）
   - アプリケーション全体に適用する場合:
     `C:\Program Files\Claris\FileMaker Pro 19\Extensions\` (または `FileMaker Pro 20\Extensions\` など)
2. FileMaker Pro を再起動します。
3. **環境設定 > プラグイン** で `QRBarcodeReader` にチェックが入っていることを確認します。

---

## 関数リファレンス

### 1. `QRB_Version`
プラグインのバージョン情報を返します。
- **引数**: なし
- **戻り値**: `"1.0.0"` (文字列)

### 2. `QRB_Decode ( container )`
コンテナ内の画像から最初に検出されたコードの値を返します。
- **引数**: `container` (オブジェクトフィールド)
- **戻り値**: 検出されたコードの文字列。検出されなかった場合は空文字 `""` を返します。詳細なエラーは `QRB_LastError` で取得します。

### 3. `QRB_DecodeAll ( container )`
コンテナ内の画像から検出されたすべてのコードの情報をJSON配列で返します。
- **引数**: `container` (オブジェクトフィールド)
- **戻り値**: JSON配列形式の文字列。検出されなかった場合は空のJSON配列 `[]` を返します。
  ```json
  [
    { "text": "Hello FileMaker", "format": "QRCode" },
    { "text": "1234567890128", "format": "EAN13" }
  ]
  ```

### 4. `QRB_LastError`
直前に呼び出された `QRB_Decode` または `QRB_DecodeAll` のエラー情報を取得します。
- **引数**: なし
- **戻り値**: `コード: メッセージ` 形式の文字列。エラーがない場合は空文字 `""`。
  
  | コード | 内容 |
  |---|---|
  | 0 | 正常 |
  | 1 | コンテナが空、または画像以外のフォーマット |
  | 2 | 画像データの展開に失敗（破損、または未対応形式） |
  | 3 | コードが検出されなかった |
  | 9 | 内部エラー（例外発生） |

---

## 開発とビルド手順

### 1. 前提条件
- **Xcode / Command Line Tools**
- **FileMaker Plug-in SDK** (本リポジトリと同一ディレクトリ配下に `PlugInSDK/` が配置されている前提)
- 外部依存関係: `zxing-cpp`, `stb`, `nlohmann-json` (リポジトリの `third_party/` に配置)

### 2. サードパーティライブラリの準備
`third_party/` ディレクトリで `zxing-cpp` をクローンし、ユニバーサル静的ライブラリとしてビルドします。

```bash
cd third_party
git clone https://github.com/zxing-cpp/zxing-cpp.git
cmake -S zxing-cpp -B zxing-build \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_BUILD_TYPE=Release \
  -DZXING_READERS=ON -DZXING_WRITERS=OFF \
  -DBUILD_SHARED_LIBS=OFF \
  -DZXING_EXAMPLES=OFF
cmake --build zxing-build -j
```

※ `stb_image.h` および `nlohmann/json` はヘッダオンリーライブラリとして `third_party/` 内に含まれているか、参照可能な位置に配置します。

### 3. プラグインのビルド
Xcode プロジェクトからビルドを実行します。

```bash
cd plugin
xcodebuild -project QRBarcodeReader.xcodeproj -target QRBarcodeReader -configuration Release build
```

ビルド完了後、`plugin/build/Release/QRBarcodeReader.fmplugin` が生成されます。

### 4. バイナリの検証
ユニバーサルバイナリになっていること、署名が正しいことを確認します。

```bash
cd plugin/build/Release
# アーキテクチャ確認
lipo -info QRBarcodeReader.fmplugin/Contents/MacOS/QRBarcodeReader
# 署名の確認
codesign --verify QRBarcodeReader.fmplugin && echo "codesign: OK"
```

---

## ディレクトリ構成

```
.
├── CLAUDE.md            # 開発ローカルルール・進め方
├── SPEC.md              # 詳細な機能仕様書
├── TECHNICAL_NOTES.md   # 実装上の注意・SDKの罠
├── README.md            # 本文書 (作成されたファイル)
├── plugin/              # Xcode プロジェクト・プラグインソースコード
│   └── QRBarcodeReader/
├── third_party/         # zxing-cpp 等の外部ライブラリ
├── test-images/         # テスト用画像および画像生成スクリプト
└── docs/                # 詳細ドキュメント
    ├── ENVIRONMENT.md   # 開発環境・ビルドコマンドの記録
    ├── SDK_DIFF.md      # SDKの公式ヘッダと資料の相違点記録
    └── TEST_GUIDE.md    # ユーザー向けの動作テスト手順
```

---

## ライセンス・クレジット

- デコードエンジン: [ZXing-C++](https://github.com/zxing-cpp/zxing-cpp) (Apache License 2.0)
- 画像デコード: [stb_image](https://github.com/nothings/stb) (Public Domain / MIT)
- JSONパーサ: [nlohmann/json](https://github.com/nlohmann/json) (MIT)
