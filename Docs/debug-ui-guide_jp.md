# Tank Physics Sandbox デバッグUIガイド

## 1. この文書について

この文書は、Tank Physics Sandboxに現在実装されているデバッグUIの用途と操作方法をまとめたものです。

対象ウィンドウ:

- `Tank Sandbox`
- `Tracked Vehicle`
- `Camera`
- `Renderer Settings`
- `RtPbrSurvey Debug`

## 2. 共通の操作規約

### スライダーの操作

多くの数値項目にはスライダーと増減ボタンがあります。中央のスライダーで大きく調整し、左右のボタンで一定量ずつ微調整できます。既定値へ戻す操作も同じコントロールに含まれます。

### 黄色の項目名

項目名が黄色になった設定は、編集値と物理シミュレーションへ適用済みの値が異なります。`Apply & Reset`または`Apply Ground & Reset`を押すまで物理挙動には反映されません。

### 即時反映される設定

次の設定は原則として操作後すぐに反映されます。

- Camera設定
- RtPbrSurvey Debugの描画設定
- Body Materialの表示用マテリアル
- Physics Debug Overlay
- Track Shoe Display
- Show Track Proxies

物理形状、質量、駆動系、サスペンション、履帯配置、地面設定はResetを伴う再構築が必要です。

## 3. Tank Sandboxウィンドウ

起動直後に表示されるトップメニューです。

- `Box Drop`: 箱の落下試験へ移動します。
- `Tracked Vehicle Map`: Tracked Vehicleで使用するマップを選択します。
- `Reload Map Files`: `Config/Maps`のJSONマップを再読み込みします。
- `Start Tracked Vehicle`: 選択したマップで戦車シミュレーションを開始します。

Tracked VehicleまたはBox Dropから`ESC`を押すとトップメニューへ戻ります。

## 4. Tracked Vehicleウィンドウ

各機能GroupはExpandableな見出しになっており、起動時はCloseです。確認または調整するGroupだけを開くことで、縦に長いウィンドウを整理できます。

Tank Slot、AutoLoad、Apply/Save/Load、Pause/Step/Resetは常に操作できるようにウィンドウ上部へ配置されています。

`Reset GUI`はTracked Vehicleウィンドウの位置をClient座標`(10, 10)`、サイズを`560 x 720`へ戻します。Tank設定、Camera設定、シミュレーション状態は変更しません。

### 基本状態

ウィンドウ上部には次の状態が表示されます。

- シミュレーションStepと経過時間
- 車体位置
- 接地中の車輪数
- Sleep状態
- CPUフレーム時間

### Drive Telemetry

駆動系の調整結果を数値で確認します。

- `Speed`: 現在の水平速度。m/sとkm/hを表示します。
- `Maximum`: 現在のシミュレーションで記録した最高速度です。
- `0-10 m/s`: 停止状態から10 m/sへ到達するまでの時間です。
- `Engine`: 現在のエンジン回転数です。
- `Gear`: 現在のギアです。負数は後進、0はニュートラル、正数は前進です。
- `Clutch`: クラッチ接続率です。

`0-10 m/s`は停止中にリセットされ、スロットル入力で計測を開始します。

`Track Traction`には左右履帯ごとの伝達状態を表示します。

- `contact`: 接地している物理車輪数
- `drive`: 前後方向へ加えたImpulseの合計
- `lateral`: 横方向へ加えたImpulseの合計
- `slip`: 接地車輪の周速度と接地点速度の差の平均

Engine Torqueを増やしても加速しない場合、`slip`だけが大きくなっていればパワー不足ではなく空転が主因です。左右の`contact`やImpulseに大きな差がある場合は、サスペンション姿勢、摩擦、片側接地を確認します。

### Map

現在のマップ名と摩擦係数の色分けを表示します。

- 青: 低摩擦、`0.45`未満
- 緑: 標準、`0.45`以上`0.80`未満
- 赤: 高摩擦、`0.80`以上

### Physics Debug Overlay

物理側の履帯接触情報を描画します。

- Cyan: サスペンション
- Green/Orange: 接触状態
- Yellow: 接触法線

描画用の履板や車輪モデルとは異なり、物理計算が参照している接地点と方向を確認するための表示です。

### Gamepad

GameInputから取得したデバイス情報を表示します。

- 接続状態とデバイス名
- VID/PID
- Button、Axis、Switch数
- Gamepad mappingの有無
- Raw axisと押下中のraw button
- 左スティックとブレーキ状態

アナログ履帯入力はAxis 1と3がニュートラルへ戻った後に有効化されます。`waiting for neutral`表示中は誤発進防止のため入力されません。

### Ground

地面とランダム障害物を調整します。

- `Floor Size`: 床の一辺の長さ
- `Floor Friction`: 床の摩擦係数
- `Grid Enabled`: 方眼表示
- `Grid Spacing`: 方眼間隔
- `Obstacle Count`: ランダム障害物数
- `Obstacle Seed`: ランダム配置seed
- `Obstacle Area`: 障害物の配置範囲

`Apply Ground & Reset`で地面、障害物、戦車を再構築します。`Save Ground`と`Load Ground`はTank本体設定とは別の地面設定を保存・復元します。

### Physics Settings

- `Chassis Mass`: 車体質量
- `Recoil Impulse`: 発砲時の反動Impulse
- `Recoil Point Forward`: 反動作用点の前後位置
- `Recoil Point Height`: 反動作用点の高さ
- `Neutral Brake`: 無入力時ブレーキの有効化
- `Neutral Brake Strength`: 無入力時ブレーキ強度

### Rolling Parameter

Assault車体の横転操作と安定化を調整します。

- `Rolling Input`: ロール入力の有効化。既定はOFFです。
- `Roll Torque`: ロール開始時のトルク
- `Roll Distance`: 1回のロールで目標とする横移動距離
- `Torque Cutoff Angle`: ロールトルクを停止する傾斜角
- `Stabilization Torque`: 正立面または反転面へ安定させるトルク
- `Stabilization Damping`: ロール角速度の減衰

### Body Material

戦車を構成する描画パーツのマテリアルを調整します。

対象:

- Hull Upper / Lower
- Structure Upper / Lower
- Wheels
- Contacted Wheels
- Track Shoes
- Track Proxies
- Forward Marker

各マテリアルでAlbedo、Roughness、Metallic、Ambient Occlusion、Emissiveを調整できます。編集終了後に描画へ反映され、物理形状は変更しません。

`Body Material`の外にある`Color Wheels by Contact`をONにすると、接地車輪へ`Contacted Wheels`のマテリアルを使用します。`Save Visual`、`Load Visual`、`AutoLoad`は物理設定とは別のVisual設定を扱います。

### 履帯表示

- `Track Shoe Display`: 描画用履板の表示
- `Show Track Proxies`: 物理履帯位置を表す黒いproxyの表示

Track Proxyの表示をOFFにしても物理計算には影響しません。

### Turn Traction

- `Longitudinal Friction`: 履帯の前後方向摩擦。加速、制動、登坂、空転へ影響
- `Lateral Friction`: 履帯の横方向摩擦。直進安定性、横滑り、旋回抵抗へ影響
- `Stationary Inner Track Ratio`: 片側入力が0のとき、内側履帯へ与える速度比
- `Stationary Left/Right Track`: 信地旋回時の左右トラクション倍率
- `Pivot Left/Right Track`: 超信地旋回時の左右トラクション倍率

### Drive Response

- `Engine Torque`: 最大エンジントルク。主に発進と加速へ影響します。
- `Engine Max RPM`: 最大回転数。主に到達速度へ影響します。
- `Shift Down RPM`: シフトダウン回転数
- `Shift Up RPM`: シフトアップ回転数
- `Clutch Strength`: 接続済みクラッチの伝達強度
- `Final Drive Ratio`: 全前進・後進ギア比へ掛ける終減速比
- `Clutch Release`: 変速後にクラッチが完全接続するまでの時間

Final Drive Ratioを大きくすると駆動力寄り、小さくすると最高速寄りになります。高トルク設定では接地摩擦と車体姿勢も併せて確認してください。

### Body Yaw

- `Yaw Speed Limit`: 車体Yaw角速度の上限
- `Yaw Damping`: 左右履帯入力が等しいときのYaw減衰

現在Yaw速度が上限を超えた場合、`Tank Yaw Speed`が黄色になり`LIMITED`と表示されます。

### Track Layout Adjustment

物理車体、履帯、車輪配置を変更します。

- Track Width / Spacing
- Suspension Stroke
- Chassis Width / Length
- End Wheel Radius
- Road Wheel Radius
- Wheel Layout: `1+2+1`、`1+3+1`、`1+4+1`
- End Wheel Offset / Vertical Offset
- Road Wheel Vertical Offset
- Middle Wheel Offset
- Start Upside Down

これらは`Apply & Reset`後に有効になります。

### Tank設定のSave/Load

ウィンドウ上部でSlot 1、2、3を選択できます。

- `AutoLoad`: Slot切り替え時に自動Load
- `Apply & Reset`: 編集値で物理車両とマップ衝突形状を再構築
- `Save`: 選択Slotへ保存
- `Load`: 選択Slotから読み込み

Loadしただけでは、黄色表示される物理設定は現在の車両へ反映されません。必要に応じて`Apply & Reset`を実行します。

### Export glTF

現在の表示用戦車モデルをBlender等で読み込める形式へ出力します。

- `glTF (.gltf)`: ASCII JSON形式。内容確認やデバッグ向け
- `glB (.glb)`: Binary形式
- `Export Path`: 出力先
- `Export Tank glTF`: 出力実行

### Simulation

シミュレーション操作はウィンドウ上部に常時表示されます。

- `Pause / Resume`: シミュレーション停止・再開
- `Step Fwd`: Pause中に1 Stepだけ進める
- `Fire / Recoil`: 設定済み反動を車体へ適用
- `Reset`: 現在の設定とマップで車両を初期化

### Track Input

アナログ入力、Yaw速度、Joltへ渡した最終入力を表示します。

- `Left / Right / Roll`: 正規化済みアナログ入力
- `Tank Yaw Speed`: 制限適用前後の車体Yaw状態
- `SetDriverInput`: JoltのTrackedVehicleControllerへ渡したForward、左右履帯比、Brake

入力問題を調査するときは、Gamepad raw axis、Track Input、SetDriverInputの順に確認します。

## 5. Cameraウィンドウ

`Mouse Control`はマウス入力の用途を選択します。

- `Gameplay`: 通常マウスを将来のゲーム操作用に予約
- `Alt Gesture`: Alt＋マウスで現在構図からDebug Cameraへ移行。既定値
- `Always Debug`: 任意のマウス操作でDebug Cameraへ移行

`Alt + Mouse`は現在構図からDebug Slotへ移行します。Follow TankがONなら位置追従は維持しますが、回転Chaseは解除します。Camera 1、2、3は変更しません。Debug CameraのYaw/PitchはTank Chase目標から独立します。`Alt + Ctrl`専用操作はありません。

Follow Cameraの位置YawはワールドY軸で計算します。Camera UpはワールドYを視線平面へ射影して毎フレーム再生成し、Z Rollを蓄積しません。車体のPitchとRollは反映しません。PerspectiveとOrthographicの切替は、注視距離での表示高さを合わせて補間します。OrthographicからPerspectiveへはTransition開始時、PerspectiveからOrthographicへはTransition完了時に投影方式を切り替えます。

Debug Orbitの水平回転はワールドY軸を使い、Camera UpをワールドY基準で再生成します。Look Down上限はRtPbrSurvey Arcballと同じ88度です。戦車移動時は位置と注視点だけが追従し、背後方向へ引っ張る回転補正は行いません。

ImGui操作中はDebug Cameraへ切り替わりません。

### Camera Slot

Camera 1、2、3とDebug Cameraをメモリおよびファイルへ保存できます。

- `AutoLoad`: Slot選択時、未読または変更されたファイルだけを読み込み
- `Save Camera`: 現在カメラを選択Slotへ保存
- `Load Camera`: 選択Slotから明示Load

Gamepad Button 4は`1 -> 2 -> 3`、Button 7は逆方向へ切り替えます。Debug CameraはGUIからのみ選択します。

### Follow Tank

- `Follow Distance`: 戦車との距離、最大250m
- `Look Down Angle`: 水平を0度とする見下ろし角。現在の上限は88度
- `Position Speed`: 位置追従速度
- `Rotation Speed`: 向き追従速度
- `Yaw Speed Limit`: カメラの最大Yaw回転速度
- `Yaw Damping`: カメラYaw角速度の加速・減速係数
- `Damping`: 追従全体の減衰

`Follow Tank`がOFFのときはRear High、Rear Quarter、Side High、Top RearのプリセットとマウスOrbit操作を使用できます。

### Projection

- `Perspective`: 透視投影。`FOV Y`を20～120度で調整します。
- `Orthographic`: 平行投影。`Ortho Height`で表示範囲を調整します。

FOV 0はOrthographicを意味しません。Projectionを明示的に切り替えます。

## 6. Renderer Settingsウィンドウ

RtPbrSurvey SceneRendererの共有設定をTank側で保存・復元します。

- `Save`: `Config/renderer_debug.json`へ保存
- `Load`: ファイルから復元
- `Reset`: Renderer設定を既定値へ戻す
- `Capture`: スクリーンショットを保存
- `F12`: Captureのショートカット

Camera、Tank物理設定、Tank Visual設定、Ground設定はRenderer設定ファイルへ含まれません。

## 7. RtPbrSurvey Debugウィンドウ

### Back Buffer

背景クリア色を調整します。

### PBR Lighting

直接光の有効化、方向、強度、色を調整します。Light Directionは直接光とRayQuery Shadowの方向へ反映されます。

### Environment Mapping

- IBL Enabled / Intensity
- Diffuse IBL / Specular IBL
- Show Skybox / Skybox Preview / Preview Exposure
- Asset HDRおよびProcedural環境
- Auto Update / Apply Environment

`Auto Update`がOFFの場合、`Apply Environment`を押すまで環境リソースを再生成しません。

### Tone Mapping

None、Reinhard、ACESと関連パラメータを調整します。

### RayQuery Shadow

RayQuery Shadowの有効化、Soft Shadow、Normal Bias等を調整します。Tankの動的車体ではNormal Bias `0.05`が安定確認済みの暫定基準です。Shadow Mask表示とPauseを併用すると時間変化と自己交差を切り分けやすくなります。

### Hybrid Reflection

Hybrid Reflection、寄与率、Material Gate、各種Hit Overlayを調整します。Hit Overlayは交差位置、法線、Material ID、UV、Albedo等のデバッグに使用します。

### Render View

- Rendering Path: Forward / Deferred
- View Mode: Lit、G-buffer、Shadow Mask等
- Debug LightPass Gradient

G-bufferやShadow Maskの確認にはDeferredを選択します。

## 8. キーボード操作

- `W / S`: 前進・後進
- `A / D`: 左右旋回
- `Shift + A / D`: 超信地旋回
- `Q / E`: 左右ロール
- `Space`: ブレーキ
- `ESC`: Top Menuへ戻る
- `F12`: スクリーンショット

ウィンドウが入力Focusを失った場合、誤発進防止のため車両入力はNeutralへ戻されます。

## 9. 調整時の推奨手順

1. Flat mapでDrive TelemetryとSetDriverInputを確認します。
2. Engine Torque、Max RPM、Final Drive Ratioを1項目ずつ変更します。
3. `Apply & Reset`後、0–10 m/sと最高速度を比較します。
4. Physics Debug Overlayで左右の接地状態を確認します。
5. 摩擦、坂道、段差マップで同じTank Slotを比較します。
6. 良好な値をTank SlotへSaveします。

CLIでは次の形式で同じTank設定とマップを再現できます。

```powershell
build\Debug\TankPhysicsCli.exe `
  --test map `
  --map Config\Maps\mobility_course.json `
  --tank-settings Config\tank_physics_slot1.json `
  --settle-steps 180 `
  --steps 480 `
  --throttle 1
```
