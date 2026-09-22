# Request 008: スクリーンショット要求キューの堅牢化

日付: 2026-09-22

## 背景

Tank Physics Sandbox では `Screen Shot` ボタンと F12 キーの両方から
スクリーンショットを要求します。どちらも Tank 側の同じメソッドを経由し、
`SceneRenderer::RequestScreenshot()` を呼びます。

Debug ビルドで F12 を押すと、breakpoint により停止することがあります。
RtPbrSurvey は待機中の要求を `m_screenshotRequests`、GPU 読み戻し中の
capture を `m_pendingScreenshotCapture` として別に保持します。しかし
スクリーンショット pass は、pending capture がないことを assertion しています。

複数要求を受け入れる public API である以上、前の読み戻しが完了する前に次の
要求が入ることは正当な状態です。この状態で debugbreak してはいけません。

## 要求する挙動

- スクリーンショット要求を FIFO キューとして扱う。
- 前の GPU 読み戻し中でも、新しい要求を受け付ける。
- 一度に実行する読み戻しは一件とし、完了後に次の待機要求を開始する。
- 通常の連続した host 要求で assertion / debugbreak を発生させない。
- 受け入れたすべての要求について、成功または失敗の結果を返す。
- 既存の `SceneRenderer::RequestScreenshot()` と
  `ConsumeScreenshotResult()` の source compatibility を維持する。

Tank 側で F12、UI ボタン、自動ロールテストのそれぞれに特別な debounce を
実装する必要はありません。host 側での頻度制限は任意のポリシーとしては可能ですが、
renderer 正常動作の前提にしてはいけません。

## 検証

- 複数の要求をキューへ入れる回帰テストを追加する。
- 前の読み戻しが pending の間に次の要求を追加するケースを検証する。
- 結果が FIFO 順で返り、assertion / debugbreak がないことを確認する。
- CMake の `RtPbrSurvey::SceneRenderer` target と standalone app をビルドする。
- Tank 固有の名前や挙動を入れず、backend-neutral に保つ。
