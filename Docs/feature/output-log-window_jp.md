# Outputログウィンドウ

F9、またはトップメニューの「Output (F9)」で表示を切り替える。

- レベル: Trace / Debug / Info / Warning / Error / Fatal。チェックボックスで複数選択。
- カテゴリ: 任意のUTF-8文字列。Categoryで複数選択。新しいカテゴリは初期状態で表示。
- Search: メッセージの部分一致。Exclude: 一致したメッセージを除外。大文字小文字を区別し、日本語も検索可能。
- 判定はレベル AND カテゴリ AND Search AND NOT Exclude。
- 色: Traceは灰、Debugは水色、Infoはテーマの通常色、Warningは黄、Errorは赤、Fatalはピンク紫。
- Copy filteredは現在のフィルターに一致するログ全体をコピーする（画面外の行も含む）。
- Clearは保持ログを削除し、フィルターは維持する。Reset filtersは全フィルターを解除する。
- Auto-scrollは最下部にいるときだけ新規ログへ追従する。複数行メッセージに対応。

## 出力API

```cpp
#include "Diagnostics/Log.h"
using namespace Tank::Diagnostics;
Write(LogLevel::Info, "Physics", "物理ワールドを初期化しました");
Write(LogLevel::Warning, "Vehicle", "履帯がスリップしています");
```

`TankDiagnostics`はC++標準ライブラリだけに依存し、PhysicsからUIやRenderingへの依存はない。
ワーカースレッドから書き込める。UIは変更時だけスナップショットを取得し、フィルター結果をキャッシュして可視行だけ描画する。

保持上限は10,000件。超過時は古いログから破棄し、discardedに件数を表示する。
1件のメッセージは4,096バイト、カテゴリは128バイトまで（切り詰め時の「...」を除く）。
UTF-8の文字境界で切り詰める。CRLFはLFに正規化し、末尾改行は除去する。
Fatalは表示レベルであり、プロセス終了などの副作用はない。

日本語表示には既存のメイリオ読み込みを使用する。Windowsの`Fonts/meiryo.ttc`がない環境では日本語フォントの導入・設定が必要。
ソースはUTF-8、CRLF。日本語リテラルを含むターゲットはMSVCの`/utf-8`でビルドする。

## 接続済みの出力

- アプリ起動ログ
- JoltのTraceとAssert（Assertの既存ブレーク動作は維持）
- 車両の既存OutputDebugStringログ
- 上記はWindowsデバッガへも`OutputDebugStringW`で同時出力

他のコードが直接呼ぶOutputDebugStringや標準出力、レンダラーの独自ログは自動収集しない。
必要な出力箇所から共通APIを呼ぶ。既存のD3D12ファイルログ機能は従来通り。

## 検証

`LogTests` / CTest名 `TankDiagnostics.Log`:
日本語の保持・検索・除外、レベルとカテゴリのAND条件、FIFO上限、Clear、
UTF-8境界の切り詰め、複数行、制御文字、4スレッド同時書き込みとスナップショット取得、Sinkの着脱を確認する。
### 2026-09-22 検証結果

- VS2022 Debug ALL_BUILD: 成功、ビルドエラー0。
- CTest: 79件中78件成功。追加したTankDiagnostics.Logは成功。
- TankPhysics.MobilityMapCliのみ失敗（forward_distance=5.03998、required=45）。
  今回の変更を含まないC:/work/TankPhysicsSandbox/build/Debug/TankPhysicsCli.exe
  （2026-09-21ビルド）へ同じ引数・同じmap/settingsファイルを渡しても同じ数値で失敗。
  走破挙動や期待値はこの変更では修正しない。
- scripts/run.batからtracked-vehicleを起動し、実機キャプチャでOutput・日本語・レベル色を確認。
- 変更テキストのCRLF、UTF-8 BOMなし、git diff --checkを確認。
- OpenCodeの読み取り専用レビューはプロバイダーのCLIバージョン要求で未実施。
