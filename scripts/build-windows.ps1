param()
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$configure = @('-S', '.', '-B', 'build', '-A', 'x64')
if (Test-Path 'vendor/JUCE/CMakeLists.txt') {
    $configure += "-DJUCE_DIR=$root/vendor/JUCE"
    $configure += "-DFETCHCONTENT_SOURCE_DIR_STRETCH=$root/vendor/signalsmith-stretch"
    $configure += "-DFETCHCONTENT_SOURCE_DIR_LINEAR=$root/vendor/signalsmith-linear"
}
& cmake @configure
if ($LASTEXITCODE -ne 0) { throw 'Configure failed. Install Visual Studio 2022 Desktop C++ and Windows SDK.' }
& cmake --build build --config Release --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
& ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
Write-Output 'Build and tests passed. VST3: build/MicrotonalPitchMapPro_artefacts/Release/VST3/Microtonal Pitch Map Pro.vst3'
Write-Output 'This does not establish Cakewalk Sonar compatibility; run docs/SONAR-TEST.md in Sonar.'
