# Step 1.2 物理観測データ設計

## 1. 目的

走行・停止状態、迫撃ウイリー、ローリング、動作妨害を判定するために必要な物理観測値を整理し、Step 1.3で実装する読み取り専用データ境界を定義する。

このStepでは物理挙動、停止判定、特殊動作状態遷移を変更しない。

関連文書:

- [Step 1.1 現状テスト・挙動基準](./special-moves-step1-1-baseline-2026-08-26_jp.md)
- [迫撃・ローリング実装ステップ](./special-moves-implementation-steps-2026-08-26_jp.md)
- [迫撃・ローリング入力／状態遷移 仕様草案](./mortar-and-rolling-input-state-draft-2026-08-21_jp.md)

## 2. 現在のスナップショット生成

現在のsimulation更新順序は次のとおりである。

```text
TankController::PreStep()
  ↓
PhysicsWorld::Step(deltaTime)
  ↓
TankController::PostStep(deltaTime)
  ↓
TankState
  ↓
TrackedVehicleTestState / Rendering / UI
```

`TankState` は物理step完了後の `PostStep` で更新される。状態判定へ渡す観測値も、この同じ時点の値から生成する。

原則:

- 1つの観測スナップショットは1つの物理stepに対応する。
- 前stepと現stepの値を暗黙に混在させない。
- `stepIndex` と `timeSeconds` を観測値へ関連付ける。
- UI frameではなくphysics stepを観測更新単位とする。
- `deltaTime <= 0` では観測値を進めない。

## 3. 座標系と単位

現在の車体ローカル軸は次のとおりである。

```text
+X: 右
+Y: 上
+Z: 前
```

単位:

| 種類 | 単位 |
|---|---|
| 位置・距離 | m |
| 並進速度 | m/s |
| 角速度 | rad/s |
| UI表示用yaw speed | degree/s |
| 力積 | N·s |
| torque | N·m |
| 時間 | s |

状態判定では角速度をrad/sで統一し、degree/sはUI表示またはJSON設定境界だけで使用する。

## 4. 現在取得できる生観測値

### 4.1 車体

`TankState` に次が存在する。

| 観測値 | 状態 | 用途 |
|---|---|---|
| `stepIndex` | 既存 | 観測stepの識別 |
| `timeSeconds` | 既存 | 状態滞在時間、確認時間 |
| `body.position` | 既存 | 移動量、特殊動作開始位置 |
| `body.rotation` | 既存 | 姿勢、仰角、roll進行度 |
| `linearVelocity` | 既存 | 停止、滑走、復帰判定 |
| `angularVelocity` | 既存 | 停止、旋回、姿勢安定判定 |
| `speedMetersPerSecond` | 既存 | 水平面の速度 |
| `sleeping` | 既存 | 物理bodyのactive状態の補助情報 |

`speedMetersPerSecond` はworld XZ成分から計算した水平速度であり、落下や上下動を含まない。停止判定では水平速度だけでなく、全3軸の並進速度も確認する。

### 4.2 駆動

| 観測値 | 状態 | 用途 |
|---|---|---|
| `TankInput` | 既存 | プレイヤー要求の有無 |
| `TrackedDriverInput` | 既存 | Joltへ渡した駆動指令 |
| `engineRpm` | 既存 | 駆動状態の診断 |
| `transmissionGear` | 既存 | 前後進状態の診断 |
| `clutchFriction` | 既存 | 動力伝達状態の診断 |

`sleeping` や入力ゼロだけを停止判定には使用しない。走行入力が受理された場合は、実速度上昇前でも `Moving` へ戻すため、物理観測と入力観測の両方が必要である。

### 4.3 wheelと接地

各 `TrackedWheelState` に次が存在する。

| 観測値 | 状態 | 用途 |
|---|---|---|
| `trackIndex` | 既存 | 左右track集約 |
| `wheelIndex` | 既存 | wheel識別 |
| `upperSurface` | 既存 | 上下surfaceの区別 |
| `hasContact` | 既存 | 接地状態 |
| `contactPosition` | 既存 | 支持範囲、接触位置 |
| `contactNormal` | 既存 | 地面方向、姿勢安定 |
| `contactLongitudinal` | 既存 | 接地面の前後軸 |
| `contactLateral` | 既存 | 接地面の横軸 |
| `angularVelocityRadians` | 既存 | wheel回転、空転診断 |
| `longitudinalSlipMetersPerSecond` | 既存 | 前後方向の相対滑り |
| `suspensionVelocityMetersPerSecond` | 既存 | 接地の安定度 |
| 各種impulse | 既存 | 支持、駆動、横抵抗の診断 |
| `suspensionAtHardPoint` | 既存 | suspension限界 |

`longitudinalSlipMetersPerSecond` は次の値として取得されている。

```text
wheel angular velocity × wheel radius
  - contact point longitudinal velocity
```

接地していないwheelでは0で初期化されるため、集約時には必ず `hasContact` を条件にする。

## 5. 既存値から共通算出する派生観測値

利用側ごとの計算差を防ぐため、次を共通の観測スナップショットとして一度だけ算出する。

### 5.1 車体軸

```text
bodyRight
bodyUp
bodyForward
```

`body.rotation` からworld座標の単位vectorとして算出する。

### 5.2 車体ローカル速度

```text
localLinearVelocity
  lateral
  vertical
  forward

localAngularVelocity
  pitch
  yaw
  roll
```

world速度を車体軸へ射影する。停止判定、ウイリー進行、ローリング進行で同じ符号規約を使用する。

### 5.3 速度の大きさ

```text
linearSpeedMetersPerSecond
horizontalSpeedMetersPerSecond
angularSpeedRadiansPerSecond
```

- `linearSpeedMetersPerSecond` はXYZを含む全速度。
- `horizontalSpeedMetersPerSecond` は既存の `speedMetersPerSecond` と同じworld XZ速度。
- `angularSpeedRadiansPerSecond` はXYZ角速度vectorの長さ。

### 5.4 track別接地集約

左右trackごとに次を算出する。

```text
contactCount
lowerSurfaceContactCount
upperSurfaceContactCount
averageContactNormal
maximumAbsoluteLongitudinalSlip
averageAbsoluteLongitudinalSlip
maximumAbsoluteSuspensionVelocity
hasSuspensionHardPoint
```

停止状態の「再度走行可能」を判定する際は、少なくとも左右のlower surfaceに走行を支えられる接地が必要である。必要な個数はStep 2の調整値とし、Step 1.3では値の集約だけを行う。

### 5.5 全体接地集約

```text
totalContactCount
totalLowerSurfaceContactCount
totalUpperSurfaceContactCount
averageContactNormal
hasLeftDriveContact
hasRightDriveContact
hasRequiredDriveContact
```

`hasRequiredDriveContact` の最終的な閾値判断はStep 2で行う。Step 1.3では左右にlower surface contactが存在するという最小限の診断値として提供する。

### 5.6 データ健全性

```text
allFinite
```

状態判定に使用する車体、速度、接地集約が有限値かを示す。非有限値を0へ黙って置き換えず、`allFinite == false` として検出可能にする。

## 6. 用途別の必要観測値

### 6.1 走行・停止状態

必要:

- 全並進速度と水平速度。
- 全角速度と車体ローカル角速度。
- 左右lower surface contact。
- 前後方向のwheel slip。
- suspension velocity。
- 車体姿勢と平均接触法線。
- 走行入力が受理されているか。
- 観測値の有限性。

Step 2では、これらを入力として `Moving / StopCandidate / Stopped` を判定する。

### 6.2 迫撃ウイリー

必要:

- 開始時と現在の車体姿勢。
- body forward、up、right。
- pitch angular velocity。
- 並進速度と横滑り。
- 後部を含む必要wheel contact。
- suspension hard pointとsuspension velocity。
- 接触法線。

「後部接地」の厳密な集約には、wheelの前後位置または既存 `wheelIndex` の配置規約を使用できる。どのwheelを必要接地とするかはStep 6で決定する。

### 6.3 ローリング

必要:

- 開始時と現在の車体姿勢。
- body forward軸。
- roll angular velocity。
- 開始位置からの横移動量。
- 上下surfaceを含むwheel contact。
- 接触位置と法線。
- suspension hard point。

回転進行度は観測スナップショットへ固定値として保存せず、状態機械が開始姿勢と現在姿勢から算出する。

### 6.4 妨害判定

既存または派生値で確認できるもの:

- 姿勢進行の停滞。
- rollまたはpitch角速度の不足。
- 横移動の停滞。
- wheel contactとsuspension hard point。
- wheel impulseの増加。

現在不足しているもの:

- chassisまたは砲塔とmap geometryの直接接触。
- chassis contact point、normal、相対速度。
- chassisへ加わった接触impulseまたは反力。
- 状態機械が要求した制御出力と、実際に適用した出力の上限到達状態。

wheel contactだけでは、車体側面や砲塔が壁へ引っ掛かった状況を完全には判定できない。これらはStep 5およびStep 6の妨害検出実装前に、Jolt contact listenerまたは限定的なcollision queryを使う別の観測境界として追加する。

## 7. Step 1.3で実装する契約

Step 1.3では、既存のraw `TankState` を保持したまま、共通派生値をまとめた読み取り専用構造を追加する。

暫定名称:

```cpp
struct TankMotionObservation
{
    int stepIndex;
    float timeSeconds;

    Vec3 bodyRight;
    Vec3 bodyUp;
    Vec3 bodyForward;
    Vec3 localLinearVelocity;
    Vec3 localAngularVelocity;

    float linearSpeedMetersPerSecond;
    float horizontalSpeedMetersPerSecond;
    float angularSpeedRadiansPerSecond;

    std::array<TrackContactObservation, kTankTrackCount> tracks;
    int totalContactCount;
    int totalLowerSurfaceContactCount;
    int totalUpperSurfaceContactCount;
    Vec3 averageContactNormal;
    bool hasLeftDriveContact;
    bool hasRightDriveContact;
    bool hasRequiredDriveContact;
    bool allFinite;
};
```

track別構造の暫定名称:

```cpp
struct TrackContactObservation
{
    int contactCount;
    int lowerSurfaceContactCount;
    int upperSurfaceContactCount;
    Vec3 averageContactNormal;
    float maximumAbsoluteLongitudinalSlipMetersPerSecond;
    float averageAbsoluteLongitudinalSlipMetersPerSecond;
    float maximumAbsoluteSuspensionVelocityMetersPerSecond;
    bool hasSuspensionHardPoint;
};
```

配置方針:

- 物理・描画非依存の型として `src/Physics` に置く。
- Jolt型を公開interfaceへ含めない。
- `TankState` と同じphysics stepの値として公開する。
- `TrackedVehicleTestState` にも同じ観測値を転送する。
- UIはこの構造を読み取るだけにする。
- Step 2の閾値や状態enumをStep 1.3へ先行導入しない。
- 特殊動作の開始姿勢、進行度、状態滞在時間は状態機械側の責務とし、この構造へ含めない。

## 8. Step 1.3で追加しないもの

- `Moving / StopCandidate / Stopped` の判定。
- 迫撃またはローリングの状態enum。
- 妨害判定。
- chassis contact listener。
- 発射可否、弾道、攻撃範囲。
- UI表示用文字列。
- JSON調整値。

これらを観測値追加と分離することで、Step 1.3では物理挙動を変えずにデータ境界だけを検証できる。

## 9. Step 1.3の最低限のテスト方針

- identity rotationでbody axisがworld axisと一致する。
- 既知のrotationでlocal velocityの符号と軸が正しい。
- `linearSpeed` と `angularSpeed` がvector長と一致する。
- contactのないtrackで平均値が有限なzeroになる。
- 接地wheelだけがslip平均へ含まれる。
- 左右trackのcontact数と全体contact数が一致する。
- lowerとupper surface contactを混同しない。
- `stepIndex` と `timeSeconds` が `TankState` と一致する。
- 非有限入力を `allFinite == false` として検出する。
- 観測追加前後で既存の走行・ローリング基準テスト結果が許容範囲内に残る。

## 10. Step 1.2判定

```text
既存生観測値の棚卸し:        完了
共通派生観測値の定義:        完了
用途別の必要値分類:          完了
不足している妨害観測の特定:  完了
Step 1.3の契約案:             完了
物理挙動変更:                 なし
```

Step 1.3では、この契約を小さな型追加、集約処理、単体テスト、test bridge転送に分けて実装する。
