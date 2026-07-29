# 依頼 003: 完全な真上視点のためのhost camera up vector

Status: requested

## 日本語要約

- Tank側では `Chase`、`Top Ortho`、`Debug` の3つのカメラ情報を保持・切替・JSON保存したい。
- `Top Ortho` は近似ではなく、完全な真上からのOrthographic表示を必要とする。
- 現在のRtPbrSurveyは `XMMatrixLookAtLH` のupを常に `{0, 1, 0}` としている。
- 完全な真下視線では視線方向とupが平行になり、view matrixの基底が特異になる。
- `CameraState` にhost指定可能なup vector、または同等のorientation表現を追加してほしい。
- view/inverse view、camera constants、motion vector、RayQuery、temporal historyで同じ向きを利用すること。
- 新しい値がない既存hostでは従来の `{0, 1, 0}` を既定値として互換性を維持すること。
- Tank固有の3カメラ保存ファイルはTank側が所有し、RtPbrSurveyへ持ち込まない。

## Tank側の後続作業

- RtPbrSurveyの修正がmainへ入った後、submoduleを更新する。
- `Chase`、`Top Ortho`、`Debug` の3スロットをメモリに保持する。
- 3スロットをTank所有のJSONファイルへSave/Loadする。
- Camera UIから切替、現在値の記憶、微調整を可能にする。
