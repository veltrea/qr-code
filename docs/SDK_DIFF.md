# TECHNICAL_NOTES.md と実 SDK(SDK 26)の相違記録

ヘッダを正とする(CLAUDE.md の規則)。実装はすべてヘッダ準拠。

## 1. `kMayEvaluateOnServer` は存在しない

- TECHNICAL_NOTES.md 2 節は型フラグの例として `kMayEvaluateOnServer | kDisplayInAllDialogs` を挙げているが、SDK 26 のヘッダに `kMayEvaluateOnServer` は存在しない。
- FM16(API 57)以降、登録 API の該当引数は `compatibleOnFlags` で、`FMXCalcEngine.h` の enum(`kDisplayInAllDialogs`、`kMacCompatible`、`kServerCompatible`、`kFutureCompatible` など)を使う。
- 本プラグインは `kDisplayInAllDialogs | kFutureCompatible`(= 全環境互換)を使用。

## 2. 登録 API は `RegisterExternalFunctionEx` を使用

- `RegisterExternalFunction`(説明文字列なし・旧式)と `RegisterExternalFunctionEx`(説明文字列あり、FM15 / `k150ExtnVersion` = 56 以降)の両方がヘッダに存在する。
- 対象が FileMaker Pro 19 以降(extnVersion 62 以降)のため、`RegisterExternalFunctionEx` のみ使用し、`version >= k150ExtnVersion` でガードしている。

## 3. バージョン定数の実値(FMXExtern.h)

| 定数 | 値 |
|---|---|
| `k150ExtnVersion` | 56 |
| `kCurrentExtnVersion`(SDK 26) | 77 |

`kFMXT_Init` は成功時に `kCurrentExtnVersion` を返す(MiniExample と同じ)。
