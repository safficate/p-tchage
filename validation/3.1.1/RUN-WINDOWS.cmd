@echo off
cd /d Z:\workspace\p-tchage
build-win-clang\LocalizationTests_artefacts\Release\LocalizationTests.exe Z:\workspace\pitchmap311-final-evidence\windows-ui > Z:\workspace\pitchmap311-final-evidence\localization-windows.log 2>&1
echo %errorlevel% > Z:\workspace\pitchmap311-final-evidence\localization-windows.code
build-win-clang\PluginTests_artefacts\Release\PluginTests.exe --audio-only > Z:\workspace\pitchmap311-final-evidence\state-windows.log 2>&1
echo %errorlevel% > Z:\workspace\pitchmap311-final-evidence\state-windows.code
build-win-clang\LiveTests_artefacts\Release\LiveTests.exe > Z:\workspace\pitchmap311-final-evidence\live-windows.log 2>&1
echo %errorlevel% > Z:\workspace\pitchmap311-final-evidence\live-windows.code
build-win-clang\WindowsVst3Audio.exe "Z:\workspace\p-tchage\build-win-clang\MicrotonalPitchMapPro_artefacts\Release\VST3\Microtonal Pitch Map Pro.vst3\Contents\x86_64-win\Microtonal Pitch Map Pro.vst3" > Z:\workspace\pitchmap311-final-evidence\actual-vst3-windows.log 2>&1
echo %errorlevel% > Z:\workspace\pitchmap311-final-evidence\actual-vst3-windows.code
build-win-clang\SmootherTests_artefacts\Release\SmootherTests.exe > Z:\workspace\pitchmap311-final-evidence\smoother-windows.log 2>&1
echo %errorlevel% > Z:\workspace\pitchmap311-final-evidence\smoother-windows.code
