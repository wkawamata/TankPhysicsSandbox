# Request 009: DLSS Debug UI のグループ化

日付: 2026-09-22

## 背景

`SceneRendererDebugUi::DrawContents()` は現在、host の Render Settings
スクロール領域に `DLSS SR` と `DLSS RR` を連続したトップレベル項目として
表示します。

DLSS を調整しない間も縦方向の領域を使うため、関連する二つの項目をまとめて
折りたためるようにします。

## 要求する UI 変更

- `DLSS` の `ImGui::CollapsingHeader` を追加する。
- 既存の `DLSS SR` と `DLSS RR` の UI 全体をその内部に置く。
- Availability、Backend、Status、simple/detail mode、各設定 control は変更しない。
- 現在の renderer 動作と設定保存を維持する。
- renderer 所有・backend-neutral を維持し、Tank 固有の UI 状態や名前を追加しない。

初期状態が展開か折りたたみかは、既存 Debug UI の方針に合わせてよいです。

## 検証

- CMake の `RtPbrSurvey::SceneRenderer` target と standalone app をビルドする。
- embedded `DrawContents()` で `DLSS` header が一つだけ表示され、展開時に
  SR と RR の両方を操作できることを確認する。
- 既存 standalone `Draw()` も使えることを確認する。
