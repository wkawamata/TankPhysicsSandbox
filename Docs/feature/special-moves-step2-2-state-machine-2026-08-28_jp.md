# Step 2.2 純粋な走行状態機械

## 1. 実装結果

Jolt Physics、描画、UIへ依存しない `MobilityStateMachine` を追加した。

```text
Moving
  └─ 停止進入条件成立 ─> StopCandidate

StopCandidate
  ├─ 0.25秒継続 ──────> Stopped
  └─ 条件喪失 ─────────> Moving

Stopped
  └─ 離脱条件成立 ─────> Moving
```

1回の `Update` で実行する遷移は最大1回とする。停止確認時間が0秒の場合も、`Moving` から直接 `Stopped` には移らず、次回更新で確定する。

## 2. 入出力

入力:

- `TankMotionObservation`
- 正規化済みの走行要求 `driveRequested`
- physics stepの経過時間

出力 `MobilityStateSnapshot`:

- `state`
- `stateTimeSeconds`
- `stopCandidateProgress`（0～1）
- `lastTransitionReason`

状態機械は入力や物理車体を変更しない。

## 3. 判定

`Moving` と `StopCandidate` ではStep 2.1の進入閾値を使用する。`Stopped` では高めの離脱閾値を使用し、閾値付近での状態往復を防ぐ。

次の場合は安全側の `Moving` を選ぶ。

- 走行要求がある。
- 観測フラグまたは判定対象値が非有限値。
- 左右履帯に必要な走行接地がない。
- body upと平均接触法線のdot値が下限未満。
- 速度、角速度、履帯スリップ、サスペンション速度が該当閾値以上。

各離脱には `MobilityTransitionReason` を付け、後続のデバッグ表示で理由を確認できるようにした。

## 4. テスト

`TankPhysics.MobilityStateMachine` で次を確認した。

- `Moving → StopCandidate → Stopped`
- 0.25秒未満では `StopCandidate` を維持
- 停止候補進行度
- 進入・離脱閾値間のヒステリシス
- 走行要求による即時離脱
- 速度、角速度、スリップ、サスペンション、接地、姿勢、無効観測の各遷移理由
- 非有限値では `Moving`
- 1 physics stepにつき最大1遷移
- `Reset` による初期状態復帰

観測および特性テストを含む関連3テストはすべて成功した。

## 5. 境界

この段階では状態機械を `TankController` へ統合していない。また、閾値は独立した設定型の初期値であり、JSON保存・読込にはまだ接続していない。

次のStep 2.3で設定を `TankSettings` とJSONへ追加し、後方互換性と不正値処理を実装する。
