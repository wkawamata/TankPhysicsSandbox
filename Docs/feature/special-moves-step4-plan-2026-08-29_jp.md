# Step 4 ローリング状態モデル化 計画

Step 3で確立した特殊動作状態基盤を、既存ローリング物理試作へ接続する。

## 内部ステップ

1. 既存の `rollInputLatched`、`rollPowered`、`rollTranslationActive` を状態と補助データへ整理する。**完了:** `RollingPhase`（None/PoweredRoll/BallisticRoll/Settling）を導入し、既存の出力条件を維持した。
2. `Windup`、`PoweredRoll`、`Evaluating`、`BallisticRoll`、`Settling` をローリング専用状態として定義する。**進行:** 公開フェーズ型と90度付近の1 step評価区間を追加した。
3. `MobilityState::Stopped` からだけWindupへ入る開始ガードを追加する。
4. 90度付近の完了可能性評価を状態遷移として記録する。
5. 着地後の安定化と入力保持時の再開始判定を `Settling` に分離する。
6. 現在の物理トルク・力出力は維持し、状態遷移と出力責務を段階的に移す。
7. 平地、入力保持、走行中拒否、既存ローリング退行のテストを追加する。

## 今回の境界

最初の変更では車体姿勢や力を直接書き換えない。状態モデルが完成するまで、既存のJolt出力を正とする。妨害検出と安全復帰はStep 5で扱う。
