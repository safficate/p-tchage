@echo off
"Z:\workspace\p-tchage\build-win-clang\PluginTests_artefacts\Release\PluginTests.exe" --audio-only > "Z:\workspace\pitchmap301-evidence\windows\state.log" 2>&1
echo %errorlevel% > "Z:\workspace\pitchmap301-evidence\windows\state.code"
"Z:\workspace\p-tchage\build-win-clang\LiveTests_artefacts\Release\LiveTests.exe" > "Z:\workspace\pitchmap301-evidence\windows\live.log" 2>&1
echo %errorlevel% > "Z:\workspace\pitchmap301-evidence\windows\live.code"
"Z:\workspace\p-tchage\build-win-clang\ARATests_artefacts\Release\ARATests.exe" > "Z:\workspace\pitchmap301-evidence\windows\ara.log" 2>&1
echo %errorlevel% > "Z:\workspace\pitchmap301-evidence\windows\ara.code"
"Z:\workspace\p-tchage\build-win-clang\WindowsVst3Audio.exe" "Z:\workspace\p-tchage\build-win-clang\MicrotonalPitchMapPro_artefacts\Release\VST3\Microtonal Pitch Map Pro.vst3\Contents\x86_64-win\Microtonal Pitch Map Pro.vst3" > "Z:\workspace\pitchmap301-evidence\windows\actual-vst3.log" 2>&1
echo %errorlevel% > "Z:\workspace\pitchmap301-evidence\windows\actual-vst3.code"
"Z:\workspace\p-tchage\build-win-clang\WorkflowTests_artefacts\Release\WorkflowTests.exe" > "Z:\workspace\pitchmap301-evidence\windows\workflow.log" 2>&1
echo %errorlevel% > "Z:\workspace\pitchmap301-evidence\windows\workflow.code"
