# PitchMap Pro 3.0 — engineering preview

Windows x64 VST3 and standalone, based on the existing JUCE/Signalsmith project. This development build adds **offline adaptive regional pitch translation**: it moves a reliable tuning centre while preserving the original pitch movement. It is an engineering preview, not a commercial audio-quality or Sonar compatibility certification. The original 0.2.0 delivery is retained unchanged.

## Use it

1. Obtain clips through an ARA Region FX in a host that actually supports this plugin, or use **Load WAV**. An ordinary channel insert cannot automatically retrieve every clip or hear unrelated channels. Live channel processing and capture remain available.
2. Leave **Dogal bolgesel (klip/offline)** enabled. Analyze the clip. **Upper keys audition actual source segments; lower keys audition editable target tones.** Missing source notes are disabled. Repeated source clicks cycle through occurrences.
3. For an absolute tuning target, select a source key and lower target key, enter the target cents, then **ESLESTIR → HEDEFE UYGULA**. Source Do +10 c matched to target Do +5 c requests -5 c. Do to Re is a two-semitone destination, without a percentage.
4. For a relative change such as “add +5 cents”, enable direct cent/note shifts in Advanced and enter +5 c on the same destination note. The requested amount is relative to the source, rather than an absolute reference centre. Keep unrelated global offsets at zero.
5. Review the yellow source and blue planned output curves. Apply, compare A/B, listen, refine, then bounce the channel or export WAV. Offline processing is cancelable; partial output is not published. Initial correction remains disabled until Apply.

Advanced exposes precision and minimum intervention. Below the selected precision/estimated uncertainty the engine can retain original PCM. Low-confidence centres and identified conflicting overlapping tails are declined. Short connected transitions can require reduced corrections. These decisions are explicit; a requested correction is not a guarantee on every recording.

## Implemented in this delivery

- Multi-window offline YIN analysis, harmonic checks, weighted robust regional centres and heuristic uncertainty.
- Original `P(t)` and separately planned `C(t)`, constant regional offsets, transitions between stable anchors, bounded corrections when transition time is insufficient.
- Automatic continuous/discrete behavior hypotheses; dry attack protection with an explicit processed-audio bridge; no automatic flattening of vibrato.
- Minimum intervention, linked stereo, actual rendered-output verification and bounded stable-centre refinement. Every measurement is from emitted PCM, not just a parameter value.
- Source/target row order clarified, background cancellation, optional analysis metadata saved with host state, legacy v1/v2 state semantics retained.
- Existing custom cents, 12-note Scala import, note-level edits, undo/redo, ARA clip access, WAV import/export, standalone, and live reference sidechain.

## Limits that matter

Persistent **sidechain reference-map learning is unfinished**. Existing sidechain modes follow a simultaneously playing reference; they do not retain a learned map. Only routed audio reaches the plugin. The adaptive planner operates on offline clips, while live input uses the existing lower-cost streaming engine.

This is one common-mode shifter. It cannot separately tune overlapping notes, players or chord tones. Tail detection is heuristic and can miss an overlap. Uncertainty is an engineering estimate, not a calibrated probability. Attacks kept dry are initially unshifted; the following bridge can color the sound. Piano-specific modeling, neural separation, trained instrument inference and the AI assistant are research/design only, with no placeholder buttons. Real-instrument listening and native Windows/current Sonar/HP laptop validation remain outstanding.

44.1/48 kHz mono/stereo are supported by the audio tests. No MIDI or external audio interface is required; the DAW supplies its selected driver. Live FX reports 150 ms latency; use host compensation. ARA cached playback reports zero latency. Source PCM is embedded in project state, capped at 10 minutes / 240 MiB per instance, so projects can become large.

## Build, evidence and licensing

Use Visual Studio 2022 Desktop C++, Windows SDK, CMake 3.24+ and Git, then run `scripts/build-windows.ps1`. The source package includes pinned dependency source for offline builds. See [reproducible build instructions](docs/BUILD-3.0.md), [test plan](docs/TEST-PLAN-3.0.md), [3.0 test report](docs/TEST-REPORT-3.0.md), [research/architecture](docs/ARCHITECTURE-3.0.md) and [Sonar acceptance protocol](docs/SONAR-TEST.md). The corpus is explicitly synthetic, instrument-inspired audio, not real violin/ney/oud recordings.

AGPL-3.0-or-later; see [commercial licensing implications](docs/LICENSING-3.0.md). No paid dependencies, remote AI calls or private-audio uploads are introduced. This is a development-branch package, not a new published GitHub Release.
