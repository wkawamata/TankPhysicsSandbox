# Step 2 走行・停止状態モデル 実装計画

## 1. 目的

車体が走行中か、停止へ収束中か、特殊動作を開始できる安定停止状態かを、入力ではなく物理観測値を中心に判定する。

```text
Moving
  → StopCandidate
  → Stopped
```

迫撃とローリングは、Step 3以降で `Stopped` からだけ開始可能にする。

## 2. 基本方針

- 入力がゼロになっただけでは `Stopped` にしない。
- 惰性走行、横滑り、斜面滑落、履帯空転、外力による移動は `Moving` とする。
- `Stopped` は、再度走行を開始できる接地と姿勢を持つ安定状態とする。
- 状態機械はJolt、UI、描画へ依存しない。
- `TankMotionObservation` と正規化した走行要求だけを入力にする。
- 状態判定値はJSONで調整可能にする。
- 同じ入力列と観測列から同じ遷移結果を返す。

## 3. Step 2.1：停止観測値と初期閾値の確定

### 目的

停止、惰性、旋回、スリップで実際に観測される値を確認し、状態遷移の初期閾値を決める。

### 主な作業

- 停止後のSpeed、Angular、左右Max Slipを記録。
- 惰性走行中の値を記録。
- pivot turnとstationary turn中の角速度を記録。
- brakeによる停止過程を記録。
- lower contact数と平均接触法線を確認。
- suspension velocityの停止時ノイズを確認。
- 進入閾値、離脱閾値、停止確認時間の初期値を決定。

### 初期候補値

```text
stoppedEnterLinearSpeed:       0.20 m/s
stoppedExitLinearSpeed:        0.35 m/s
stoppedEnterAngularSpeed:      0.20 rad/s
stoppedExitAngularSpeed:       0.35 rad/s
stoppedEnterTrackSlip:         0.20 m/s
stoppedExitTrackSlip:          0.40 m/s
stoppedEnterSuspensionSpeed:   0.10 m/s
stoppedConfirmSeconds:         0.25 s
```

2026-08-28の特性テストで各状態を分離できることを確認し、Step 2.2へ渡す初期値として採用した。測定結果は [Step 2.1 停止観測値と初期閾値](./special-moves-step2-1-threshold-observation-2026-08-28_jp.md) を参照する。

追加の初期値として、suspension離脱閾値を0.20 m/s、body upと平均接触法線の最低dot値を0.90とする。

### 完了条件

- 停止時と走行時を分離できる初期閾値が決まる。
- 進入閾値より離脱閾値が高く、ヒステリシスが成立する。
- 停止時の物理ノイズを誤って `Moving` とし続けない。

## 4. Step 2.2：純粋な状態機械

**状態: 2026-08-28 完了。** 実装とテスト結果は [Step 2.2 純粋な走行状態機械](./special-moves-step2-2-state-machine-2026-08-28_jp.md) を参照する。

### 目的

物理エンジンに依存しない `MobilityStateMachine` を実装する。

### 型案

```cpp
enum class MobilityState
{
    Moving,
    StopCandidate,
    Stopped
};

enum class MobilityTransitionReason
{
    None,
    StopConditionsEntered,
    StopConfirmed,
    DriveRequested,
    LinearSpeedExceeded,
    AngularSpeedExceeded,
    TrackSlipExceeded,
    SuspensionUnstable,
    RequiredContactLost,
    PoseUnstable,
    InvalidObservation
};
```

状態出力:

```cpp
struct MobilityStateSnapshot
{
    MobilityState state;
    float stateTimeSeconds;
    float stopCandidateProgress;
    MobilityTransitionReason lastTransitionReason;
};
```

### 遷移

```text
Moving
  └─ 停止進入条件成立 ─> StopCandidate

StopCandidate
  ├─ 確認時間完了 ─────> Stopped
  └─ 条件喪失 ─────────> Moving

Stopped
  ├─ 走行要求受理 ─────> Moving
  ├─ 離脱閾値超過 ─────> Moving
  ├─ 必要接地喪失 ─────> Moving
  └─ 姿勢不安定 ───────> Moving
```

### 完了条件

- 純粋な単体テストだけで全遷移を確認できる。
- 1 physics stepで1遷移までとする。
- 非有限観測では安全側の `Moving` を選ぶ。
- `StopCandidate` の進行度を0～1で取得できる。

## 5. Step 2.3：設定とJSON

**状態: 2026-08-28 完了。** 停止判定値を `TankSettings` とschema version 17へ追加し、round-tripテストを通過した。詳細は [Step 2.3 停止判定設定とJSON](./special-moves-step2-3-settings-json-2026-08-28_jp.md) を参照する。

### 目的

停止判定値を `TankSettings` へ追加し、JSONで保存・読込可能にする。

### 主な作業

- 進入・離脱の線速度閾値。
- 進入・離脱の角速度閾値。
- 進入・離脱のtrack slip閾値。
- suspension安定閾値。
- 最低lower contact数。
- 許容姿勢角またはbody upと平均接触法線のdot閾値。
- 停止確認時間。
- JSON round-trip test。
- 不正値のclampまたはdefault fallback。

### 完了条件

- 設定が既存JSONとの後方互換性を保つ。
- 新しい設定がround tripする。
- 不正値でNaNや状態停止を起こさない。

## 6. Step 2.4：TankControllerへの統合

**状態: 2026-08-29 完了。** `PostStep`で観測後に状態機械を更新し、`TankState`とテスト状態へsnapshotを公開した。詳細は [Step 2.4 TankController統合](./special-moves-step2-4-controller-integration-2026-08-29_jp.md) を参照する。

### 目的

physics step後の観測値を使って状態機械を更新し、`TankState` へ状態を公開する。

### 更新順序

```text
PreStep
  └─ drive requestを物理へ適用

PhysicsWorld::Step

PostStep
  ├─ raw TankState
  ├─ TankMotionObservation
  └─ MobilityStateMachine::Update
```

### 主な作業

- `TankController` が `MobilityStateMachine` を所有。
- `TankMotionObservation` とdrive requestを状態機械へ渡す。
- reset直後は `Moving` から開始。
- `TrackedVehicleTestState` へmobility snapshotを転送。
- `TankController` のphysics制御は変更しない。

### 完了条件

- 安定後に `Moving → StopCandidate → Stopped` と遷移する。
- 走行入力受理時、実速度上昇前に `Moving` へ戻る。
- 入力解除直後の惰性中は `Moving` を維持する。
- reset、転倒、接地喪失でも有限な状態を保つ。

## 7. Step 2.5：デバッグ表示

### 目的

Step 1.5で追加した固定サマリーを正式な走行状態表示へ置き換える。

### 表示案

```text
Mobility: Moving / StopCandidate / Stopped
State Time: 0.00 s
Stop Progress: 0%
Reason: StopConfirmed
Speed: ...
Angular: ...
Max Slip: ...
Contacts: ...
```

### 主な作業

- `Unavailable (Step 2)` を実状態名へ置換。
- 状態滞在時間、停止候補進行度、直近遷移理由を表示。
- 閾値と現在値を比較可能にする。
- UIから状態を直接変更しない。
- 色だけに依存せず文字で表示。

### 完了条件

- 走行、惰性、停止過程を画面上で追跡できる。
- 閾値超過理由を確認できる。
- スクロールしても状態表示が隠れない。

## 8. Step 2.6：統合テストと手動確認

**状態: 2026-08-29 実装確認完了。** 全CTestは51/52成功、Tank系は全件成功した。手動GUI確認は後続作業として残し、既知のRtPbrSurvey HDRI欠落は別問題として扱う。詳細は [Step 2.6 統合テストと確認結果](./special-moves-step2-6-validation-2026-08-29_jp.md) を参照する。

### 自動テスト

- 静止観測を規定時間与えると `Stopped` になる。
- 確認時間未満では `StopCandidate` のまま。
- 惰性速度が残っていれば `Moving`。
- 線速度が低くても角速度が高ければ `Moving`。
- 車体速度が低くてもtrack slipが高ければ `Moving`。
- 必要接地がなければ `Stopped` にならない。
- 走行要求で直ちに `Moving`。
- 閾値付近のノイズで状態が往復しない。
- 同じ入力・観測列で同じ遷移結果になる。

### 手動確認

- 起動後、安定すると `Stopped` になる。
- 前進入力で即座に `Moving` になる。
- 入力解除後、惰性中は `Moving` を維持する。
- 停止直前に `StopCandidate` を経由する。
- 安定後に `Stopped` になる。
- pivot turn、stationary turn中は `Moving`。
- 滑走中は入力がなくても `Moving`。

### 完了条件

- 自動テストがすべて成功する。
- 手動確認で状態と見た目が一致する。
- 既存走行・ローリング挙動に退行がない。
- Step 3が `MobilityState == Stopped` を開始ガードとして利用できる。

## 9. 実装順序

```text
2.1 観測・閾値
  ↓
2.2 純粋状態機械
  ↓
2.3 設定・JSON
  ↓
2.4 TankController統合
  ↓
2.5 デバッグ表示
  ↓
2.6 自動・手動確認
```

## 10. Step 1からの継続事項

Step 1.6の手動確認では、停止1～2秒後のSpeed、Angular、左右Max Slipがすべて0.0へ収束した。この値を停止時の基準とし、Step 2.1では惰性、旋回、滑走中の値との差を測定して初期閾値を決定する。

Step 1で追加した惰性走行テストは、入力解除だけで `Stopped` にしないための回帰テストとして維持する。
