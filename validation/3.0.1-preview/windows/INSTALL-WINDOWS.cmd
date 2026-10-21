@echo off
"Z:\workspace\p-tchage\dist\windows-x64-3.0.1-preview\MicrotonalPitchMapPro-Setup-x64.exe" /S
echo %errorlevel% > "Z:\workspace\pitchmap301-evidence\windows\install.code"
copy /y /b "C:\Program Files\Common Files\VST3\Microtonal Pitch Map Pro.vst3\Contents\x86_64-win\Microtonal Pitch Map Pro.vst3" "Z:\workspace\pitchmap301-evidence\windows\installed-payload.vst3" > "Z:\workspace\pitchmap301-evidence\windows\installed-copy.log" 2>&1
echo %errorlevel% > "Z:\workspace\pitchmap301-evidence\windows\installed-copy.code"
"Z:\workspace\p-tchage\build-win-clang\WindowsVst3Audio.exe" "C:\Program Files\Common Files\VST3\Microtonal Pitch Map Pro.vst3\Contents\x86_64-win\Microtonal Pitch Map Pro.vst3" > "Z:\workspace\pitchmap301-evidence\windows\installed-vst3.log" 2>&1
echo %errorlevel% > "Z:\workspace\pitchmap301-evidence\windows\installed-vst3.code"
"C:\Program Files\Microtonal Pitch Map Pro\Uninstall.exe" /S
echo %errorlevel% > "Z:\workspace\pitchmap301-evidence\windows\uninstall.code"
