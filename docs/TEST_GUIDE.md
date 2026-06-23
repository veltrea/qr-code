# FileMaker での動作確認手順(ユーザー向け)

## フェーズ 1: 雛形の確認(QRB_Version)

### 手順 1. プラグインをインストールする

ターミナルを開いて、次の 2 行をそのままコピーして貼り付け、Enter を押してください。

```bash
mkdir -p ~/Library/"Application Support"/FileMaker/Extensions
cp -R /Volumes/DISK/dev/filemaker-plugin/qr-code/plugin/build/Release/QRBarcodeReader.fmplugin ~/Library/"Application Support"/FileMaker/Extensions/
```

**確認方法:** 続けて次の 1 行を実行し、`QRBarcodeReader.fmplugin` と表示されれば成功です。

```bash
ls ~/Library/"Application Support"/FileMaker/Extensions/
```

### 手順 2. FileMaker Pro を再起動する

FileMaker Pro が起動中なら、完全に終了(⌘Q)してから起動し直してください。

### 手順 3. プラグインが有効か確認する

1. FileMaker Pro のメニューから「FileMaker Pro」>「環境設定...」(または「設定...」)を開く
2. 「プラグイン」タブをクリック
3. 一覧に **QRBarcodeReader** が表示され、チェックが入っていることを確認
   - 「未署名のプラグインを許可しますか」という確認が出た場合は「許可」を選んでください(ad-hoc 署名のため一度だけ出ます)

**表示されない場合:** 手順 1 のフォルダ位置と FileMaker の再起動をもう一度確認してください。

### 手順 4. QRB_Version を呼ぶ

1. 適当なデータベース(新規作成で可)を開く
2. メニュー「ツール」>「データビューア」を開く(データビューアがないエディションの場合は、計算フィールドを 1 つ作る方法でも可)
3. 「監視」タブ >「+」で新しい式を追加し、次を入力:

```
QRB_Version
```

4. 結果が **1.0.0** と表示されれば合格です

### 結果の報告

- 手順 3 で一覧に出たか / チェックが入ったか
- 手順 4 の結果(`1.0.0` が出たか、エラーや「?」になったか)

を教えてください。確認が取れ次第、フェーズ 2(デコード実装)に進みます。

---

## フェーズ 2: デコード機能の確認 (QRB_Decode / QRB_DecodeAll / QRB_LastError)

### 事前準備: テスト用画像の用意
リポジトリの `test-images/` ディレクトリに、テスト用の画像が用意されています。
もし見当たらない、または新規に作成したい場合は、ターミナルで以下を実行してください。

```bash
python3 /Volumes/DISK/dev/filemaker-plugin/qr-code/test-images/generate_images.py
```

`test-images/` ディレクトリ内に以下の3つのファイルが作成されます。
- `qr_normal.png` (デコード結果: `Hello FileMaker`)
- `qr_japanese.png` (デコード結果: `日本語テンプレート`)
- `qr_newline.png` (デコード結果: 改行を含む 3 行のテキスト)

### 手順 1. 最新のプラグインを再配置して FileMaker を再起動する
フェーズ 1 と同様に、ビルドされたプラグインを配置します。

```bash
cp -R /Volumes/DISK/dev/filemaker-plugin/qr-code/plugin/build/Release/QRBarcodeReader.fmplugin ~/Library/"Application Support"/FileMaker/Extensions/
```
FileMaker Pro を完全に終了（⌘Q）し、再起動します。

### 手順 2. テスト用データベースの作成
1. FileMaker Pro で空のデータベースを新規作成します。
2. 以下のフィールドを定義します。
   - `画像` (オブジェクト / コンテナフィールド)
   - `デコード結果_単一` (計算フィールド、またはデータビューアで監視)
   - `デコード結果_全件` (計算フィールド、またはデータビューアで監視)
   - `エラー情報` (計算フィールド、またはデータビューアで監視)
3. レイアウトに `画像` フィールドを配置し、レコードを1件作成します。
4. `画像` フィールドに、用意した `qr_normal.png` をドラッグ＆ドロップして挿入します。

### 手順 3. 動作確認 (データビューア等での確認)

データビューアの「監視」タブで、以下の式を順に評価してください。

#### ① QRB_Decode の評価
- **計算式:** `QRB_Decode( 画像 )`
- **期待する結果（qr_normal.png 時）:** `"Hello FileMaker"`
- **期待する結果（qr_japanese.png 時）:** `"日本語テンプレート"` (文字化けしていないこと)
- **期待する結果（qr_newline.png 時）:** 改行入りの 3 行の文字列

#### ② QRB_DecodeAll の評価
- **計算式:** `QRB_DecodeAll( 画像 )`
- **期待する結果（qr_normal.png 時）:**
  ```json
  [{"format":"QRCode","text":"Hello FileMaker"}]
  ```
- **FileMakerでのパーステスト:** `JSONGetElement( QRB_DecodeAll( 画像 ) ; "[0].text" )` が `"Hello FileMaker"` を返すことを確認してください。

#### ③ QRB_LastError (エラーハンドリング) の評価
エラー時の動作を検証します。

- **ケースA: 空のコンテナ**
  1. レコードを新規追加し、`画像` フィールドを空にします。
  2. 計算式 `QRB_Decode( 画像 )` を評価（結果は空文字 `""` になります）。
  3. 計算式 `QRB_LastError` を評価。
  4. **期待する結果:** `"1: Container is empty"` などのエラー情報。

- **ケースB: 画像以外のデータ**
  1. `画像` フィールドに、画像ではないテキストファイル（例: `CLAUDE.md` 等）を「ファイルとして挿入」します。
  2. `QRB_Decode( 画像 )` を評価。
  3. `QRB_LastError` を評価.
  4. **期待する結果:** `"1: Unsupported container stream format (must be image)"` などのエラー情報。
