# Step 2.1 停止観測値と初期閾値

## 1. 結論

Step 1で追加した `TankMotionObservation` を使い、停止、惰性走行、pivot turn、stationary turn、走行後の制動をヘッドレス環境で計測した。

初期候補値は各状態を十分に分離できたため、Step 2.2の状態機械へ採用する。停止は瞬間値では確定せず、すべての進入条件が0.25秒連続して成立した場合だけ `Stopped` とする。

## 2. 測定条件

- 固定タイムステップ: 1/60秒
- 初期安定時間: 3.0秒
- 走行時間: 3.0秒
- 旋回観測時間: 1.0秒
- 制動後の停止確認時間: 0.25秒（15 physics steps）
- テスト: `TankPhysics.MobilityObservationCharacterization`

## 3. 測定結果

| 状況 | Linear m/s | Angular rad/s | Max Slip m/s | Suspension m/s | Up・Contact | Lower Contacts |
|---|---:|---:|---:|---:|---:|---:|
| 安定停止 | 0.00000033 | 0.000000000048 | 0.00000044 | 0.0000 | 1.000000 | 6 |
| 入力解除直後の惰性 | 12.9533 | 0.00083 | 17.2719 | 0.00563 | 0.999999 | 6 |
| Pivot turn | 0.00144 | 5.50565 | 8.80819 | 0.00064 | 1.000000 | 6 |
| Stationary turn | 2.24524 | 1.84276 | 5.93478 | 0.03932 | 0.999994 | 6 |
| 制動後の停止確認時 | 0.01494 | 0.01431 | 0.0000 | 0.02744 | 1.000000 | 6 |

制動中に進入条件へ最初に入った時刻は0.90秒だったが、その後いったん条件外へ出た。連続して安定する区間は約1.32秒から始まり、1.55秒で0.25秒の確認を完了した。したがって単発の閾値通過だけでは停止確定にしない。

## 4. Step 2.2へ渡す初期値

```text
stoppedEnterLinearSpeed:       0.20 m/s
stoppedExitLinearSpeed:        0.35 m/s
stoppedEnterAngularSpeed:      0.20 rad/s
stoppedExitAngularSpeed:       0.35 rad/s
stoppedEnterTrackSlip:         0.20 m/s
stoppedExitTrackSlip:          0.40 m/s
stoppedEnterSuspensionSpeed:   0.10 m/s
stoppedExitSuspensionSpeed:    0.20 m/s
stoppedMinimumUpAlignment:     0.90
stoppedConfirmSeconds:         0.25 s
```

加えて次を必須条件とする。

- 観測値がすべて有限値である。
- 左右履帯が走行に必要なlower contactを持つ。
- 走行要求がない。

離脱閾値は進入閾値より高くし、センサー相当の微小ノイズで `Stopped` と `Moving` が往復しないようにする。接地数そのものは車輪構成へ依存するため、Step 2.2では固定総数6ではなく既存の `hasRequiredDriveContact` を使用する。

## 5. 判断

- 安定停止のノイズは全候補閾値より十分小さい。
- 惰性はlinear speedとtrack slipで停止から分離できる。
- Pivot turnはangular speedとtrack slipで分離できる。
- Stationary turnはlinear speed、angular speed、track slipのすべてで分離できる。
- 制動後は0.25秒の連続確認により一時的な閾値通過を除外できる。

以上によりStep 2.1を完了とし、次はStep 2.2の純粋な `MobilityStateMachine` を実装する。
