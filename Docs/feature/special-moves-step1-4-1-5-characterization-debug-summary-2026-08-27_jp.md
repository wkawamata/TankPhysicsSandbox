# Step 1.4～1.5 現状固定テスト・デバッグ状態サマリー

## 1. 目的

Step 1.4として入力解除後の惰性走行を現状挙動として固定し、Step 1.5として `Tracked Vehicle` デバッグダイアログの最上部へ常時確認可能な物理観測サマリーを追加する。

状態判定モデルはStep 2、特殊動作状態モデルはStep 3で実装するため、この段階では状態を推測表示しない。

## 2. Step 1.4 現状固定テスト

### 2.1 追加テスト

```text
TankPhysics.TrackedVehicleCoasting
```

確認内容:

- 車体を安定させてから3秒間前進入力を行う。
- 前進入力解除直後も物理速度が残る。
- 入力解除を物理的な停止として扱わない。
- 惰性走行中の `TankMotionObservation` が有限値を保つ。
- 新旧の水平速度観測値が一致する。
- 車体ローカル前進速度が残る。
- 左右履帯のlower contactが残る。
- brake入力によって惰性速度が低下する。

基準実行値:

```text
Driven speed:  12.9693 m/s
Coast speed:   12.9533 m/s
Braked speed:  6.31738e-13 m/s
```

このテストにより、将来の `MobilityStateMachine` は入力がゼロになったことだけで `Stopped` へ遷移してはいけないことを固定する。

## 3. Step 1.5 デバッグ状態サマリー

`Tracked Vehicle` ダイアログを次の構造へ変更した。

```text
Tracked Vehicle
  ├─ State Summary       常時表示
  └─ Controls Child      スクロール領域
       ├─ Tank Settings
       ├─ Simulation
       ├─ Telemetry
       └─ その他の設定
```

サマリー表示内容:

```text
Mobility: Unavailable (Step 2)
Special: Legacy (Step 3)
Input: Roll <value>
Motion Data: Valid / INVALID
Speed: <value> m/s
Angular: <value> rad/s
Contacts: L <count> (<lower> lower) R <count> (<lower> lower)
Max Slip: L <value> R <value> m/s
```

設計上の扱い:

- 状態サマリーは読み取り専用。
- `Mobility` はStep 2完了まで `Unavailable` と明示。
- `Special` はStep 3完了まで `Legacy` と明示。
- 未実装の状態を速度から推測して表示しない。
- 観測値が非有限の場合は `INVALID` を文字と色の両方で表示。
- 下部設定だけをchild window内でスクロールさせ、サマリーを上部へ残す。

## 4. 変更ファイル

```text
CMakeLists.txt
src/Ui/TrackedVehiclePanel.cpp
tests/TrackedVehicleCoastingTests.cpp
```

## 5. ビルド・テスト結果

### ビルド

```text
scripts\build.bat TrackedVehicleCoastingTests
scripts\build.bat TankSandbox
```

結果:

- `TrackedVehicleCoastingTests` Debugビルド成功。
- `TankSandbox` Debugビルド成功。
- runtime shader 28個の検証成功。

### テスト

```text
TankPhysics.TrackedVehicleCoasting: PASS
```

全CTest:

```text
成功: 49
失敗: 1
合計: 50
```

Tank関連テストはすべて成功した。失敗は既知の `RtPbrSurvey.AssetPath` のみで、RtPbrSurvey test executable側のHDRI配置不足が原因である。

## 6. Step 1.6で必要な手動動作確認

実装は手動GUI確認が必要な段階へ到達した。`scripts\run.bat` で起動し、次を確認する。

### 表示

- `Tracked Vehicle` ダイアログ最上部に `State Summary` が表示される。
- 下部設定をスクロールしても `State Summary` が隠れない。
- 文字が重ならず、ダイアログ幅内で読める。
- `Motion Data: Valid` と表示される。

### 停止中

- SpeedとAngularが停止近傍へ下がる。
- 左右のcontact数とlower contact数が表示される。
- Max Slipが停止近傍へ下がる。

### 走行・惰性

- 前進中にSpeedとMax Slipが更新される。
- 入力を離した直後もSpeedが0にならず、惰性走行を表示する。
- brake後にSpeedが停止近傍へ下がる。

### 旋回・ローリング

- 旋回中にAngularが増える。
- ローリング中も `Motion Data: Valid` を維持する。
- ローリング姿勢に応じてlower/upper contact数が変化する。
- ローリング後も表示更新が継続する。

## 7. 判定

```text
Step 1.4 characterization test:  完了
Step 1.5 debug summary実装:       完了
自動ビルド・テスト:              完了
Step 1.6 手動GUI確認:             完了
```

2026-08-28 手動確認結果:

| 確認項目 | 結果 |
|---|---|
| `State Summary` が最上部に表示される | 成功 |
| 下部をスクロールしてもサマリーが隠れない | 成功 |
| 走行入力解除直後もSpeedが残る | 成功 |
| 旋回中にAngularが増える | 成功 |
| ローリング中も `Motion Data: Valid` を維持する | 成功 |
| ローリング姿勢に応じてlower/upper接地数が変化する | 成功 |
| 停止後にSpeed、Angular、左右Max Slipが停止近傍へ収束する | 成功（すべて0.0） |

すべての手動GUI確認が成功したため、Step 1全体を完了とする。
