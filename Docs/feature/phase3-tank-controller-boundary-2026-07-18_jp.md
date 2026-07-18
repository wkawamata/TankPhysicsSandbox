# Phase 3 TankController境界

日付: 2026-07-18

## 目標

Joltの `TrackedVehicleController` を導入する前に、tank simulationの最初の描画非依存境界を定義する。

`TankController` はtank固有の物理挙動を所有する。RtPbrSurvey、DirectX 12、ImGui、DirectXMathには依存しない。

## 責務

`TankController` の責務:

- 既存physics world内でtank vehicle bodyとconstraintを生成、所有する。
- 正規化されたdriver inputを受け取る。
- fixed simulation stepでtank固有のcontrolを進める。
- CLI検証と描画で使うplain-data snapshotを出力する。

`TankController` の責務ではないもの:

- application windowまたはrendererの生成。
- stateからRtPbrSurvey scene instanceへの変換。
- keyboardまたはgamepad inputの直接読み取り。
- debug geometryの描画。
- 最初の増分でのtuning JSON読み込み。

## 入力境界

初期inputはJoltとplatform APIから独立させる。

```cpp
struct TankInput
{
    float throttle;
    float steering;
    float leftTrack;
    float rightTrack;
    bool brake;
};
```

最初のtracked-vehicle proofでは `leftTrack` と `rightTrack` を直接使用する。上位のthrottleとsteeringからのmappingは別の増分で追加する。

## 出力境界

初期snapshotにはplain scalar dataだけを含める。

```cpp
struct TransformState
{
    float position[3];
    float rotation[4];
};

struct TankState
{
    float timeSeconds;
    int stepIndex;
    TransformState body;
    float linearVelocity[3];
    float angularVelocity[3];
    bool sleeping;
};
```

wheel、suspension、contact、track animation stateは、consumerが必要になった時点で追加する。

## Consumer flow

```text
CLI inputまたはUI input
  -> TankInput
  -> TankController
  -> TankState
  -> CLI assertionまたはrendering adapter
```

rendering adapterは `TankState` をDirectXMath matrixへ変換できるが、この変換は `src/Physics/` の外側に置く。

## 実装ステップ

1. `TankInput`、`TransformState`、`TankState` をplain C++ typeとして追加する。
2. vehicle bodyをまだ生成しない、compile可能な `TankController` interfaceを追加する。
3. input clampと初期snapshot stateのunit-level testを追加する。
4. headless flat-ground tracked-vehicle testを追加する。
5. 同じtestをTank SandboxのUI sceneから利用できるようにする。
6. forward、steering、pivot turn assertionを別々の増分として追加する。

## 最初の増分境界

最初のコード増分ではplain-data type、最小controller lifecycle、testだけを追加する。境界を検証するまで、Jolt `TrackedVehicleController`、rendering integration、JSON tuning、debug drawingは対象外とする。

## 最初の増分結果

最初の境界実装として次を追加した。

- plainな `TankInput`、`TransformState`、`TankState` type。
- 正規化input clampを持つ最小 `TankController` lifecycle。
- 決定的な初期stateとfixed-step bookkeeping。
- 初期state、input clamp、正のtimestep、zero timestep拒否を検証するCTest executable。

controllerはまだJolt objectを所有しない。これによりtracked-vehicle setupを導入する前に、公開contractを独立してtestできる。
