# Validation report — Microtonal Pitch Map Pro 0.2.0

Updated 2026-10-19 UTC. **Cakewalk Sonar and the target HP EliteBook have not been tested.** Windows execution below uses Wine 10 on a Linux cloud host; it is not native Windows/Sonar certification.

## Artifacts and environment

The delivery contains an actual PE32+ x86-64 VST3 DLL bundle, Windows standalone and AMD64 NSIS 3.11 installer. It is not a browser wrapper. Clang 19.1.7 targets the MSVC ABI, Windows SDK 10.0.26100 and MSVC CRT 14.44, statically linked. SHA-256 hashes are supplied with the package.

Pinned dependencies: JUCE 8.0.12 `29396c22c93392d6738e021b83196283d6e4d850`, Signalsmith Stretch `a670068d9aeb64913331d5cc29337b19a457a7df`, Signalsmith Linear `146b26f91b48835d665fa0498d64d9617818d27a`, ARA SDK 2.2.0 `e723ce48a917b1ce2d7851c78e468f36b51ea151` (API `d8a3d608e5243255aea40b1d40654d3b2c66af71`, Library `ac1beaa7a3b58c8b970628c40c8fe823f8bec573`). Source and applicable licenses accompany the release.

## Completed automated checks

Native Linux Release CTest: **5/5 passed**, 0 failed, in the final recorded run: pitch engine, processor state/playback, actual editor construction/paint, ARA clip transfer, live audio. After correcting the synthetic host's archive restore editing bracket, the ARA test was rebuilt and rerun successfully. Evidence: `validation/linux-ctest.txt`, `linux-LastTest.log`, `linux-ara-retest.txt`, `linux-ara-LastTest.log`, `editor-linux.png`.

Windows executable results under Wine are in `windows-validation/`; each exit code is captured **inside the Windows command interpreter**, since the outer compatibility runner can mask a failed Windows exit code.

- **Engine: 82 assertions passed**, both native and Windows. Includes 44.1/48 kHz offline correction, +/- targets, low five-string bass B0 detection, fine targets -5/+3/+5/+10 c in both absolute-reference and relative-shift modes, no percentage dependency, original/zero preservation, vibrato motion, stereo equality/polarity/general stereo phase, WAV/MIDI and source/map/settings state.
- **Live: 242 assertions native; 248 Windows including six output WAV writes.** Real processor callbacks with variable blocks (including 1/31/64/128/256/257 samples), no loaded/captured clip, no playhead and no message pump. Checks legacy 0/50/100% paths, relative -20/-5/-3/+3/+5/+10/+37.5 c, C to C sharp/D, exact destination reference tuning, A/B/host bypass/wet zero, state reload, equal and inverted stereo, no C++ heap allocation in callbacks, optional sidechain modes (including a low bass reference), unchanged reference PCM and preservation of stereo tails when incoming audio becomes silent. Upper keys produce tuned reference frequencies; lower keys play genuine source PCM; missing source notes are rejected. `audio-examples/` contains the Windows callback output at both rates, latency aligned, for the 0/50/100% legacy cases.
- **ARA: 44 assertions passed**, native and Windows. A synthetic host uses the real ARA C API and plugin document/playback implementation. Source PCM and notes are obtained **before the first processBlock or transport playback**. Reader creation/destruction is checked against ARA source-management/endEditing restrictions; playback never calls host sample IO. Checks source trim, timeline position, zero reported ARA latency, real corrected bounce with silent track input, parameter state, preservation of manual edits across host clip refresh, and ARA document archive restoration without separate component state. This validates the implemented protocol path against the fixture, not against Sonar.
- **Processor state/playback: passed native and Windows.** Source PCM, mappings and parameters reload; rendered notes reach the callback at 44.1/48 kHz, including project/source rate mismatch; bypass and timeline boundaries are checked. Cached playback advances a dry delay only, avoiding repeated live pitch analysis and spectral processing. A limited 2,000-block/256-frame cached benchmark took 12.895 ms native and 19.8628 ms under Wine with a fixed playhead; these are cloud synthetic timings, not target laptop CPU/dropout results.
- **Actual Windows DLL:** `WindowsVst3Audio.exe` loads the delivered VST3 with LoadLibrary, instantiates effect/controller through the Steinberg factory and processes real input via IAudioProcessor, with no source file, editor, transport context or GUI pump. Results for the complete target/shift/reference/sidechain matrix and offline mode are recorded in `windows-validation/actual-vst3.log` and `.code`. The test uses the VST3 parameter text parser for engineering units such as cents/signed semitones; JUCE's plain floating host units are normalized. Output is measured independently using interpolated zero crossings.

## Representative measurements

Source A4 is +10 c. The native callback results for a target of -20 c were:

| Rate | Original | Half legacy strength (expected -5 c) | Full target (expected -20 c) |
| --- | ---: | ---: | ---: |
| 44.1 kHz | +10.052 c | -4.980 c | -19.883 c |
| 48 kHz | +10.026 c | -5.049 c | -20.011 c |

Basic reference matching ignores strength percentages. Source C +10 c mapped to a C sharp reference +5 c measured +104.992/+105.104 c relative to C; mapped to D +5 c measured +205.009/+204.970 c at 44.1/48 kHz. ARA A +10 c to B +5 c measured +205.300/+204.932 c relative to A. Sidechain cent matching A4 to A3 -5 c measured -4.986/-5.049 c; following G3 +7 c while preserving source octave measured -192.989/-192.997 c relative to A4.

All fine-target assertions use <1 c tolerance on the specified stable synthetic tones. Short-window YIN and zero-crossing measurements have finite error. These results do **not** establish universal sub-cent accuracy or natural timbre on arbitrary musical material. Natural instrument/listening tests are still required.

## Runtime behavior and remaining host checks

Normal live FX reports 6,615 samples at 44.1 kHz or 7,200 at 48 kHz: 150 ms, to be compensated by the host. Prepared ARA clip playback reports zero latency. Clip analysis/rendering runs on the worker; host clip PCM import runs on a non-audio control thread. Playback accesses preloaded memory. Maximum per instance is 600 seconds / 240 MiB PCM. Full PCM in project/archive state increases file size and memory use.

Native Linux editor software rendering passed. Native Windows UI, actual audio device operation and Sonar UI integration are **unverified**. Previous Wine GUI snapshot attempts on 0.1.0 stalled; no completed Windows GUI snapshot is claimed for this release. The Windows audio/state/ARA tests avoid GUI painting.

The installer execution, installed-module load and removal are recorded separately in `windows-validation/install*.log`, `installed-vst3.log`, `uninstall.log` and corresponding `.code` files. Inspect those results alongside the final summary JSON.

Still required on the latest stable Windows Sonar at the actual test date: VST3 scan, normal FX insertion, Region FX/ARA discovery and clip sharing, host sidechain routing, playback/seek/loop, realtime/offline bounce, project close/reopen and target HP EliteBook/internal audio performance. See `SONAR-TEST.md`. ARA editor-only roles, clip duplication workflows and host-specific project routing have not been validated in Sonar.

Monophonic fundamentals only: no polyphonic source separation, automatic project-wide listening, ONNX model or shared multi-stem editor is implemented. A normal track FX cannot fetch all project clips; ARA must be provided by the host, or users must load/capture source audio. Time-stretched ARA clips must be bounced first. Scala is restricted to supported 12-step octave scales. No unrun Sonar or hardware test is marked as passed.
