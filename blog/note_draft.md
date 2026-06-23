# 【記事タイトル案】
FileMakerでQR・バーコードを高速読取！macOS/Windows両対応のプラグイン「QRBarcodeReader」をGitHubで公開しました

## はじめに
Claris FileMakerでシステムを開発する際、画像からQRコードやバーコードを読み取りたいというケースはよくあります。しかし、標準機能だけでは実装が難しかったり、動作速度や精度に課題を感じたりしたことはないでしょうか。

そこで、C++製の高速デコードライブラリ「ZXing-C++」をベースに、macOSとWindowsの両環境で動作するFileMaker外部関数プラグイン「QRBarcodeReader」を開発し、GitHubでオープンソースとして公開しました。

https://github.com/veltrea/qr-code

この記事では、このプラグインの特徴や使い方、導入手順についてご紹介します。

## 主な特徴と機能
このプラグインは、FileMakerのコンテナフィールドに保存された画像から、高精度かつ高速にQRコードや各種バーコードを検出します。

### 1. macOSとWindowsの両対応
macOS（Apple Silicon / Intelのユニバーサルバイナリ）と、Windows（64bit）の両プラットフォームでネイティブに動作します。クライアント混在環境でも安心して導入いただけます。

### 2. 複数バーコードの一括読み取りに対応
一般的なプラグインでは画像内の「最初の1個」しか読めないことが多いですが、本プラグインでは画像に含まれるすべてのコードを検出し、JSON配列形式でまとめて取得できます。

### 3. 詳細なエラー取得機能
「画像が破損している」「コードが検出されなかった」「コンテナが空だった」など、読み取りに失敗した原因を詳細に取得できる関数を用意しています。デバッグやユーザーへの親切なエラー表示に役立ちます。

### 4. マルチスレッド対応
FileMaker Serverでの並列実行も考慮し、スレッドセーフな設計になっています。

## 動作環境
- 対応OS：
  - macOS 11 (Big Sur) 以降
  - Windows 10 / 11 (64bit)
- 対応製品：Claris FileMaker Pro 19 以降 / FileMaker Server
- アーキテクチャ：
  - Mac：Apple Silicon (arm64) + Intel (x86_64)
  - Windows：x64 (64bit)

## 提供される関数と使用例
プラグインを導入すると、以下の外部関数が利用可能になります。

### QRB_Decode ( コンテナ )
画像から最初に検出されたコードの値をテキストで返します。
もっともシンプルな使い方です。

（計算式の例）
QRB_Decode ( テーブル::コンテナフィールド )

### QRB_DecodeAll ( コンテナ )
画像に含まれるすべてのコードの情報をJSON配列で返します。

（計算式の例）
QRB_DecodeAll ( テーブル::コンテナフィールド )

（返り値の例）
[
  { "text": "Hello FileMaker", "format": "QRCode" },
  { "text": "1234567890128", "format": "EAN13" }
]

このように、値とバーコードの規格（フォーマット）がJSON形式で取得できるため、FileMakerのJSON関数を使って簡単にパースできます。

### QRB_LastError
直前に呼び出したデコード関数のエラー情報を取得します。

（返り値の例）
- 正常時：空文字
- EAN13などでのエラー時：「3: コードが検出されなかった」など

エラーコードの定義：
- 0：正常
- 1：コンテナが空、または画像以外のフォーマット
- 2：画像データの展開に失敗（破損、または未対応形式）
- 3：コードが検出されなかった
- 9：内部エラー

## 導入方法
プラグインの導入は非常に簡単です。

1. GitHubリポジトリのReleaseページから環境に合わせたファイルをダウンロードします。
   - macOS：QRBarcodeReader.fmplugin.zip
   - Windows：QRBarcodeReader.fmx64.zip
2. ダウンロードしたファイルを解凍し、FileMakerのExtensionsフォルダにコピーします。
   - macOSの例：ライブラリ/Application Support/FileMaker/Extensions/
   - Windowsの例：C:\Users\<ユーザー名>\AppData\Local\FileMaker\Extensions\
3. FileMaker Proを再起動します。
4. 環境設定の「プラグイン」タブで「QRBarcodeReader」にチェックが入っていることを確認します。

これで、計算式の中で「QRB_」から始まる関数が使えるようになります。

## おわりに
本プラグインはMITライセンス（依存ライブラリはそれぞれのライセンス）で公開しており、商用・非商用問わず自由にご利用いただけます。

ソースコードや詳細なビルド手順、仕様についてはGitHubリポジトリをご確認ください。バグ報告や機能要望などのフィードバックも歓迎しています！

https://github.com/veltrea/qr-code
