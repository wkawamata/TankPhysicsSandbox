# Phase 2 物理テストシーン

日付: 2026-07-17

## 目標

UI applicationとheadless CLIの両方から実行できる物理検証シーンを追加する。

UI applicationは最上位のImGui menuから開始する。このmenuから物理検証シーンへ入り、検証シーンで `ESC` を押すと最上位menuへ戻る。

## アーキテクチャ

物理検証は描画から独立させる。

共有するテストフロー:

```text
BoxDropTest
  -> Jolt stateを所有または駆動
  -> BoxDropState snapshotを返す
```

CLIとUIは同じテストobjectを使用する。

```text
TankPhysicsCli
  -> BoxDropTest

TankSandboxApp
  -> BoxDropTest
  -> SceneRendererによる可視化
```

描画はstate snapshotの可視化だけを担当する。物理コードはRtPbrSurveyに依存しない。

## 最初のシーン

最初の検証シーンは `box-drop` とする。

- static floor body
- dynamic box body
- fixed timestep update
- boxが床の上で静止したかによる最終PASS/FAIL判定

これはtank motionより意図的に単純にしている。allocator設定、Jolt type登録、body生成、重力、衝突、sleep state、physicsからrenderingへのstate経路を検証する。

## 初期CLI形式

```powershell
TankPhysicsCli.exe --test box-drop --steps 300 --dt 0.0166667
```

期待する出力:

```text
test=box-drop step=300 t=5.000 box.y=0.500 sleeping=true
PASS box-drop
```

## Step 1〜3の境界

最初の増分では次だけを追加する。

1. この設計メモ。
2. 描画に依存しないPhysics module skeleton。
3. `box-drop` test pathを選択できるCLI executable target。

Joltによるbox-drop simulationとUI scene transitionは後続の増分で追加する。

## Step 4の結果

`BoxDropTest` は内部に最小のJolt worldを所有し、公開headerにはJolt typeを露出しない。

実装した動作:

- Jolt allocator、factory、type registryをprocessごとに一度初期化。
- static floor bodyを生成。
- 設定した初期高度へdynamic box bodyを生成。
- 呼び出し側が指定したfixed timestepでphysics worldを更新。
- position、rotation、経過時間、step index、sleep stateを持つ `BoxDropState` snapshotを返す。
- 物理をRtPbrSurveyおよび描画コードから独立させる。

CLI検証:

```powershell
build\Debug\TankPhysicsCli.exe --test box-drop --steps 300 --dt 0.0166667
```

確認した結果:

```text
test=box-drop step=300 t=5.00001 box.y=0.48 sleeping=true
PASS box-drop final_y=0.48
```

PASS範囲は現在 `0.45 <= final_y <= 0.65` としている。小さなsolverやcontact marginの差に敏感にならず、統合経路を検証するための許容幅である。

## Step 5の結果

`TankSandboxApp` に次のapplication mode skeletonを追加した。

- `TopMenu`
- `PhysicsBoxDrop`

起動時は `TopMenu`。`Box Drop` buttonで `PhysicsBoxDrop` へ移動し、`PhysicsBoxDrop` で `ESC` を押すと `TopMenu` へ戻る。

## Step 6の結果

`PhysicsBoxDrop` は `TankPhysicsCli` と同じ `BoxDropTest` を実行し、RtPbrSurveyでstateを可視化する。

実装した動作:

- mode開始時にfloorとboxのrendering instanceを一度だけ構築。
- fixed timestepでphysicsを進め、各frameではbox transformだけを更新。
- Box Drop scene生成時だけscene resourceをreload。
- 1 instanceのTop Menu sceneから2 instanceのBox Drop sceneへ切り替えた後、表示instance数を明示的に設定。
- ImGuiへstep、経過時間、box高度、sleep state、Reset buttonを表示。
- RtPbrSurveyとDirectXMathの使用をapplication layerに限定し、`BoxDropTest` の描画非依存を維持。

scene reloadは以前のsceneの表示instance数を維持するため、明示的な表示数更新が必要である。これがない場合、Top Menuの1 instanceが引き継がれ、floorだけが表示されて2番目のboxが非表示になる。

## Step 7の検証

2026-07-18に次を検証した。

```powershell
scripts\build.bat TankPhysicsCli
build\Debug\TankPhysicsCli.exe --test box-drop --steps 300 --dt 0.0166667
scripts\build.bat TankSandbox
```

CLI結果:

```text
test=box-drop step=300 t=5.00001 box.y=0.48 sleeping=true
PASS box-drop final_y=0.48
```

両targetのbuildに成功した。落下するboxとReset動作の最終的な目視確認は、UIを確認できる時点まで保留する。

## 自動回帰テスト

CLIのbox-drop scenarioを `TankPhysics.BoxDrop` としてCTestへ登録した。このtestは同じ300 step simulationを実行し、process出力に `PASS box-drop` が含まれることを要求する。

```powershell
ctest --test-dir build -C Debug --output-on-failure
```
