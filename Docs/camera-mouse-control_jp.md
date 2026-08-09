# カメラ・マウス操作モード

## 目的

Camera Slot 1、2、3の構図から、画面を跳ばさずDebug Cameraへ移行してマウス調整できるようにする。同時に、将来のWASD＋マウスによるGameplay操作とDebug操作を競合させない。

## 設計方針

Camera SlotとMouse Control Modeを分離する。

- Camera Slot 1、2、3: 保存・復元できるカメラプリセット
- Debug Slot: デバッグ操作中の作業カメラ
- Mouse Control Mode: マウス入力をGameplayとDebugのどちらへ渡すかを決める

Slotはカメラの状態を所有し、Mouse Control Modeは入力の意味を所有する。

## Mouse Control Mode

### Gameplay

通常のマウス入力をDebug Cameraへ渡さない。将来の照準、視点操作、UI以外のゲーム入力に使用する。

CameraウィンドウからDebug Slotを明示選択した場合は、Debug Cameraをマウス操作できる。

### Alt Gesture

既定モード。

Camera Slot 1、2、3を使用中に`Alt + マウス操作`を行うと、現在の構図からDebug Slotへ移行する。Altを押さない通常マウス入力はGameplay用に予約される。

### Always Debug

Camera Slot 1、2、3でマウス操作を開始すると、自動的にDebug Slotへ移行する。デバッグ作業を優先するモードで、将来のGameplayマウス操作とは併用しない。

## シームレスなDebug移行

Debug操作開始時に次を実行する。

1. 現在表示中のCamera Stateを取得する。
2. 現在Slotのメモリキャッシュを更新する。
3. 現在位置、注視点、Projection、FOV、Ortho HeightをDebug Slotへコピーする。
4. Debug SlotのファイルはLoadしない。
5. Follow Tankを解除する。
6. 現在の注視点をOrbit pivotとしてDebug Camera Controllerを初期化する。
7. 同じマウス操作をDebug Cameraへ渡す。

移行時に補間や保存済みDebug CameraのLoadを行わないため、画面位置は不連続に変化しない。

## 入力優先順位

1. ImGuiがマウスをCaptureしている場合はUI操作
2. Debug Slot選択中はDebug Camera操作
3. Alt GestureでAlt押下中はDebug Cameraへ移行
4. Always Debugでは任意のマウス操作でDebug Cameraへ移行
5. それ以外はGameplay用としてDebug Cameraへ渡さない

AltキーはWindowsでは`WM_SYSKEYDOWN/WM_SYSKEYUP`として通知されるため、Alt Gestureの判定は通常キーの押下状態を保持せず、マウスイベント発生時の`GetAsyncKeyState(VK_MENU)`を使用する。

## 操作

- Cameraウィンドウの`Mouse Control`でモードを選択する
- 既定: `Alt Gesture`
- `Alt + 左ドラッグ`: 現在構図からDebug Orbitを開始
- `Alt + 中ドラッグ`: 現在構図からDebug Panを開始
- `Alt + Mouse Wheel`: 現在構図からDebug Zoomを開始
- Debug Slot移行後は、Altを離してもDebug Camera操作を継続できる
- Camera Slot 1、2、3を選択すると保存カメラへ戻る

## 保存方針

自動移行したDebug Cameraはメモリ上の作業状態であり、自動Saveしない。必要な場合だけDebug Slotを選択した状態で`Save Camera`を押す。

Mouse Control Modeは現時点ではセッション設定であり、Camera Slot JSONには含めない。Slotの構図と入力デバイス方針を混在させないためである。

## 将来拡張

- Gameplay CameraでWASD移動と通常マウス視点を実装
- Gameplay用感度、反転、照準モードを別設定として追加
- Debug Cameraへ移行するModifier keyを設定可能にする
- Previous Slotへ戻るコマンドを追加
- Mouse Control Modeをアプリケーション設定として保存
