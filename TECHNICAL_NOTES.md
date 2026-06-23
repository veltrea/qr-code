# 技術資料: FileMaker Plug-in SDK 実装ノート

SPEC.md の補足資料。FileMaker プラグイン特有の落とし穴と、実装の具体的な型・API を整理する。実装時はこの資料を SDK 同梱のサンプルコードと突き合わせて確認すること。

---

## 1. SDK の入手と構成

- 入手先: Claris 公式サイト「FileMaker Plug-in SDK」(無償、Claris ID 必要な場合あり)
- 注意: SDK のバージョンにより API 名・サンプル構成が変わっていることがある。**この資料の API 名は目安であり、必ず手元の SDK ヘッダ(`FMWrapper/` 配下)を正とすること。**

SDK に含まれる主要物:

| 内容 | 用途 |
|---|---|
| `FMWrapper.framework`(macOS) | リンク対象。fmx:: クラス群の実体 |
| ヘッダ群(`FMXExtern.h`, `FMXTypes.h`, `FMXData.h`, `FMXBinaryData.h`, `FMXCalcEngine.h` など) | 外部関数 API |
| サンプルプロジェクト(Xcode) | **これを複製して土台にするのが最も安全** |

サンプルをゼロから書き直さず、サンプルの Xcode プロジェクトをコピーして改名・差し替えする方針を取る。ビルド設定(バンドル拡張子 `.fmplugin`、Info.plist の構成、フレームワークの組み込み)を自前で再現するのは事故の元。

## 2. プラグインのライフサイクル

エントリポイント `FMExternCallProc` に FileMaker から各種メッセージが届く。最低限処理するもの:

| メッセージ | 処理内容 |
|---|---|
| `kFMXT_GetString` | プラグイン名・オプション文字列を返す。**プラグイン ID(QRBc)はここで返すオプション文字列に埋め込む**。書式は SDK サンプルに従う |
| `kFMXT_Init` | `RegisterExternalFunction` で全関数を登録。成功なら `kCurrentExtnVersion` を返す |
| `kFMXT_Shutdown` | `UnRegisterExternalFunction` で登録解除 |
| `kFMXT_Idle` | v1 では何もしない |

### 関数登録

`fmx::ExprEnv::RegisterExternalFunctionEx`(SDK バージョンによっては `RegisterExternalFunction`)を使う。渡すもの:

- プラグイン ID(QuadChar `QRBc`)
- 関数 ID(100〜103。**SPEC.md 3.4 の採番を厳守**)
- 関数名(`QRB_Decode` など)
- 定義文字列(`QRB_Decode( container )` — FileMaker の計算式エディタに表示される)
- 最小・最大引数数
- 型フラグ(`kMayEvaluateOnServer | kDisplayInAllDialogs` 相当)
- コールバック関数ポインタ

コールバックのシグネチャ(目安):

```cpp
static fmx::errcode Do_QRB_Decode(
    short funcId,
    const fmx::ExprEnv& env,
    const fmx::DataVect& parms,
    fmx::Data& result);
```

## 3. コンテナフィールド(BinaryData)の扱い

ここが本プラグインの核心であり、最大の罠でもある。

### 3.1 ストリーム構造

FileMaker のコンテナは「複数の型付きストリームの束」。画像なら例えば `JPEG` 本体 + `FNAM`(ファイル名)+ プレビュー用ストリームが同居する。

```cpp
const fmx::BinaryData& bin = parms.AtAsBinaryData(0);

// 型を指定してインデックスを探す
fmx::QuadCharUniquePtr typeJPEG('J','P','E','G');
fmx::int32 idx = bin.GetIndex(*typeJPEG);
if (idx >= 0) {
    fmx::uint32 size = bin.GetSize(idx);
    std::vector<unsigned char> buf(size);
    bin.GetData(idx, 0, size, buf.data());
}
```

探索の優先順: `JPEG` → `PNGf` → `GIFf` → `BMPf` → `FILE`。

### 3.2 罠

- **「ファイルとして挿入」されたコンテナ**は `FILE` ストリームになる。拡張子を信用せず stb_image の自動判別に渡す。
- **PDF や HEIC が入っている可能性**がある。stb_image は HEIC/PDF を扱えないので、展開失敗 → エラーコード 2 で返す(v1 の既知制限として README に記載)。
- **外部保存(リモートコンテナ)**のフィールドでも、計算式経由で渡る時点で BinaryData として解決されるのが通常だが、空で届くケースがあればエラーコード 1 で返す。
- `parms.At(0)` の Data が BinaryData を持たない(テキストが渡された等)場合のガードを必ず入れる。`fmx::Data::GetNativeType` で確認できる。

## 4. ZXing-C++ の組み込み

### 4.1 取得とビルド

```bash
git clone https://github.com/zxing-cpp/zxing-cpp.git
cmake -S zxing-cpp -B zxing-build \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_BUILD_TYPE=Release \
  -DZXING_READERS=ON -DZXING_WRITERS=OFF \
  -DBUILD_SHARED_LIBS=OFF \
  -DZXING_EXAMPLES=OFF
cmake --build zxing-build -j
```

- `BUILD_SHARED_LIBS=OFF` で静的ライブラリ(`libZXing.a`)を作り、プラグインに焼き込む。dylib 配布は依存解決が面倒になるので避ける。
- `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` を**忘れない**。片アーキだとリンク時か実行時に落ちる。
- C++17 以降が必要。プラグイン本体側のビルド設定も C++17 以上に合わせる。
- オプション名は zxing-cpp のバージョンで変わることがある(旧: `BUILD_READERS` / 新: `ZXING_READERS` など)。cmake が未知オプションを警告したら `cmake -LH zxing-build` で実名を確認する。

### 4.2 デコード呼び出し

```cpp
#include "ReadBarcode.h"

int w, h, ch;
unsigned char* pixels = stbi_load_from_memory(buf.data(), (int)buf.size(), &w, &h, &ch, 1); // 1 = grayscale
if (!pixels) { /* エラーコード 2 */ }

ZXing::ImageView view(pixels, w, h, ZXing::ImageFormat::Lum);

ZXing::ReaderOptions opts;   // 旧バージョンでは DecodeHints という名前
opts.setTryHarder(true);
opts.setTryRotate(true);
opts.setTryInvert(true);

auto results = ZXing::ReadBarcodes(view, opts);

for (const auto& r : results) {
    std::string text = r.text();                       // UTF-8
    std::string format = ZXing::ToString(r.format());  // "QRCode" 等
}
stbi_image_free(pixels);
```

## 5. FileMaker への文字列返却

```cpp
fmx::TextUniquePtr outText;
outText->Assign(utf8String.c_str(), fmx::Text::kEncoding_UTF8);
result.SetAsText(*outText, parms.At(0).GetLocale());
```

- `Assign` のエンコーディング指定を UTF-8 にすること。デフォルトに任せると日本語が化ける。
- ロケールは引数のものを引き継ぐのが無難。引数なし関数(`QRB_Version` 等)では `fmx::LocaleUniquePtr` のデフォルトで良い。

## 6. ビルド設定の要点(Xcode)

| 設定 | 値 |
|---|---|
| Wrapper Extension | `fmplugin` |
| Architectures | `arm64 x86_64`(Build Active Architecture Only = No) |
| C++ Language Dialect | C++17 以上 |
| リンク | `FMWrapper.framework` + `libZXing.a` |
| 署名 | ad-hoc 署名(`codesign --force --deep -s -`)で可。配布時に Gatekeeper 警告が出る点は既知 |

インストール先(動作確認時):

```
~/Library/Application Support/FileMaker/Extensions/
```

または FileMaker Pro の `Extensions` フォルダ。インストール後 FileMaker Pro を再起動し、環境設定 > プラグイン で有効化を確認する。

## 7. クラッシュ防止の鉄則

1. 外部関数コールバックの先頭から末尾まで `try { ... } catch (...) { }` で囲む。例外が FileMaker 本体へ漏れると**アプリごと落ちる**
2. `stbi_load_from_memory` の戻り値 NULL チェックを必ず行う
3. 巨大画像(例: 1 億画素超)はデコード前に `stbi_info_from_memory` で寸法を確認し、上限(例: 50MP)を超えたらエラーコード 2 で弾く
4. スレッドローカルでエラー状態を持つ(`thread_local std::string g_lastError;`)。FileMaker Server では複数スレッドから同時評価され得る

## 8. 参考資料

- Claris 公式: FileMaker Plug-in SDK ドキュメント(SDK 同梱の PDF / ヘッダコメント)
- zxing-cpp: https://github.com/zxing-cpp/zxing-cpp (README と `example/ZXingReader.cpp` が最短の手本)
- stb_image: https://github.com/nothings/stb
- nlohmann/json: https://github.com/nlohmann/json
