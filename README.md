# PitchMap Pro 3.0.1 — engineering preview

Windows x64 VST3 and standalone, based on the existing JUCE/Signalsmith project. This development build adds **offline adaptive regional pitch translation**: it moves a reliable tuning centre while preserving the original pitch movement. It is an engineering preview, not a commercial audio-quality or Sonar compatibility certification. The original 0.2.0 delivery is retained unchanged.

## Use it

1. Load a WAV or open host clips through supported ARA Region FX. An ordinary insert cannot retrieve unrelated project clips. For imported WAV, the visible **WAV baslangici** is set from the last cursor position supplied by the DAW; check it against the actual clip start. Without a host position it starts at zero. ARA clips retain their host timeline positions.
2. Choose the purpose on the main screen: **Hedef / makam notalarina akortla** (absolute tuning) or **Kaynaga cent / nota ekle** (relative shift). Keep **Dogal bolgesel (klip/offline)** enabled. **ANALIZ ET / AYAR BELIRLE** measures source regions and explains the proposal; it does not change the audio. **DINLE / A-B** can audition the whole original clip before applying anything.
3. In absolute mode, the lower keys and cent fields define the target tuning. The default is 12-tone equal temperament at A440, not a guessed makam. Source A +10 c and target A +5 c propose **−5 c**. To map C to D, choose D in C's destination selector, or select upper C and lower D then **ESLESTIR**. Percentages are unnecessary.
4. In relative mode, **Kaydin tamamina eklenecek** adds the entered cents to all source notes. Per-source cent and semitone settings add to this amount: source A +10 c, global +5 c and A +2 c produce a requested A +17 c. The lower A key auditions that calculated target from the analysed source. Upper keys always audition the original source segments.
5. The proposal panel shows the basis, source centre, your target, requested difference, planned amount and reason for any reduction or refusal. Press **ONERIYI UYGULA** to render. The same panel then shows actual output measurements and deviation where measurement is reliable. A blue curve alone is a planned target, not proof of success.
6. Compare the rendered sound with **DINLE / A-B**, then bounce in Sonar or export WAV. WAV export uses the last applied rendered PCM and the A/B/Wet settings; it does not silently render pending targets through a different engine. Subsequent target changes remain a new proposal until Apply; the previous rendered clip remains audible. This explicit clip workflow differs from live input and sidechain modes, where enabled parameters operate continuously. Restoring an enabled project regenerates its saved clip render offline.

Advanced exposes precision and minimum intervention. Below the selected precision/estimated uncertainty the engine can retain original PCM. Low-confidence centres and identified conflicting overlapping tails are declined. Short connected transitions can require reduced corrections. These decisions are explicit; a requested correction is not a guarantee on every recording.

## Implemented in this delivery

- Multi-window offline YIN analysis, harmonic checks, weighted robust regional centres and heuristic uncertainty.
- Original `P(t)` and separately planned `C(t)`, constant regional offsets, transitions between stable anchors, bounded corrections when transition time is insufficient.
- Automatic continuous/discrete behavior hypotheses; dry attack protection with an explicit processed-audio bridge; no automatic flattening of vibrato.
- Minimum intervention, linked stereo, actual rendered-output verification and bounded stable-centre refinement. Every measurement is from emitted PCM, not just a parameter value.
- Source/target row order clarified, background cancellation, optional analysis metadata saved with host state, state v4 includes global relative cents; v1/v2 legacy and v3 adaptive semantics retained.
- Existing custom cents, 12-note Scala import, note-level edits, undo/redo, ARA clip access, WAV import/export, standalone, and live reference sidechain.

## Limits that matter

Persistent **sidechain reference-map learning is unfinished**. Existing sidechain modes follow a simultaneously playing reference; they do not retain a learned map. Only routed audio reaches the plugin. The adaptive planner operates on offline clips, while live input uses the existing lower-cost streaming engine.

This is one common-mode shifter. It cannot separately tune overlapping notes, players or chord tones. Tail detection is heuristic and can miss an overlap. Uncertainty is an engineering estimate, not a calibrated probability. Attacks kept dry are initially unshifted; the following bridge can color the sound. Piano-specific modeling, neural separation, trained instrument inference and the AI assistant are research/design only, with no placeholder buttons. Real-instrument listening and native Windows/current Sonar/HP laptop validation remain outstanding.

44.1/48 kHz mono/stereo are supported by the audio tests. No MIDI or external audio interface is required; the DAW supplies its selected driver. Live FX reports 150 ms latency; use host compensation. ARA cached playback reports zero latency. Source PCM is embedded in project state, capped at 10 minutes / 240 MiB per instance, so projects can become large.

## Build, evidence and licensing

Use Visual Studio 2022 Desktop C++, Windows SDK, CMake 3.24+ and Git, then run `scripts/build-windows.ps1`. The source package includes pinned dependency source for offline builds. See [reproducible build instructions](docs/BUILD-3.0.md), [test plan](docs/TEST-PLAN-3.0.md), [3.0.1 workflow test report](docs/TEST-REPORT-3.0.1.md) and [3.0 baseline report](docs/TEST-REPORT-3.0.md), [research/architecture](docs/ARCHITECTURE-3.0.md) and [Sonar acceptance protocol](docs/SONAR-TEST.md). The corpus is explicitly synthetic, instrument-inspired audio, not real violin/ney/oud recordings.

AGPL-3.0-or-later; see [commercial licensing implications](docs/LICENSING-3.0.md). No paid dependencies, remote AI calls or private-audio uploads are introduced. This is a development-branch package, not a new published GitHub Release.
