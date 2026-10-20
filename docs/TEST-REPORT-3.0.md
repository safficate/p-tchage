# PitchMap Pro 3.0 engineering preview — actual test report

2026-10-20 UTC. Development branch: `development/pitchmap-pro-3.0`. Started from `c2caa39ce3fb0604732db11e3a9de3b317a66729`. **Sonar, native Windows desktop, the specified HP laptop and real instrument listening were not tested.** Wine and synthetic-host tests do not certify them. No GitHub Release is created; original 0.2.0 packages are unchanged.

## Completed checks

| Check | Actual result |
|---|---|
| Baseline 0.2.0 | Rebuilt; Linux 5/5 groups, original Windows DLL 29 audio cases rerun under Wine |
| Final Linux CTest | 6/6 groups; 52.26 s in shared cloud environment |
| Regional emitted-audio suite | 13,040 assertions on Linux and Windows PE/Wine |
| Exported corpus | 13,148 assertions; 36 cases with source/output/AB WAV, source/output analysis CSV and plan JSON |
| Legacy engine | 82 assertions on emitted PCM/state behavior |
| Live callback | 242 assertions, actual streamed audio, reference isolation and no file/transport/message-loop requirement |
| ARA synthetic host | 44 transfer/trim/bounce/state assertions |
| Actual final Windows VST3 DLL | Factory load and 29 audio cases at 44.1/48 kHz, realtime and host-offline modes; max fixture error 0.0685 c |
| Installed DLL | Same 29 cases after silent installation; copied installed DLL SHA-256 matches packaged DLL |
| Uninstall | Plugin and application directories removed; NSIS launcher exit 0 and completed filesystem cleanup observed |
| Editor | Native Linux construction, software paint, basic percentage hiding and explicit legacy-to-adaptive control transition |
| State/cancellation | v2 keeps legacy DSP; v3 controls/PCM/centres/onset/uncertainty survive; cancelled analysis retains prior map; adaptive -5 c reaches host callback |

Assertion counts largely include sample-identity checks; they are not thousands of independent recordings. The actual DLL cases exercise channel processing/bounce, while adaptive clip rendering is separately exercised through processor and ARA integration fixtures. The DLL GUI was not validated on Windows.

All Windows commands record genuine Windows `%errorlevel%` rather than relying on the Wine wrapper. NSIS normal uninstall uses a temporary child: the report records its launcher exit plus completed directory cleanup, not a separately captured child exit. Installation hashes and scope are in `windows/INSTALLATION-SUMMARY.json`.

## Fine-cent fidelity on emitted audio

Stable harmonic A4 fixtures, requested +/-3/5/20 c at both project rates. The acceptance gate is **each independent measurement within 1 c**, not merely a planned parameter. Periodicity is measured from rendered PCM; cycle crossings use a Schmitt-style crossing counter; the separate Python spectral estimate fits the known fundamental from the exported long-window WAV. No strongest FFT-bin note identification is used.

| Rate | Requested | Periodicity | Cycle crossings | Independent spectral |
|---|---:|---:|---:|---:|
| 44100 | -20 c | -19.756 c | -20.149 c | -20.067 c |
| 44100 | -3 c | -3.330 c | -2.646 c | -2.547 c |
| 44100 | -5 c | -5.596 c | -4.309 c | -4.299 c |
| 44100 | +20 c | +19.312 c | +20.509 c | +20.547 c |
| 44100 | +3 c | +2.915 c | +2.691 c | +2.746 c |
| 44100 | +5 c | +5.292 c | +4.924 c | +4.936 c |
| 48000 | -20 c | -20.022 c | -19.972 c | -20.183 c |
| 48000 | -3 c | -3.369 c | -2.541 c | -2.400 c |
| 48000 | -5 c | -5.688 c | -4.078 c | -4.074 c |
| 48000 | +20 c | +19.442 c | +20.798 c | +20.938 c |
| 48000 | +3 c | +3.131 c | +2.702 c | +2.907 c |
| 48000 | +5 c | +5.216 c | +4.936 c | +4.794 c |

Maximum independent spectral error: **0.9379 c**. Both C++ measurements also passed their separate one-cent gates. Calibration sometimes balances slightly different periodic and spectral estimates; raw values remain visible. This is a controlled stable-signal result, not guaranteed one-cent accuracy on every performance or claimed universal sub-cent quality.

## Expression, transitions, attacks and stereo

Vibrato acceptance: correlation >=0.97, residual contour RMS <=3 c and centre displacement within 1 c of requested -10 c.

| Runtime | Rate | Achieved / requested -10 c | Contour correlation | Residual RMS |
|---|---|---:|---:|---:|
| Linux | 44100 | -9.108 c | 0.991774 | 1.387 c |
| Linux | 48000 | -9.385 c | 0.991216 | 1.524 c |
| Windows PE / Wine | 44100 | -9.108 c | 0.991775 | 1.387 c |
| Windows PE / Wine | 48000 | -9.385 c | 0.991216 | 1.524 c |


The connected-legato model reaches -20/+15 c plateaus within 1 c. Maximum sampled C(t) change across all corpus cases is 0.534020 c per 1 ms (gate <=1.01). An impossible short transition reports reduced correction; hard-zero decisions are preserved. Identified two-event and three-event overlap conflicts decline the whole group; compatible overlap has one shared displacement. Local projection has a final feasibility fallback, and calibration trims cannot exceed the connected-transition bound.

The first 15 ms after each independently known synthetic pick at 0, 0.8 and 1.6 s remains sample-identical in both channels/rates, including the exported WAV check (maximum absolute difference 0). This catches delayed stable-F0 boundaries rather than validating only self-detected intervals. Short-window onsets anchor a 40 ms dry interval followed by a 40 ms bridge. The dry attack is initially unshifted and the bridge can color phase/timbre; real listening remains required.

Already-tuned synthetic material is bit-exact at both rates. All 36 exports preserve duration, are finite and peak at or below 0.437907. Ensemble side-energy width remains present: approximately 0.2518 before and 0.2535/0.2537 after; stereo correlation is about 0.8733/0.8731. These are measured relationships, not proof of universal unchanged stereo phase. Identical-channel behavior is exercised; a separate adaptive inverted-channel fixture was not added.

Missing-fundamental and changing-dominant-harmonic models have zero wrong-octave frames among the reliable interior frames at both rates. Even-only ambiguity, independent polyphony and arbitrary instrument mixtures remain unsolved. The guitar overlap corpus applies common displacement; it does not demonstrate complete automatic tail detection or independent note repair.

## Measurements that did not establish precision

The following processed regions lack a confirmed output-centre measurement within selected precision: **44100-guitar-overlap (2 regions), 44100-ney-slide-noise (1 regions), 48000-guitar-overlap (7 regions), 48000-ney-slide-noise (1 regions)**. The plan JSON marks them unconfirmed and the UI summary requests review. These are not tuning-accuracy passes. No automatic rollback based on a calibrated audible-artifact model is implemented. Transient location, naturalness, weak/harmonic tails and ensemble centre reliability require licensed real recordings and human listening.

## Performance evidence and its limits

A Linux process generated/analyzed/rendered/exported all 36 three-second cases, including extra output reanalysis, in 32.57 s wall / 30.46 s user CPU, with 25524 KiB maximum RSS. These are short synthetic cases on a shared cloud host, not the 16 GB laptop, maximum-length clip memory or DAW dropout measurements. Cached callback timings are retained verbatim in `linux-LastTest.log`; they are not advertised as a laptop CPU percentage.

Expensive new work remains on the worker, with cancellation and immutable render publication. Live input retains the lower-cost existing engine and 150 ms latency; cached ARA playback reports zero. Maximum source cap remains 10 minutes / 240 MiB, and embedded PCM enlarges project files. Target-machine CPU/RAM/driver/dropout and latest Sonar scan/open/playback/bounce/reload gates remain unrun.

## Unfinished features / next gates

Persistent sidechain reference-map capture/learning is unfinished; existing live following still requires the routed reference playing. AI language control, independent chord/player correction, neural separation, trained instrument classification and advanced piano processing are design/research only. No fake controls are added. pYIN/neural runtime comparisons were not performed; consulted implementations, blocked primary papers and licensing are distinguished in the research documents. A force-process low-confidence override, cross-reanalysis hysteresis and calibrated perceptual optimization are unfinished. Direct automation of conflicting hidden legacy parameters is outside the supported adaptive workflow.

Next: licensed real performances and blind listening, current stable native Sonar on the specified Windows 11/internal-audio laptop, stronger onset/tail inference, persistent reference learning, additional detector benchmarks, and a more useful global constrained planner. See `SONAR-TEST.md` and `IMPLEMENTATION-ROADMAP.md`.

## Evidence and reproduction

[Machine-readable results](../validation/3.0.0-preview/TEST-RESULTS.json), [Linux log](../validation/3.0.0-preview/linux-LastTest.log), [Windows logs](../validation/3.0.0-preview/windows), [metrics CSV](../validation/3.0.0-preview/metrics/audio-metrics.csv), [pitch-contour PDF](../validation/3.0.0-preview/metrics/pitch-contours.pdf), [fine-cent PDF](../validation/3.0.0-preview/metrics/fine-cent-errors.pdf), [attack PDF](../validation/3.0.0-preview/metrics/attack-protection.pdf). Original and corrected WAVs are separate rate-specific audio-example ZIPs. Recreate them with RegionalTests and `scripts/report-regional-corpus.py`; build steps/pins are in `BUILD-3.0.md`. The binary ZIP manifest identifies the corresponding-source commit and SHA-256 hashes.
