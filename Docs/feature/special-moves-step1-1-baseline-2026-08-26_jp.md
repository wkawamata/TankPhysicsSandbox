# Step 1.1 現状テスト・挙動基準

## 1. 目的

迫撃・ローリング実装を開始する前に、現在のビルド、テスト、走行、旋回、ローリング試作の基準を記録する。

この文書は現状の保証範囲と未保証事項を明確にするためのものであり、挙動変更は含まない。

関連文書:

- [迫撃・ローリング実装ステップ](./special-moves-implementation-steps-2026-08-26_jp.md)
- [迫撃・ローリング入力／状態遷移 仕様草案](./mortar-and-rolling-input-state-draft-2026-08-21_jp.md)

## 2. 確認環境

```text
確認日:       2026-08-26
構成:         Debug / x64
Generator:    Visual Studio 17 2022
Branch:       codex/frame-timing-telemetry
Commit:       f3043b5 Update RtPbrSurvey for glTF baseColorFactor support
CTest件数:    48
```

確認開始時点の作業ツリーには、今回作成した次の未追跡文書が存在した。

```text
Docs/feature/mortar-and-rolling-input-state-draft-2026-08-21_jp.md
Docs/feature/special-moves-implementation-steps-2026-08-26_jp.md
```

ソースコードの変更は存在しなかった。

## 3. ビルド結果

実行コマンド:

```text
scripts\build.bat
```

結果:

- `TankSandbox` Debugビルド成功。
- Jolt Physics、RtPbrSurvey、Tankの各ライブラリをリンク成功。
- `TankSandbox.exe` のStreamline host import検証成功。
- Debug出力フォルダのruntime shader 28個を検証成功。
- ビルドエラーなし。

## 4. 全自動テスト結果

実行コマンド:

```text
ctest --test-dir build -C Debug --output-on-failure
```

結果:

```text
成功: 47
失敗: 1
合計: 48
```

Tank Physics、Tank Input、Tank App、Tank Renderingのテストはすべて成功した。走行、旋回、ローリングを含むTracked Vehicleテストもすべて成功した。

失敗したテスト:

```text
RtPbrSurvey.AssetPath
```

失敗理由:

```text
Failed to load HDRI:
C:\work\TankPhysicsSandbox\build\External\RtPbrSurvey\Debug\Assets\Environment\default_environment.hdr
```

`default_environment.hdr` はTank実行ファイル側の `build\Debug\Assets\Environment` には存在するが、RtPbrSurveyテスト実行ファイル側の `build\External\RtPbrSurvey\Debug\Assets\Environment` にはコピーされていない。

この失敗は特殊動作やTank Physicsの挙動失敗ではなく、RtPbrSurvey単体テストのruntime asset配置問題として記録する。Step 1.1では修正しない。

## 5. 主要物理テスト結果

次の7テストを詳細出力付きで再実行し、すべて成功した。

```text
TankPhysics.TrackedVehicleNeutral
TankPhysics.TrackedVehicleForward
TankPhysics.TrackedVehiclePower
TankPhysics.TrackedVehicleSteering
TankPhysics.TrackedVehiclePivot
TankPhysics.TrackedVehicleStationaryTurn
TankPhysics.TrackedVehicleRoll
```

### 5.1 Neutral

```text
PASS TrackedVehicle neutral
```

現在のテストが保証する内容:

- 300 step後の車体位置と姿勢が有限値。
- 車体が床上へ安定し、飛び去らない。
- 無入力時の横・前後ドリフトが0.25 m未満。
- 無入力時の車体roll、pitchが小さい。
- 線速度が0.2 m/s未満へ収束。
- 角速度が0.2 rad/s未満へ収束。
- 少なくとも1個のwheel contactが存在。
- wheel、suspension、contact観測値が有限。

未保証:

- `Moving / StopCandidate / Stopped` の状態分類。
- 停止確認時間とヒステリシス。
- スリップ中や外力で押されている場合の停止判定。

### 5.2 Forward / Power

```text
Forward distance:      62.841 m
Power distance:        62.841 m
Power lateral drift:   0.00017922 m
0 to 10 m/s:           2.21668 s
```

現在のテストが保証する内容:

- 前進入力で10 m/sを超える。
- 前進距離が期待範囲へ達する。
- 最大速度と0～10 m/s加速時間を観測できる。
- 前進時の横流れが小さい。
- 接地wheelの縦方向スリップ値が有限。
- 接地wheelへ駆動インパルスが発生する。

未保証:

- 入力解除後の惰性から停止までの時間。
- 横滑りと履帯空転を統合した制御可能性判定。

### 5.3 Steering

```text
Left yaw:  -2.63014 rad
Right yaw:  2.60927 rad
```

現在のテストが保証する内容:

- 左右steeringで反対方向のyaw回転が発生する。
- 左右の回転量が概ね対称。

### 5.4 Pivot Turn

```text
Left yaw:       -0.466554 rad
Right yaw:       0.456015 rad
Left distance:   0.00166071 m
Right distance:  0.00125229 m
```

現在のテストが保証する内容:

- 左右pivot turnで反対方向のyaw回転が発生する。
- pivot中の並進移動が小さい。

### 5.5 Stationary Turn

```text
Left yaw:        2.61233 rad
Right yaw:      -2.61227 rad
Left distance:   2.54923 m
Right distance:  2.54651 m
```

現在のテストが保証する内容:

- 片側trackを停止するstationary turnが左右に動作する。
- 左右結果が概ね対称。

### 5.6 Rolling

```text
Operated body up Y: -1.0
Settled body up Y:  -0.999991
Lateral distance:    2.51982 m
```

現在のテストが保証する内容:

- `rollingInputEnabled` が有効な場合だけroll入力を物理制御へ渡す。
- roll入力で車体が正立姿勢から回転する。
- 90度相当のcutoff後、入力保持だけで角速度が増加し続けない。
- 入力解除後、正立または反転の近い姿勢へ安定化する。
- 1回のroll操作で横へ2.0～2.8 m移動する。
- 今回の基準実行では反転姿勢へ収束した。

未保証:

- 明示的なローリング状態遷移。
- 0～90度、90～180度の進行度。
- 90度地点での完了可能性判定。
- 障害物による妨害検出。
- 開始姿勢への安全復帰。
- 妨害理由とユーザー向けサイン。
- 入力保持による複数回の連続ローリング。
- ローリング開始時の停止状態ガード。

## 6. 現在取得可能な観測値

`TankState` および `TrackedVehicleTestState` から次を取得できる。

- step indexとsimulation time。
- 車体positionとrotation。
- 車体linear velocityとangular velocity。
- speed、maximum speed、0～10 m/s time。
- engine RPM、transmission gear、clutch friction。
- yaw speedとyaw limit状態。
- body sleeping状態。
- 各wheelのtransform。
- suspension origin、direction、length、velocity、hard point状態。
- wheel angular velocity。
- suspension、longitudinal、lateral impulse。
- longitudinal slip speed。
- contact有無、位置、法線、縦方向、横方向。

現時点では左右trackの接地数、平均・最大スリップ、角速度の大きさなどはUI側または利用側で集約する必要がある。

## 7. GUI確認結果

既定手順として次を使用した。

```text
scripts\run.bat
```

`TankSandbox` processは起動したが、今回の自動確認環境ではmain window handleを生成せず、Windows画面操作ツールから対象windowとして取得できなかった。processは応答状態だったがCPUを継続使用したため、今回起動した検証processを終了した。

したがって、次のGUI目視確認はStep 1.1時点で未完了である。

- 平地での走行と停止。
- 左右steering、pivot turn、stationary turn。
- ローリングの見た目と入力解除後の姿勢。
- `Tracked Vehicle` debug dialogの現在配置。

自動物理テストは成功しているが、GUI確認完了とは扱わない。

## 8. Step 1.1で確認した既知の課題

1. `RtPbrSurvey.AssetPath` がテスト出力先のHDRI不足で失敗する。
2. `scripts\run.bat` から起動したprocessを今回の自動確認環境でwindowとして取得できない。
3. 走行・停止状態モデルは未実装。
4. 特殊入力認識と特殊動作状態機械は未実装。
5. デバッグダイアログ上部の固定状態サマリーは未実装。
6. 現在のローリング試作は反転または正立への安定化を許容し、1回転完了を保証していない。

## 9. Step 1.1判定

```text
ビルド:                    成功
Tank関連自動テスト:        成功
全CTest:                   47 / 48成功
主要物理基準値の採取:      完了
現状保証・未保証の整理:    完了
GUI目視確認:               未完了
```

Step 1.2の観測データ整理へ進むための基準は採取できた。ただし、GUI起動確認と `RtPbrSurvey.AssetPath` は既知の未解決事項として残す。
