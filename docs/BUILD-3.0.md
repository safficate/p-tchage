# Reproducing 3.0.1 engineering preview

## Windows native

Install Visual Studio 2022 Desktop development with C++, Windows SDK, CMake >=3.24, Ninja or MSBuild, and Git. Run `powershell -ExecutionPolicy Bypass -File scripts/build-windows.ps1` from the corresponding-source package. Pinned vendor directories are used automatically. Run tests only after the build has finished. The CMake default enables ARA; `-DMP_ENABLE_ARA=OFF` explicitly builds a regular FX variant.

Outputs: `build/MicrotonalPitchMapPro_artefacts/Release/VST3/Microtonal Pitch Map Pro.vst3` and `.../Standalone/Microtonal Pitch Map Pro.exe`. Copy the whole VST3 bundle. Source package includes JUCE, Signalsmith Stretch/Linear and ARA SDK API/Library; unused ARA Examples and repository histories are omitted. C++20 and a 64-bit target are required.

Run the regional test executable with an optional output directory to recreate the 24 instrument-inspired synthetic cases and their input/output/AB WAV, plan JSON and analysis CSV: `RegionalTests.exe C:\pitchmap-corpus`. The suite also exports +/-3/5/20-cent calibration fixtures at both rates (36 cases total). Exit status must be zero. These are synthetic stress models, not licensed real performances.

For the real DLL harness, use the WindowsVst3Audio CMake target with the built DLL path. CTest exercises static engine/processor/ARA integration; DLL factory validation is separate. WorkflowTests exercises analysis-only proposals, original preview, explicit clip Apply, actual changed PCM, target audition, cursor anchoring and state migration. RegionalTests also checks left-only/right-only ±5-cent rendering at both rates. See Tests/WindowsVst3Audio.cpp and its console usage. A successful build or Wine run does not prove host compatibility. Perform docs/SONAR-TEST.md in the current stable Sonar on the intended laptop.

## Cloud toolchain used here

Retained rootless tooling is outside the repository at `/workspace/.cloud-tools`. Native: source `activate-dev.sh`, configure `build-linux` using the pinned vendor sources and `MP_JUCE_HOST_TOOL`, build with `--parallel 2`, then run CTest with `DISPLAY=:99 FONTCONFIG_FILE=/workspace/.cloud-tools/fonts.conf`. Start Xvfb if needed and check display readiness before GUI tests. Windows: source `windows-build-env.sh` and use the existing `build-win-clang` cache; this cross-compiles with clang 19, Windows SDK 10.0.26100 and static MSVC 14.44 runtime. Caches and machine-specific toolchains are not required by the native Windows build.

Wine 10 is executed through `/workspace/.cloud-tools/run-windows.sh`. Its outer exit status is not reliable for every application: each .cmd writes `%errorlevel%` immediately after the tested command into a fresh .code file. The delivered logs retain these files. Installation uses NSIS 3.11, AMD64 target, followed by actual installed DLL load/audio tests and uninstall. Native Windows desktop tests remain unrun.

## Dependency pins

| Component | Commit |
|---|---|
| JUCE 8.0.12 | 29396c22c93392d6738e021b83196283d6e4d850 |
| Signalsmith Stretch | a670068d9aeb64913331d5cc29337b19a457a7df |
| Signalsmith Linear | 146b26f91b48835d665fa0498d64d9617818d27a |
| ARA SDK 2.2.0 | e723ce48a917b1ce2d7851c78e468f36b51ea151 |
| ARA API | d8a3d608e5243255aea40b1d40654d3b2c66af71 |
| ARA Library | ac1beaa7a3b58c8b970628c40c8fe823f8bec573 |

No new runtime dependency was added. Licensing notes are in LICENSING-3.0.md. The saved cloud-onboarding draft points to these retained tools, requires no new package purchases and stores no credentials. It is a draft configuration, not a deployed environment change.

Optional metric/figure reproduction uses Python with NumPy, SciPy and Matplotlib: `python scripts/report-regional-corpus.py CORPUS OUTPUT`. These are development/report tools, not plugin runtime dependencies. It measures the emitted WAV files independently, exports CSV/JSON, and writes standalone PDF/PNG figures.
