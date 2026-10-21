# PitchMap Pro 3.0.1 — workflow and emitted-audio test report

2026-10-21 UTC. Development branch `development/pitchmap-pro-3.0`. **Native Windows, current Sonar and the specified HP laptop were not tested.** Windows PE executables and the actual x64 VST3 DLL were executed under Wine 10. This does not certify Sonar compatibility. No GitHub Release was published; 0.2.0 and 3.0.0 delivery packages are unchanged.

## Concrete changes

The primary selector separates an absolute reference target from an amount added to the source. The source measurement and user-selected target determine the cent difference; the default target is explicitly A440 / 12-tone equal temperament, not an inferred makam. Relative mode adds a global cent amount and per-source cent/semitone amounts. An individually detached note retains its explicit override.

Analysis produces a proposal with its basis, source centre, target, requested difference, effective plan, uncertainty and reason for reductions/refusals. Apply renders the clip; subsequent target edits remain pending until Apply. There is no additional confirmation dialog. Enabled live-input/sidechain processing remains continuous; restoring an enabled project regenerates its saved render offline.

The result panel retains measured rendered-output metadata instead of treating a planned curve as a verified result. Missing/out-of-precision measurements are flagged. The original clip can be auditioned before Apply, including local audition while bypassed. Relative target keys audition the target calculated from the analysed source centre. Export uses the last applied immutable PCM with A/B/Wet settings, rather than silently rendering pending settings through the legacy engine.

Imported WAVs use the last timeline cursor supplied by the DAW; the start is visible and editable. With no host position the value is zero and the status asks for adjustment. ARA clips retain host positioning and ignore the separate WAV offset; the ARA bounce/state fixtures explicitly leave a stale +30 s WAV offset set. Fine-cent verification/calibration now uses the audible high-energy source channel instead of always channel zero, fixing right-only stereo undercorrection.

## Completed checks

| Check | Actual result |
|---|---|
| Native Linux CTest | 6/6, 30.46 s; engine, state/playback, editor, workflow, ARA, live |
| Native regional suite | Run once through full corpus export: 13,188 assertions, 36 exported cases; the redundant CTest regional entry was excluded |
| Windows PE regional suite / Wine | 13,080 assertions, including 8 left-only/right-only regressions |
| Windows PE workflow / Wine | 41 assertions: proposal-only analysis, original audition, Apply, real PCM, pending edits, export, target tone, WAV positioning, state and Apply-to-unanalysed-clip chain |
| Legacy engine / live / ARA | 82 / 248 / 44 Windows PE assertions, all exit 0 |
| Actual Windows VST3 DLL | 33 load/audio cases; 44.1/48 kHz, realtime/offline, new global-cent parameter and hidden-relative-offset isolation |
| Installation | NSIS install, installed DLL hash equality and same 33 real-DLL audio cases; uninstall exit plus completed directory cleanup |
| Actual editor rendering | Native Linux software painting at 1060×680 and 1240×820; screenshots retained; Windows GUI not tested |

All Windows command exit codes were captured immediately as genuine `%errorlevel%` in `.code` files. NSIS uninstall launches a temporary child: evidence records the launcher exit and completed cleanup, not a separately observed child exit. See `windows/INSTALLATION-SUMMARY.json`. Assertion counts include repeated sample-identity checks; they are not thousands of recordings.

## Actual sound, not parameter echoes

A harmonic source measured approximately A4 +10 c. The processor's DAW callback produced:

| Mode | Entered settings | Desired output relative to A440 | Measured output |
|---|---|---:|---:|
| Absolute target | A +5 c | +5 c | +4.35684 c |
| Relative addition | Global +5 c and source A +2 c | +17 c | +17.3337 c |

These are measured output frequencies. They meet the controlled one-cent gate; they are not exact mathematical identity. Changing the absolute target to −5 c and applying again changed actual output accordingly. Until that second Apply, playback and WAV export retained the earlier applied sound. Analysis alone did not enable correction or publish corrected PCM. The 30 s WAV anchor was exercised through a simulated DAW playhead and actual callback output. The offset parameter's floating-point value was 30.000001907 s; the test uses the parameter's 1 ms entry resolution rather than exact floating-point equality.

Left/right regression measurements below use an independent cycle-crossing estimator on emitted audio, not the engine's detector:

| Rate | Audible channel | Requested shift | Independent measured shift |
|---|---|---:|---:|
| 44100 | left | -5 c | -4.33033 c |
| 44100 | left | 5 c | 4.91215 c |
| 44100 | right | -5 c | -4.33033 c |
| 44100 | right | 5 c | 4.91215 c |
| 48000 | left | -5 c | -4.08201 c |
| 48000 | left | 5 c | 4.93668 c |
| 48000 | right | -5 c | -4.08201 c |
| 48000 | right | 5 c | 4.93668 c |

Each case passed the one-cent gate, engine verification and silent-channel preservation. The 36-case synthetic corpus additionally covers ±3/5/20 c, vibrato, connected transitions, attacks and selected overlap/harmonic stress models. Independent spectral maximum fine-cent error: **0.9379 c**. Known first-15-ms pick attack sample difference: **0**. Source/output/AB WAV, per-region plans and analysis CSV are supplied. DSP behavior outside the channel-verification fix and new relative control remains the 3.0 baseline; see TEST-REPORT-3.0.md for its detailed limits.

## Limits and performance scope

This is a heuristic common-mode shifter, not independent polyphonic tuning. Persistent sidechain learning, AI assistance, trained instrument models, neural separation and a piano-specific model remain unfinished. Confidence is an engineering estimate; unreliable centres and conflicting tails can be declined, and short transitions can reduce corrections. A curve, an entered value or a successful render alone does not establish cent fidelity or natural sound.

No real-performance listening panel or current Sonar scan/open/playback/bounce/reload test was run. The synthetic one-cent gate is not a guarantee on every recording. Consult SONAR-TEST.md for the outstanding native acceptance work. Native Windows GUI, HP laptop CPU/RAM/internal-driver/dropout and long-project performance remain unverified.

The native corpus process took 91.52 s wall / 54.31 s user CPU, max RSS 25184 KiB, while other compilation work shared the cloud host. This is not a target-laptop benchmark. Offline analysis/render/export remain off the audio callback; live allocation checks passed. Live latency remains 150 ms, ARA cached latency zero. State v4 stores global relative cents; v3 restores adaptive semantics with a zero new global amount, and v1/v2 migration remains covered.

## Evidence

[Machine-readable results](../validation/3.0.1-preview/TEST-RESULTS.json), [Linux test log](../validation/3.0.1-preview/linux-LastTest.log), [Windows logs](../validation/3.0.1-preview/windows), [workflow screenshots](../validation/3.0.1-preview/gui), [independent metrics](../validation/3.0.1-preview/metrics). The binary ZIP manifest records the corresponding-source commit and binary SHA-256 hashes. Reproduction: BUILD-3.0.md and Tests/WorkflowTests.cpp.
