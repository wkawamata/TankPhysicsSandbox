# ゲームパッド Chase Orbit操作

日付: 2026-08-14

## 目的

Game Camera 1/2/3のTank Chase中に、十字キーで一時的に戦車の周囲を確認できるようにする。既存Camera SlotやDebug Cameraの設定値は変更しない。

## 操作

- 十字キー左・右: Chase方向へ水平Orbit offsetを加える。
- 十字キー上・下: ChaseのLook Down angleへ仰角offsetを加える。
- 十字キー解放: SpringとDampingによりoffset 0へ戻り、戦車後方のChaseへ復帰する。

標準GameInput gamepad mappingと、先頭Raw switchの8方向Hat switchの両方に対応する。

## 角度範囲

- 水平操作target: -60から+60 degrees
- 水平hard limit: -90から+90 degrees
- 仰角操作target: 基準Look Down angleから最大45 degrees
- 最終Look Down angle: 0から`CameraController::kMaximumLookDownDegrees`
- 現在のRtPbrSurvey互換上限: 約80.2 degrees

## 状態所有

一時offsetと角速度は`CameraController`だけが保持する。Camera Slot JSONへ保存せず、Camera 1/2/3の基準Yaw offsetとLook Down angleにも書き戻さない。

次の場合、入力targetは自動的に0になる。

- Follow TankがOFF
- Tank Yaw ChaseがOFF
- Debug Camera slot

## テスト

`CameraControlTests`は次を検証する。

- 押下中に水平・仰角offsetがtargetへ近づく。
- 水平offsetがhard limitを越えない。
- Look Down angleがRtPbrSurvey互換上限を越えない。
- 入力解放後に両offsetが0へ戻る。
