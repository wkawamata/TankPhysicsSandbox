# Step 1.3 観測データ境界 実装結果

## 1. 目的

Step 1.2で定義した物理観測データを、物理挙動と状態判定を変更せず、`TankController`、ヘッドレステスト、後続システムから共通利用できる読み取り専用スナップショットとして実装する。

関連文書:

- [Step 1.2 物理観測データ設計](./special-moves-step1-2-observation-design-2026-08-26_jp.md)
- [迫撃・ローリング実装ステップ](./special-moves-implementation-steps-2026-08-26_jp.md)

## 2. 実装内容

### 2.1 観測型

`TankTypes.h` に次を追加した。

```text
TrackContactObservation
TankMotionObservation
TankState::motionObservation
```

`TankMotionObservation` が提供する値:

- physics step indexとsimulation time。
- world座標のbody right、up、forward軸。
- 車体ローカル並進速度と角速度。
- 全並進速度、水平速度、全角速度。
- 左右track別の接地数。
- lower surfaceとupper surfaceの接地数。
- 平均接触法線。
- 前後方向slipの最大絶対値と平均絶対値。
- suspension速度の最大絶対値。
- suspension hard pointの有無。
- 左右のlower drive contact。
- 状態判定に使用する値の有限性。

### 2.2 共通集約関数

次を追加した。

```cpp
TankMotionObservation BuildTankMotionObservation(const TankState& state);
```

この関数はJolt固有型に依存せず、既存 `TankState` だけから派生観測値を生成する。

主な規則:

- 車体ローカル速度はquaternionから生成したbody軸への射影で算出。
- 接地集約には `hasContact == true` のwheelだけを使用。
- lowerとupper surfaceを分離。
- slipとsuspension速度は絶対値で最大・平均を算出。
- 接地がない場合の平均値は有限なzeroを維持。
- 非有限値をzeroへ隠蔽せず `allFinite == false` で通知。
- 状態enum、閾値、停止判定は含めない。

### 2.3 physics stepへの接続

`TankController::PostStep()` でwheel snapshot生成後に `TankMotionObservation` を生成する。

```text
PhysicsWorld::Step
  ↓
TankController::PostStep
  ├─ raw TankState更新
  ├─ wheel snapshot更新
  └─ motionObservation生成
```

これによりraw値と派生観測値は同じ `stepIndex` と `timeSeconds` を持つ。

### 2.4 ヘッドレステスト境界

`TrackedVehicleTestState` に `motionObservation` を追加し、`TankController::State()` から転送するようにした。

Step 2以降の状態遷移テストは、Jolt APIへ直接依存せずこの観測値を利用できる。

## 3. 変更ファイル

```text
CMakeLists.txt
src/Physics/TankController.cpp
src/Physics/TankMotionObservation.cpp
src/Physics/TankMotionObservation.h
src/Physics/TankTypes.h
src/Physics/TrackedVehicleTest.cpp
src/Physics/TrackedVehicleTest.h
tests/TankMotionObservationTests.cpp
```

## 4. 追加テスト

CTest名:

```text
TankPhysics.TankMotionObservation
```

確認内容:

- identity rotationでbody軸とworld軸が一致。
- 90度yawでbody rightとforwardが正しい方向へ回転。
- world velocityからlocal velocityへの射影。
- 並進速度、水平速度、角速度のvector長。
- 左右track別のcontact集約。
- lowerとupper surface contactの分離。
- 接地wheelだけを使ったslip集約。
- suspension速度とhard pointの集約。
- 接地なしの場合の有限なzero値。
- 非有限入力の検出。
- 実際の `TrackedVehicleTest` からのstep/time転送。
- 安定車両の接地と有限観測値。

## 5. ビルド結果

実行:

```text
scripts\build.bat TankMotionObservationTests
scripts\build.bat ALL_BUILD
```

結果:

- `TankMotionObservationTests` Debugビルド成功。
- 全Debug targetビルド成功。
- `TankSandbox.exe` ビルド成功。
- runtime shader 28個の検証成功。
- 新規warningなし。
- RtPbrSurvey既存header由来のwarningは継続。

## 6. テスト結果

専用テスト:

```text
TankPhysics.TankMotionObservation: PASS
```

全CTest:

```text
成功: 48
失敗: 1
合計: 49
```

Tank側および新規観測テストはすべて成功した。既知の次のテストだけが失敗した。

```text
RtPbrSurvey.AssetPath
```

原因はStep 1.1と同じで、RtPbrSurvey test executable側のruntime asset folderに `default_environment.hdr` が存在しないことである。本変更による失敗ではない。

## 7. 非対象

Step 1.3では次を実装していない。

- `Moving / StopCandidate / Stopped`。
- 迫撃・ローリング状態。
- 状態判定の閾値。
- chassisとmapの直接接触観測。
- 妨害判定。
- デバッグダイアログへの表示。
- JSON設定。

## 8. Step 1.3判定

```text
観測型:                    実装完了
共通集約関数:              実装完了
TankController接続:        実装完了
TrackedVehicleTest転送:    実装完了
専用単体テスト:            成功
全Tank関連テスト:          成功
物理挙動変更:              なし
```

次はStep 1.4として、Step 1.1で整理した未保証部分から、観測データと現在挙動を固定するcharacterization testを追加する。
