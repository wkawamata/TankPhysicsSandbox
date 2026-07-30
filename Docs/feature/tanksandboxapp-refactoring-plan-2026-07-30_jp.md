# TankSandboxApp リファクタリング計画

日付: 2026-07-30

## 背景

`src/TankSandboxApp.cpp` は約3250行、`src/TankSandboxApp.h` は約230行まで拡大している。

現在の `TankSandboxApp` は次の責務を同時に持っている。

- アプリケーションとDirectX 12レンダラーの初期化
- キーボード、マウス、ゲームパッド入力
- Top Menu、Box Drop、Tracked Vehicleのモード遷移
- ImGuiウィンドウと各種調整UI
- Cameraの追従、切り替え、補間、Save/Load
- Tank Physics、Environment、Material設定のSave/Load
- Box DropとTracked VehicleのScene構築と更新
- Physics Debug Overlayと履帯、車輪、接触情報の描画
- ScreenshotとD3D12 Debug Log

機能追加を続けやすくするため、`TankSandboxApp` をアプリケーション全体の調停役へ戻す。

## 目標

- `TankSandboxApp` はライフサイクル、モード遷移、各モジュールの接続を担当する。
- Camera、設定永続化、Scene表示、UIの詳細を個別モジュールへ分離する。
- PhysicsはRenderingへ依存しない現在の境界を維持する。
- JSON形式と既存の操作、画面、CLIの挙動を維持する。
- 各段階を独立してコミットし、常にDebugビルドとCTestが成功する状態を保つ。

## 対象外

- 物理挙動、Jolt設定、TankController APIの変更
- JSON schemaの意図的な変更
- UIデザインの全面変更
- RtPbrSurvey submodule内の変更
- 新しいゲーム機能の追加

## 目標構造

```text
src/
  TankSandboxApp.*
  App/
    AppMode.h
    CameraController.*
    CameraSettingsStore.*
    TankSettingsStore.*
    TankVisualSettingsStore.*
  Rendering/
    BoxDropScenePresenter.*
    TrackedVehicleScenePresenter.*
    PhysicsDebugOverlay.*
  Ui/
    CameraPanel.*
    TrackedVehiclePanel.*
    RendererSettingsPanel.*
```

名称は実装時に既存コードとの整合を確認して確定する。ファイルを増やすこと自体を目的にせず、明確な状態と責務を持つ単位だけを分離する。

## 実施ステップ

### Step 0: 現状を基準化

- 現在のCamera AutoLoad、FOV Spring、Tank AutoLoad作業を先に独立コミットする。
- DebugビルドとCTest結果を記録する。
- Camera Slot 1/2/3、Tank Slot 1、Tank Visualの手動確認項目を短いチェックリストにする。
- リファクタリング中は機能追加を別コミットに分ける。

完了条件:

- 作業ツリーのリファクタリング開始点が明確である。
- `TankSandbox` Debugビルドと全CTestが成功する。

### Step 1: 設定ファイルI/Oを分離

- Camera、Tank Physics、Tank VisualのJSONファイルI/OをStoreクラスへ移す。
- Storeはpath、read、write、エラー文字列だけを担当する。
- Serialize/Deserialize関数と設定構造体は現在の所有モジュールに残す。
- AutoLoad、選択中Slot、Apply/Resetの判断はまだApp側に残す。

候補:

- `CameraSettingsStore`
- `TankSettingsStore`
- `TankVisualSettingsStore`

完了条件:

- `TankSandboxApp` から直接の`ifstream`、`ofstream`処理が減る。
- 既存JSONをそのまま読み書きできる。
- Storeの成功、missing file、invalid JSONテストがある。

### Step 2: Camera制御を分離

- 3 Slot cache、dirty、file-loaded、AutoLoad状態を`CameraController`へ移す。
- Follow target、position velocity、yaw velocity、FOV velocityを移す。
- Slot切り替え、Button 4 cycle、Spring/Damping更新を移す。
- `Engine::CameraState`を入出力境界として、RtPbrSurvey固有操作はApp側に残す。
- Camera UIはこの段階ではApp側に残してよい。

完了条件:

- Follow切り替えで二段移動しない。
- Position、Yaw、FOVが保存Targetへ滑らかに収束する。
- AutoLoadは未ロードSlotだけファイルを読み、メモリ上のロード済みSlotを再利用する。
- Camera計算の単体テストが追加される。

### Step 3: Scene表示を分離

- Box DropのScene構築とtransform更新を`BoxDropScenePresenter`へ移す。
- Tracked Vehicleの車体、車輪、履帯、床、障害物のScene構築と更新を`TrackedVehicleScenePresenter`へ移す。
- Physics snapshotとVisual Settingsを入力にし、PresenterはPhysicsオブジェクトを所有しない。
- `ReloadSceneResources()`はgeometry/materialが変わる場合だけ呼ぶ現在の方針を維持する。

完了条件:

- AppはPhysics stateをPresenterへ渡すだけになる。
- PhysicsモジュールからRenderingへの依存が増えていない。
- Box DropとTracked Vehicleの表示が変更前と一致する。

### Step 4: Physics Debug Overlayを分離

- wheel ray、contact point、contact normal、track proxyの描画更新を`PhysicsDebugOverlay`へ移す。
- 表示ON/OFFと色は描画側の設定として保持する。
- Jolt Debug Rendererを直接Appへ埋め込まない。

完了条件:

- Overlay OFFでは通常描画へ影響しない。
- Overlay ONで接触状態と履帯の物理範囲を確認できる。
- Physics計算結果は変更されない。

### Step 5: ImGui Panelを分離

- Camera、Tank Design、Tank Input、Rolling、Ground、Material、Renderer設定をPanel単位へ分ける。
- Panelは編集対象の設定とcommand/callbackを受け取る。
- PanelからPhysicsやRendererを直接所有しない。
- Applyが必要な値の黄色表示、即時反映値、Save/Load操作を維持する。

完了条件:

- `DrawPhysicsTrackedVehicleUi()`が小さなPanel呼び出し中心になる。
- ImGui ID conflictがない。
- UI操作と既存ショートカットが維持される。

### Step 6: Mode単位の調停を整理

- `BoxDropMode`と`TrackedVehicleMode`の導入が有益か再評価する。
- 導入する場合はPhysics test、Presenter、Mode固有のpause/step/reset状態をまとめる。
- `TankSandboxApp`にはTop Menu、現在Mode、共通Renderer、入力ルーティングを残す。
- クラス化で状態の受け渡しが増える場合は無理に実施しない。

完了条件:

- `TankSandboxApp.cpp`の責務がアプリケーション調停に限定される。
- Mode間でCamera、設定Store、Rendererの所有権が曖昧にならない。

## 推奨コミット境界

1. `Document TankSandboxApp refactoring plan`
2. `Extract settings file stores`
3. `Extract tracked vehicle camera controller`
4. `Extract physics scene presenters`
5. `Extract physics debug overlay`
6. `Extract ImGui panels`
7. `Simplify application mode orchestration`

各コミットではリファクタリングと機能変更を混在させない。

## 検証

各Stepで最低限、次を実行する。

```bat
cmake --build build --config Debug --target TankSandbox
ctest --test-dir build -C Debug --output-on-failure
```

主要な手動確認:

- Top MenuからBox DropとTracked Vehicleへ遷移できる。
- ESCでTop Menuへ戻れる。
- Tankを前進、後退、旋回、超信地旋回できる。
- Pause、Resume、Step、Resetが動作する。
- Camera Slot 1/2/3とButton 4 cycleが動作する。
- Camera、Tank Slot 1、Tank Visualが起動時にAutoLoadされる。
- Material、Environment Mapping、Physics Debug Overlayが表示される。
- Screenshot CLIとGUI captureが動作する。

## リスク

- `TankSandboxApp`のメンバーを一度に移すと所有権と初期化順序を壊しやすい。
- Scene resource reloadのタイミング変更は表示消失やInstance count回帰につながる。
- Camera stateとFollow stateを別クラスへ移す際、現在値と保存Targetを混同すると二段移動が再発する。
- UI分離時にImGui IDとApplyタイミングが変わる可能性がある。

このため、設定Store、Camera、Scene、UIの順に小さく分離し、各Stepで物理確認してから次へ進む。
