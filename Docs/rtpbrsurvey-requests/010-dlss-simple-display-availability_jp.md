# Request 010: DLSS Simple Display に Availability を反映する

日付: 2026-09-22

## 背景

`SceneRendererDebugUi` の simple `DLSS SR` 表示には、ON/OFF と Mode があり、
simple `DLSS RR` 表示には ON/OFF があります。

機能が利用不可の場合、これらの control は disabled になります。しかし保存済みの
希望設定が ON だと、disabled の checkbox が ON のまま表示されます。実際には
有効化できない機能が動作中のように見えてしまい、renderer の診断時に紛らわしい状態です。

## 要求する挙動

- DLSS SR の simple 表示は、ON/OFF と Mode のみを維持する。
- DLSS RR の simple 表示は、ON/OFF のみを維持する。
- SR が利用不可なら、disabled の ON/OFF checkbox は OFF 表示にする。
- RR が利用不可なら、disabled の ON/OFF checkbox は OFF 表示にする。
- 利用不可の間は、simple 表示の checkbox を変更できないようにする。
- 一時的な利用不可により保存済みの希望設定を書き換えない。UI には実効的な
  availability 状態を表示し、backend が利用可能に戻れば保存済みの希望設定を再び使える。
- detailed mode の control、renderer capability 判定、public API、設定 serialization は維持する。

## 検証

- 可能であれば、保存済み ON かつ SR/RR が利用不可・利用可の状態を対象に、
  UI state または helper の focused test を追加する。
- CMake の `RtPbrSurvey::SceneRenderer` target と standalone app をビルドする。
- `Draw()` と embedded `DrawContents()` で同じ挙動を確認する。
