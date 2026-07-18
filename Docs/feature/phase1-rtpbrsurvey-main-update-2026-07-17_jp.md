# Phase 1 RtPbrSurvey main更新

日付: 2026-07-17

ブランチ: `main`

## 概要

Tankの `External/RtPbrSurvey` をRtPbrSurveyの最新 `origin/main` へ更新し、upstreamのCMake renderer targetを使ってTank host appをビルドできることを確認した。

固定したRtPbrSurvey commit:

- `dcd54a3 Fix SceneRenderer CMake host integration`

## Tank側の変更

- `TankSandbox` を `RtPbrSurvey::SceneRenderer` へリンクした。
- `src/main.cpp` のWin32 Tank app entry pointを使用した。
- `External/RtPbrSurvey` を固定commitでクリーンに保った。
- RtPbrSurveyの公開headerに `CD3DX12_*` temporaryのアドレス取得などMSVC拡張依存が残っているため、Tankのコンパイル設定は `/permissive` を維持した。

## 取り込んだupstream修正

`a64338c` の検証時にRtPbrSurveyのCMake統合上の不足を2件発見し、`dcd54a3` でupstream修正された。

1. `TemporalUpscalerSupport.cpp` が参照する `Renderer/StreamlineAdapter.cpp` が、upstreamの `RtPbrSurvey.SceneRenderer` CMake source listに含まれていなかった。
2. `rtpbrsurvey_copy_runtime_files(TankSandbox)` が一部のRtPbrSurvey pathを呼び出し元scopeから解決するため、親となるTankのCMake scopeから安全に呼び出せなかった。

Tank側には、これらの回避策を残していない。現在のTank CMakeでは次を使用する。

```cmake
target_link_libraries(TankSandbox PRIVATE RtPbrSurvey::SceneRenderer)
rtpbrsurvey_copy_runtime_files(TankSandbox)
```

## 検証

ローカルconfigureでは、既存のRtPbrSurvey package cacheを使用した。

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -S . -B build -DRTPBRSURVEY_PACKAGES_DIR=C:/work/RtPbrSurvey-work/packages
```

build command:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Debug --target TankSandbox
```

結果:

- configure成功。
- `TankSandbox` Debug build成功。
- `TankSandbox.exe` を `-LogToFile` 付きで短時間起動した。
- D3D12 InfoQueueを正常に取得した。
- 短時間の起動確認では、D3D12 startup errorは記録されなかった。
- `External/RtPbrSurvey` はクリーンな状態を維持した。

## Upstreamへのフィードバック

不足していた `StreamlineAdapter.cpp` と、親scopeからのruntime copy helper問題をRtPbrSurvey coordination threadへ報告し、`dcd54a3` で修正された。

## 次のステップ

レビュー後、Tank側のsubmodule pointerとCMake整理をcommit、pushする。
