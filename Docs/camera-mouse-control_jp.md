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

Follow TankがONのCamera Slotから移行した場合、Debug Slotでも位置Followは維持する。ただし回転Chaseは解除し、切替時のYaw/Pitchを引き継いだ自由なDebug Orbitとして動作する。戦車が移動すれば位置と注視点は同じ差分だけ移動するが、戦車の背後方向へ回転を戻さない。Camera Slot 1、2、3のメモリとファイルは変更しない。

### Always Debug

Camera Slot 1、2、3でマウス操作を開始すると、自動的にDebug Slotへ移行する。デバッグ作業を優先するモードで、将来のGameplayマウス操作とは併用しない。

## シームレスなDebug移行

Debug操作開始時に次を実行する。

1. 現在表示中のCamera Stateを取得する。
2. Camera 1、2、3のメモリキャッシュは変更しない。
3. 現在位置、注視点、Projection、FOV、Ortho HeightをDebug Slotへコピーする。
4. Debug SlotのファイルはLoadしない。
5. 進行中のCamera Slot補間を現在フレームで終了する。
6. Follow TankがONなら位置追従を維持する。
7. Follow Tankの位置追従だけを維持し、回転Chaseを解除する。
8. 現在の位置と注視点から自由なDebug Orbitを初期化する。
9. Debug Cameraへマウス入力を渡す。

Follow ON中はCameraControllerがCamera Stateを所有する。DebugCameraControllerのOrbit状態からCamera Stateを再生成しない。これにより、以前のDebug Camera Yawが移行直後の水平角を上書きすることを防ぐ。

## 極対応と投影切替

Follow Cameraの位置Yawは常に戦車位置を通るワールドY軸で計算する。Camera Upも前フレームのローカル軸を蓄積せず、ワールドYを視線平面へ射影して各フレームで再生成する。車体のPitchやRollはCamera Upへ反映しない。

Debug Orbitでは戦車Pivotの並進差分だけをCamera Positionと注視点へ反映し、Tank ChaseのYaw更新は行わない。Camera UpはArcball操作後にワールドY基準で再生成する。Look Down上限はRtPbrSurvey Arcballと同じ`1.4 rad`、約80.214度とする。

PerspectiveとOrthographicの切替では、FOVとOrtho Heightを独立に補間しない。注視距離における表示高さへ変換して補間し、各フレームでPerspective FOVとOrtho Heightの両方へ戻す。

- OrthographicからPerspectiveへ移る場合は、Transition開始時にPerspectiveへ切り替える。
- PerspectiveからOrthographicへ移る場合は、Transition中はPerspectiveを維持し、Transition完了時にOrthographicへ切り替える。

これによりTransition途中で投影方式が突然切り替わることを避けつつ、注視面の表示サイズを連続的に変化させる。

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
- Debug Orbitの左ドラッグ左右: ワールドY軸を中心に水平回転
- Debug Orbitの左ドラッグ上下: 視線とワールドYから求めた右軸を中心に仰角を変更
- `Alt + Ctrl`専用操作は使用しない
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
