# 迫撃砲: 実弾発射・爆発ダメージ・緊急ブレーキ (2026-10-07)

## 概要

迫撃特殊行動に実際の発射機能を追加した。発射ボタンで円筒型の弾頭が放物線飛行して着弾し、
着弾地点中心の球状範囲に爆発ダメージを与える。併せて、発射直後に通常弾が誤発射される
不具合の修正と、走行中でも緊急ブレーキで速やかに迫撃姿勢へ入れる機能（ブレーキ強度は
調整可能）を実装した。

## 1. Mortar Parameters ウィンドウの再編

- `ImGui::SeparatorText` でグループ化し、数値項目を次の順に並べ替えた。
  - Fire Angle（角度系）: Min Fire Angle / Max Wheelie Angle / Raise Rate / Return Rate
  - Center Distance（距離系）: Minimum / Maximum Center Distance
  - Attack Radius（半径系）: Radius at Minimum / Maximum Distance
  - Stance（剛性系）: Stance Torque / Stance Damping / Emergency Brake（新規）
- Min Fire Angle のスライダー下限を 0 に緩和（ clamp 下限も 0 ）。0 の場合、
  姿勢進入直後に canFire が成立し、発射可能までの死帯がなくなる。
- 照準円の cue は canFire 未成立時に灰色で表示（従来は黄橙色）。

## 2. 迫撃発射の実装

### 入力トリガー

- MortarAiming 状態で発射ボタン（ゲームパッド X / キーボード fireAssault）の
  立ち上がりエッジで 1 発発射。
- 発射と同時にホイーリー姿勢を解除し、状態は Idle へ戻る。
- 1 ジェスチャにつき 1 発。レバーを出し続けたままでは再発射しない
  （認識器は再発射のためにはレバーを中立に戻して再ジェスチャが必要）。

### 弾道

- 弾は Jolt 剛体ではなく、半陰的オイラー積分による物理放物線として飛行させる。
  衝突判定は assault 弾と同じレイキャスト経路（ランプや箱に当たり、自車両は無視）。
- 仰角は照準角度を [10, 80] 度にクランプ、初速度は発射高さ補正つきの弾道解で、
  平坦地上では照準円の中心に着弾する。
  `v0 = sqrt(g * R^2 / (2 * cos(e) * (R * sin(e) + h * cos(e))))` （h = 砲口高さ）
- 同時飛行上限 `mortarMaximumProjectileCount`（既定 8 発）。

### 弾頭モデルとエフェクト

- 弾頭は円筒メッシュ（半径 0.12 m, 全長 1.0 m）。速度方向へ+y 軸を回転で合わせ、
  飛行姿勢を視覚化する。
- 着弾エフェクトは発光オレンジの球インスタンス。爆発半径まで 0.25 秒で膨張後、
  0.5 秒で収縮して消える。静的地形には着弾マークが持続表示される。
- プレゼンタ側に弾/爆発のリソースプール（`EnsureMortarCapacity`）を追加。

### 爆発ダメージ

- 着弾地点中心の球状範囲（照準距離に応じ `mortarMinimumAttackRadiusMeters` 〜
  `mortarMaximumAttackRadiusMeters` で補間）で破壊可能オブジェクトへダメージ。
- 減衰は距離に対する線形（中心で全ダメージ、縁で 10% 下限）。
- 既定ダメージ `mortarExplosionDamage = 80`。
- 判定は箱の AABB 最近点と爆発中心の距離。

## 3. 不具合修正: 迫撃発射直後に通常弾が発射される

- 原因: 発射ボタン長押しで迫撃を発射すると、状態が即 Idle に戻るため、
  次のステップから通常弾の長押し連射（レベル発射）が発動していた。
- 修正: `m_fireConsumedByMortar` フラグを追加。`FireMortarShell()` が発射ボタン
  入力を消費したことを記録し、ボタンが離されるまで通常弾の発射条件を遮断する。
  離ボタン後の新しい押し直しは通常弾として正常動作する。

## 4. 緊急ブレーキ（走行中の迫撃姿勢進入）

### 動作

- 走行中に迫撃ジェスチャを行うと `MortarStarting` が即座に受理され、
  **ブレーキサブフェーズ**（MortarStarting かつ mobility 未停止）に入る。
- ブレーキサブフェーズ中は:
  - 運転力入力を上書き: スロットル無効 + ホイールブレーキ
    （強度 = `mortarEmergencyBrakeAmount`、ユーザーブレーキ入力との大きい方）
  - 車体姿勢（ホイーリー）の stance トルク、照準更新、発射トリガーは保留
  - mobility 状態機械の drive-request を抑止（スロットルスティックを押し続けて
    も停止判定が妨害されない。ロールのブレーキ先行例と同じ設計）
- mobility が Stopped になった瞬間に通常の照準シーケンスへ移行。
  ジェスチャのやり直しは不要。

### 調整

- 新設定 `mortarEmergencyBrakeAmount`（0〜1、既定 1.0）。
  Mortar Parameters ウィンドウ Stance グループの "Emergency Brake" スライダーで
  調整可能。0 の場合は緊急ブレーキなし（惰性で停止を待つ、従来の挙動に近い）。
- Mortar Profile の保存/読み込み（JSON キー `emergencyBrakeAmount`）に含めた。
  バージョン 1 のまま、欠落フィールドは既存値を維持する後方互換。

## 5. Rolling の緊急ブレーキ強度調整

- ロールはもともと走行中リクエストを受理して安全停止ゲートまで固定ブレーキ
  （brake = 1.0）で減速する仕組みがあった（`rollBrakingToStart` /
  `preparingRollThisFrame`）。これを迫撃と同じ形式で調整可能にした。
- 新設定 `rollEmergencyBrakeAmount`（0〜1、既定 1.0）。ブレーキサブフェーズの
  適用値は `max(ユーザーブレーキ入力, rollEmergencyBrakeAmount)`。
- Rolling Parameters ウィンドウに "Emergency Brake" スライダーを追加。
- Rolling Profile の保存/読み込み（JSON キー `emergencyBrakeAmount`）に含めた。
  バージョン 1 のまま、欠落フィールドは既存値を維持する後方互換。

## 6. 迫撃砲の威力・弾速パラメータ

Mortar Parameters ウィンドウに "Warhead" グループを追加した（Attack Radius と
Stance の間）。

- **Explosion Damage**（威力）: `mortarExplosionDamage`（0〜500、既定 80）。
  従来からあった設定だが、初めて UI に露出した。着弾爆発の中心ダメージ。
- **Muzzle Velocity**（弾速）: 「Auto Muzzle Velocity」チェックボックス +
  `mortarMuzzleVelocityMetersPerSecond` スライダー（1〜50、既定 20）。
  - Auto ON（既定）: 従来の弾道解（仰角 = 照準角度、必要初速度を計算）。
    スライダーはグレーアウトされ編集不可。
  - Auto OFF: 設定初速度を **同一軌道の時間スケーリング**として扱う。自動解の
    初速度を k 倍し、その弾にのみ重力を k^2 倍適用する（k = 設定値 / 自動解初速度、
    0.25〜5 にクランプ）。放物線は幾何学的に完全に同一で、滞空時間だけが
    1/k になる。弾速を上げるほど同じ弧のまま速く着弾し、下げると遅くなる。
    着弾点は常に照準円中心。
  - 実装: `MortarShotRequest` / `MortarProjectileState` に `gravityScale` を追加し、
    `AdvanceMortarProjectiles()` の重力項に乗算。設定は
    `mortarMuzzleVelocityAuto` / `mortarMuzzleVelocityMetersPerSecond`。
- 以上は Mortar Profile の保存/読み込み（JSON キー `explosionDamage` /
  `muzzleVelocityMetersPerSecond` / `muzzleVelocityAuto`）に含めた。後方互換。
- Configure でクランプ: 弾速 0〜100、ダメージ 0〜1000。

## 7. 不具合修正: 緊急ブレーキの再発動による姿勢ゲート振動

- 症状: 停止状態から迫撃ジェスチャ进入すると、仰角が 5 度台で振動し
  MortarAiming に到達できなくなった（SpecialMoveIntegration テスト失敗）。
- 原因: `IsMortarBraking()` が `MortarStarting かつ mobility != Stopped` を
  毎ステップ評価していた。姿勢トルクがサスペンションを一瞬不安定化
  （遷移理由 SuspensionUnstable）させるとモビリティが Stopped を離れ、
  ブレーキサブフェーズが再発動して姿勢トルク・照準更新が凍結。停止→発動→
  不安定→凍結→停止…の振動で canFire に到達しないデッドロック。
- 修正: ブレーキサブフェーズを**受理時ラッチ**方式に変更。
  `m_mortarBrakingPending` を MortarStarting 受理時に「受理時点で mobility が
  Stopped でない」場合のみ true に設定し、mobility が Stopped に到達した
  時点で恒久的に解除。以降、姿勢トルク起因でモビリティが一時的に Stopped を
  離れてもブレーキは再発動しない。走行中受理時の挙動（停止までブレーキ、
  姿勢 0 度のまま）は不変。

## 8. ホイーリー中の駆動入力ロック

- 仕様: 迫撃姿勢（ホイーリー）中は移動入力を受け付けない。従来は
  MortarStarting / MortarAiming 中でもスロットルが Jolt の運転入力へ
  そのまま通り、ホイーリー姿勢のまま走行できた。
- 修正: 運転入力生成部で、緊急ブレーキ副段階の後に追加の分岐を入れ、
  MortarStarting / MortarAiming の全期間で `forward = 0`・トラック中立
  （`leftRatio = rightRatio = 1.0`）へ上書き。ブレーキ入力は従来どおり
  有効で、`neutralBrakeEnabled` 時は中立ブレーキも適用する。
- 照準・姿勢トルク・発射トリガーのゲート（`!IsMortarBraking()`）は不変。
- 回帰テスト: MortarProjectile の最終シナリオで MortarAiming 中に
  スロットル全開 60 ステップを与え、車体の変位が 0.5 m 未満であることを確認。

## 9. Z ロール（反転）状態からの迫撃進入: 姿勢ターゲットのミラー

- 仕様: Rolling（180 度の Z ロール）後の反転状態でも Rolling 前と
  同じ操作が可能でなければならない。迫撃姿勢で反転を元に戻して
  逆向きになってはいけない。ホイーリーは反転状態でも**ワールド基準で
  前部が上がる**（後部が上がってはいけない）。
- 原因: 姿勢トルクのターゲット `targetUp` が常にワールド +Y 基準
  （直立 + ピッチ）で構成されていた。反転した車体では `pitchError` の
  `bodyRight` への射影がワールド +X 周りのトルクとなり、車体を横軸
  周り 180 度回転させて直立・逆向きに戻していた。
- 修正: `bodyUp.Dot(Y)` の符号 `upSign` で `targetUp` **全体**を反転する
  （`(Y * cos - flatForward * sin) * upSign`）。反転車体は反転姿勢と
  向きを保ったまま、車体前部をワールド上方向（空側）へ持ち上げる。
  直立時は従来と同一のターゲット。姿勢ピッチが 90 度未満のあいだ
  符号は安定で、ゲートや他経路は不変。
- 他経路は変更不要（検証済み）: mobility の Stopped 判定は
  `abs(bodyUp・接触法線)` を使い、駆動接触判定はトラック上面接触を
  含むため反転でも停止到達できる。発射解は bodyForward の水平射影と
  ワールド上向きの仰角のみを使うため、反転しても弾道は同じ。
- 回帰テスト: MortarProjectile にシナリオ追加 — ロールで反転着地し
  Stopped 安定後にジェスチャが MortarAiming を受理し、姿勢トルク
  120 ステップ後も `upY < -0.5`（反転維持）、`forwardZ > 0.5`
  （向き維持）、`forwardY > 0.15`（前部がワールド上方向へ上昇）を
  確認。修正前は各チェックが失敗することを再現済み。

## 変更ファイル

| ファイル | 内容 |
|---|---|
| `src/Physics/TankTypes.h` | `MortarShotRequest`、設定 `mortarMaximumProjectileCount` / `mortarExplosionDamage` / `mortarMuzzleVelocityAuto` / `mortarMuzzleVelocityMetersPerSecond` / `mortarEmergencyBrakeAmount` / `rollEmergencyBrakeAmount`、状態 `pendingMortarShot` / `mortarShotsFired` |
| `src/Physics/TankController.h/.cpp` | `FireMortarShell()`（弾道解と時間スケーリング）、発射トリガー、`m_fireConsumedByMortar`、`IsMortarBraking()`（受理時ラッチ `m_mortarBrakingPending`）、緊急ブレーキ上書き（mortar / roll 両方）、ホイーリー中の駆動入力ロック、mortar の drive-request 抑止、Z ロール姿勢での `targetUp` ミラー（`upSign`） |
| `src/Physics/SpecialMoveInputProcessor.h` | mortar リクエスト専用ゲート `mortarStartAllowed`（走行中受理を許可） |
| `src/Physics/TrackedVehicleTest.h/.cpp` | `MortarProjectileState`（`gravityScale` 含む）/ `MortarBlastState`、弾の進捗（時間スケーリング重力）・爆発適用・ショット引き継ぎ |
| `src/Rendering/TrackedVehicleScenePresenter.h/.cpp` | 弾/爆発プール `EnsureMortarCapacity`、円筒の速度方向合わせ、爆発スケールエンベロープ |
| `src/App/TrackedVehicleMode.cpp` | プール確保呼び出し、cue 灰色表示 |
| `src/Ui/TrackedVehiclePanel.cpp` | ウィンドウ再編、Min Fire Angle 下限 0、Emergency Brake スライダー（Mortar / Rolling 両ウィンドウ）、Auto Muzzle Velocity チェックボックス（ON でスライダーグレーアウト）、発射数テレメトリ |
| `src/Physics/MortarProfile.h/.cpp` | `emergencyBrakeAmount` / `muzzleVelocityMetersPerSecond` / `muzzleVelocityAuto` / `explosionDamage` のプロファイル追加 |
| `src/Physics/RollingProfile.h/.cpp` | `emergencyBrakeAmount` のプロファイル追加 |
| `tests/MortarProjectileTests.cpp` | 新規テスト（`TankPhysics.MortarProjectile`） |
| `tests/TrackedVehicleRollTests.cpp` | ロール緊急ブレーキ強度の検証ケース追加 |
| `CMakeLists.txt` | 上記テストターゲット登録 |

## テスト

`tests/MortarProjectileTests.cpp`（ctest 名 `TankPhysics.MortarProjectile`）:

- [x] ジェスチャで MortarAiming に到達し canFire 成立（Min Fire Angle 0）
- [x] 発射で弾 1 発生成・カウンター加算・ホイーリー即時解除
- [x] 発射後に発射ボタンを 30 ステップ押し続けても通常弾 0 発（誤発射回帰テスト）
- [x] 弾は発射点より上に上がる（放物線）、着弾後に除去
- [x] 爆発は照準中心 +-2 m 以内に着弾
- [x] 近傍の破壊可能箱は撃破、半径外の箱は無傷（球状ダメージ）
- [x] レバー保持中の再発射なし
- [x] 走行中ジェスチャで MortarStarting 受理、ブレーキ中は姿勢 0 度のまま、
      2.5 秒以内に停止して MortarAiming 到達、その後に発射成功
- [x] 弾速固定 25 m/s で同一弧のまま滞空時間が自動より短い（着弾点は照準中心 +-2 m）
- [x] 弾速 5 m/s では同一弧のまま滞空時間が自動より長い（着弾点は照準中心 +-2 m）
- [x] MortarAiming 中にスロットル全開 60 ステップでも車体変位 0.5 m 未満
      （ホイーリー中の駆動入力ロックの回帰テスト）
- [x] ロールで反転着地 -> Stopped 安定 -> ジェスチャ受理 -> 姿勢トルク
      120 ステップ後も反転姿勢・向きを維持し、前部がワールド上方向へ
      上昇（Z ロール進入の回帰テスト）

`tests/TrackedVehicleRollTests.cpp`（既存ブレーキ開始テストに追加）:

- [x] `rollEmergencyBrakeAmount = 0.4` でブレーキ待機中の適用ブレーキ値が 0.4、
      スロットル上書き（forward = 0）が効く
- [x] 弱い緊急ブレーキでも予約済みロールは停止後に開始する

実行結果: 全 85 テスト成功（HEAD 時点の既知の失敗
`MobilityStepCli` / `MobilityAllCli` / `RollingSpeedOptimization` を除く）。

## GUI 確認状況

- [x] 迫撃発射・放物線飛行・円筒弾頭・球状爆発・ダメージ（ユーザー実機確認 OK）
- [x] 発射直後の通常弾誤発射なし（修正後、ユーザー実機確認待ち）
- [ ] 走行中ジェスチャからの緊急ブレーキ進入、Emergency Brake スライダーの効き（ユーザー実機確認待ち）
- [ ] Rolling の Emergency Brake スライダーの効き（ユーザー実機確認待ち）
- [ ] ホイーリー中のスロットル無効（駆動ロック）（ユーザー実機確認待ち）
- [ ] Auto Muzzle Velocity チェックボックスとスライダーのグレーアウト、
      Auto OFF での弾速挙動（ユーザー実機確認待ち）
- [ ] Z 反転状態からの迫撃進入: 反転姿勢・向きを保ち、前部が上がる
      （ユーザー実機確認待ち）

Status: done
