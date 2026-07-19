# Phase 4 Headless tracked vehicle

日付: 2026-07-19

## 目標

描画非依存の `TankController` 境界を維持しながら、平らな床の上でJolt `TrackedVehicleController` を動かす最初のheadless proofを作る。

最初のproofではvehicle生成と安定したsimulationを検証する。前進、旋回、超信地旋回の合格条件は、それぞれ別の増分で追加する。

## 所有関係

```text
TrackedVehicleTest
  -> PhysicsWorldと平らな床を所有
  -> TankControllerを所有

TankController
  -> tank bodyとVehicleConstraintを所有
  -> vehicle step listenerを登録、解除
  -> TankStateを出力

PhysicsWorld
  -> Jolt runtime、PhysicsSystem、allocator、job systemを所有
```

test objectは `PhysicsWorld` より先に `TankController` を破棄しなければならない。

## 最初のproof

最初の実装増分では次を行う。

- 平らなstatic floorを生成する。
- box形状のdynamic tank bodyを1つ生成する。
- `TrackedVehicleControllerSettings` を使用するJolt `VehicleConstraint` を追加する。
- 小規模で左右対称なwheel構成を持つ2本のtrackを追加する。
- neutral driver inputで固定step数を実行する。
- positionとrotationが有限値で、車体が床の上に静止することを検証する。
- CTestだけから結果を公開し、UI sceneはまだ追加しない。

## Input mapping

Joltのtracked controllerはforward inputと、0ではない左右track ratioを受け取る。公式sampleでは次を使用する。

- 直進: forward `1`、ratio `(1, 1)`。
- 左旋回: forward `1`、ratio `(0.6, 1)`。
- 右旋回: forward `1`、ratio `(1, 0.6)`。
- 左超信地旋回: forward `1`、ratio `(-1, 1)`。
- 右超信地旋回: forward `1`、ratio `(1, -1)`。

`TankInput.leftTrack` と `TankInput.rightTrack` は正規化された要求track motionを表し、Joltのraw ratioではない。有効なJolt driver inputへの変換は `TankController` 内部の責務とし、movement assertionを導入する前にtestする。

## 対象外

- RenderingとImGui統合。
- Keyboardまたはgamepad input。
- JSON tuning。
- Turret、barrel、recoil、track animation。
- Slope、step、terrain friction test。
- Performance tuningまたは最終的なvehicle寸法。

## 検証

この増分では既存testをすべて維持し、専用のheadless tracked-vehicle construction testを追加する。`TankPhysicsCli` と `TankSandbox` はどちらもbuild可能な状態を維持する。

## 結果

headless tracked-vehicle proofは完了した。

- `TankController` はPimplを通してtank body、vehicle constraint、vehicle step listener登録を所有する。
- `TrackedVehicleTest` は安全な破棄順序で `PhysicsWorld`、floor、`TankController` を所有する。
- simulationの所有を `TankController::PreStep()`、`PhysicsWorld::Step()`、`TankController::PostStep()` に分け、worldがframeごとに一度だけ進むようにした。
- neutral安定性、前進、左右旋回、左右超信地旋回を、それぞれ独立したCTest executableで検証する。

Debugでの実測結果:

- 5秒間駆動した前進距離: `38.7587 m`。
- 旋回yaw: 左 `2.48281 rad`、右 `-2.50081 rad`。
- 超信地旋回yaw: 左 `2.7825 rad`、右 `-2.78252 rad`。
- 超信地旋回時の中心移動量: 両方向とも約 `0.0114 m`。
- CTest: `7/7` PASS。

次の増分は、既存のheadless stateを可視化するTank Sandbox UI sceneとする。物理挙動はこれらのtestと共有したまま維持する。
