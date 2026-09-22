# Request 007: 埋め込み可能な SceneRenderer Debug UI

日付: 2026-09-22

## 要約

Tank Physics Sandboxでは、Tank所有の`Renderer Settings`と、RtPbrSurvey所有の
`RtPbrSurvey Debug`が別Windowになっている。これを一つの`Render Settings`
Windowへ統合したい。

## 依頼

- 既存の`SceneRendererDebugUi::Draw()`は互換性のため維持する。
- `ImGui::Begin()` / `End()`を呼ばず、hostが開いたChild領域内へ描画する
  `DrawContents()`相当の公開APIを追加する。
- Tankは公開済みの`GetUiFrameContext()`から`FrameIndex`と`CPU Frame`を取得し、
  Render Settings Windowの固定ヘッダーへ表示する。
- `Temporal Upscaler`の状態表示を`DLSS SR`セクションへ移す。
- `DLSS Ray Reconstruction`の状態表示を`DLSS RR`セクションへ移す。
- Ray Tracing情報などのrenderer診断は、スクロールする埋め込み内容の先頭に
  保持してよい。
- 埋め込み経路では、追加の`RtPbrSurvey Debug`トップレベルWindowを作成しない。
- RenderGraph Windowなどrenderer所有の機能は、埋め込み経路でも利用可能にする。

## 所有境界

RtPbrSurveyはrenderer debug controlsと状態を所有する。TankはWindow、固定領域の
Save / Load / Reset / Capture、可視性、レイアウトを所有する。Tank固有の名称、
ファイルパス、Physics、レイアウト方針をRtPbrSurveyへ持ち込まない。
