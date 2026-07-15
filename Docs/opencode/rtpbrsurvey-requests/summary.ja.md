# RtPbrSurvey への依頼要約 (OpenCode Mirror)

Source: `Docs/rtpbrsurvey-requests/summary.ja.md`

---

## 依頼 001: CMake から使える SceneRenderer ターゲット

- Tank Physics Sandbox は CMake ベースなので、`RtPbrSurvey::SceneRenderer` を CMake host から使える形にしたい。
- 現状の RtPbrSurvey は Visual Studio/MSBuild アプリ project 形状が強く、Tank 側で reusable renderer layer だけをリンクしにくい。
- 希望は、RtPbrSurvey 側に CMake target または CMake host 用の明確な組み込み手順を用意すること。
- `SceneRenderer` などの公開名は backend-neutral のままにする。
- Tank 専用 API や Jolt 依存は RtPbrSurvey 側に入れない。
- Tank 側の最初の用途は、固定 cube/sphere scene と Tank 用 ImGui window を表示する renderer proof。

---

### OpenCode 補足

[2026-07-15] サブモジュール `f90591d`（`origin/main` 最新）には `SceneRenderer`、`SceneBuilder`、`SetToolUiHandler`、`DebugCameraController` が全て存在。API 側のギャップは解消済み。残りは CMake での組み込み方法（ビルドシステム統合）のみ。

[2026-07-15] `SceneRenderer` は `RtPbrSurveyEngine` のラッパー。ホストが `GraphicsDevice` を所有し、`SceneRenderer` コンストラクタに渡す。`RunFrame()` に ImGui 描画用の `UiRenderHandler` ラムダを渡す。`SetToolUiHandler()` でツール UI 用コールバックを登録できる。
