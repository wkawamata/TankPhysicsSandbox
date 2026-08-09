# Tank Physics Sandbox

[English](README.md)

Tank Physics Sandbox は、リアルで説得力のある戦車車両挙動を開発するための C++20 物理サンドボックスです。

ゲーム性より先に、車両物理の基礎を作ることを重視します。将来自作 DirectX 12 エンジンや別ホストへ移植できるように、物理と描画は分離します。

## 目標

- Jolt Physics を使って戦車サンドボックスを作る。
- 物理を描画から独立させる。
- `TankController` を車両設計の中心にする。
- JSON でチューニングできるようにする。
- `RtPbrSurvey` を任意の DirectX 12 テスト描画システムとして使う。

## 現在の状態

- 最小 CMake アプリケーション shell がある。
- `RtPbrSurvey` は `External/RtPbrSurvey` に Git submodule として追加済み。
- Jolt 車両作業へ入る前に、renderer host integration を検討中。

## ビルド

submodule を取得した後、プロジェクト付属のスクリプトで configure/build します。

```powershell
git submodule update --init --recursive
.\scripts\Restore-GameInput.ps1
.\scripts\configure.bat
.\scripts\build.bat TankSandbox
```

`configure.bat` は Visual Studio 付属の CMake と
`C:\dev\vcpkg` の toolchain を使用し、古い CMake cache を更新して
`build` ディレクトリを生成します。

RtPbrSurvey の NuGet packages が既定の
`C:\work\RtPbrSurvey-work\packages` 以外にある場合は、最初の引数で指定します。

```powershell
.\scripts\configure.bat C:/path/to/RtPbrSurvey/packages
```

生成される solution は `build\TankPhysicsSandbox.sln` です。
Visual Studio で solution を開いた後、ソリューション エクスプローラーの
`TankSandbox` を右クリックし、**スタートアップ プロジェクトに設定**を選択してください。

## 起動

```powershell
.\scripts\run.bat
```

このスクリプトは作業ディレクトリをリポジトリルートへ固定し、
`Config/renderer_debug.json` などのホスト設定を確実に読み込みます。追加のコマンドライン引数も
そのまま渡します。実行ファイルの直接起動もサポートしますが、その場合は呼び出し側が
目的に合う作業ディレクトリを指定してください。

## Gamepad 入力

Windows のGamepad入力にはMicrosoft GameInputを使用します。左stickで前後移動、
通常旋回、pivot turnを行います。標準mapping対応controllerではAボタン、
raw-controller fallbackではbutton 3をbrakeとして使用します。キーボード操作も
引き続き利用できます。

`Tracked Vehicle` debug windowには、接続したデバイス名、button数、axis数、
switch数、左stickの現在値が表示されます。configureの前に
`scripts\Restore-GameInput.ps1` を実行し、Windows入力層が使用するGameInput SDKを
取得してください。

## ドキュメント

- [プロジェクトとエージェント向けルール](AGENTS.md)
- [機能別ドキュメント](Docs/feature/)
