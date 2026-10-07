# 非対称パースペクティブ・カメラ

## 目的

TankSandbox の Perspective Camera は、視線方向を回転させずに投影中心だけを移動できる。これにより、戦車を画面下寄りに配置しながら遠方を広く見せる構図を作れる。

## 操作

Perspective を選択すると、Camera パネルに次の値が表示される。

- `FOV Y`: 垂直方向の合計画角。
- `Horizontal Lens Shift`: 水平方向の投影中心。`0.00` は左右対称。
- `Vertical Lens Shift`: 垂直方向の投影中心。`0.00` は上下対称。
- `Up FOV` / `Down FOV`: 上下の個別画角を指定する高度設定。

`Up FOV` と `Down FOV` は各 `0.1` から `89.9` 度まで設定できる。編集結果は `FOV Y` と `Vertical Lens Shift` へ正規化されるため、画面アスペクト比が変わっても構図の意図を維持できる。

## 推奨値

見下ろし戦車カメラの出発点としては、`FOV Y = 45`、`Vertical Lens Shift = 0.20` が扱いやすい。これは上方を少し広く取り、戦車を下寄りに表示する。

個別に調整する場合は、`Up FOV = 30`、`Down FOV = 15` から試す。急激な非対称化は画面端のパースを強めるため、まず `Vertical Lens Shift` を `±0.30` 以内で使う。

## 実装境界

RtPbrSurvey の `Engine::CameraState` は `lensShiftX` と `lensShiftY` を持つ。Perspective の投影行列は near plane の左右・上下端を求め、`XMMatrixPerspectiveOffCenterLH` で生成する。値がゼロのときは、従来の対称投影と同じ行列になる。

Tank の Camera Slot は `perspectiveLensShiftX` / `perspectiveLensShiftY` として保存する。JSON schema v1 を読んだ場合は両方をゼロに補完し、次の Save では schema v2 として書き出す。

フレームごとの TAA jitter は、保存する Lens Shift には混ぜない。将来追加する場合は、レンダラーが一時的な jitter を加えた effective projection を作る。VR の眼ごとの視錐台も、この低レベルの off-center projection 境界へ直接入力する。

## 迫撃中のカメラ調整

Cameraウィンドウの `Mortar Camera` で、Follow Tankが有効なときの迫撃カメラを調整できる。

- `Mortar Pitch Offset`: 最大仰角時に追加する俯角。初期値は18度。
- `Mortar Distance Offset`: 最大仰角時に追加する追従距離。正値で遠ざかり、負値で近づく。初期値は0 m。合計距離は4〜250 mに制限される。
- `Mortar Response Speed`: 迫撃開始・終了時の変化への応答速度。大きい値ほど速い。初期値は6 /s。

追加量は迫撃の仰角進行度に比例する。編集は即時反映され、迫撃を解除すると通常の俯角・距離に戻る。`Save Camera`で選択中のカメラスロットへ保存する。これらはMortar ProfileではなくCamera Profileの設定である。旧カメラJSONでは従来と同じ初期値を使用する。
