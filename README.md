# Microtonal Pitch Map Pro 0.2.0

Windows x64 VST3 pitch correction for isolated vocals and instruments, with optional ARA 2 clip access and a reference sidechain. Uses JUCE 8.0.12 and Signalsmith Stretch. Sonar compatibility is **not yet verified in Sonar**; this is a functional test build, not a certification.

## Quick workflow

1. In an ARA-capable host, apply the plugin to selected clips as a Region FX/ARA effect. The host shares those clips without requiring playback or recording. An ordinary track FX insert cannot automatically fetch all project clips. For an ordinary insert, process incoming audio live, or Load WAV / Capture + Analyze for note selection.
2. The **12 upper keys** play reference tones. Adjust the cent field under each key to tune that reference; click the key again to hear it. A4 is 440 Hz at 0 cents.
3. The **12 lower keys** play actual note segments from the original clip. Repeated clicks cycle through occurrences. A missing source note is disabled; the plugin never invents a source sample.
4. Select a lower source key and an upper target key. Click **ESLESTIR**, then **HEDEFE UYGULA**. The destination selectors below the source keys also let you set each mapping explicitly. Do to Do sharp means +1 semitone; Do to Re means +2 semitones. No percentage is required.
5. For fine tuning, keep source and target as the same note and set the upper reference to the desired cents. Source Do +10 c matched to reference Do +5 c is corrected by -5 c. To add a relative +5 c to every detected Do instead, open Advanced and enable direct cent/note shifts; this mode adds to the source rather than matching its centre to an absolute reference.
6. Compare A/B and bounce/render the corrected channel in the host. Initial correction is disabled until Apply. Parameters, note edits, source PCM and mappings are included in component state and ARA document archives.

## Optional reference channel

Route a chosen reference track to the plugin's **Reference (sidechain)** input. Select `Sidechain: cent uyumu` to match that reference's tuning while keeping the source note, or `Sidechain: nota + cent` to follow its actual note. Click **REF. KANALA UYDUR**. The reference channel is never modified. Preserve source octave is enabled by default, allowing e.g. bass and violin to share tuning without forcing the violin into the bass register. With no confident reference the plugin applies no reference shift.

Source pitch is yellow and the target/reference is blue. Actual measured sidechain curves are shown only when supplied. Ctrl+mouse wheel zooms pitch height; wheel zooms time and Shift+wheel pans. No project-wide or polyphonic listening is claimed.

## Signal processing and limits

- Mono/stereo input, 44.1 and 48 kHz projects; ordinary playback and offline host bounce both process actual input audio. No MIDI, external interface or special hardware is needed: the host supplies audio through its selected driver.
- ARA clip correction is analyzed/rendered on a worker, then played from memory with zero reported latency. Clip import uses ARA host sample readers before playback, on a non-audio control thread.
- Ordinary live FX uses a preallocated streaming processor with host-reported latency of 150 ms (6,615/7,200 samples). Host delay compensation is needed. This is intended for editing/mixing, rather than low latency monitoring.
- Monophonic fundamental detection over approximately 30–1,600 Hz at the target 44.1/48 kHz rates, natural vibrato preservation, formant compensation and transition smoothing. Stable voiced passages use bounded centre refinement. No automatic separation of polyphonic chords or overlapping instruments.
- At most 10 minutes / 240 MiB PCM per instance. Higher rates reduce the available duration. Capture memory is allocated only when requested. Full embedded PCM makes project/archive files larger.
- ARA source files can differ from project sample rate; they are interpolated. ARA time-stretched regions must be bounced first. Multi-region spans are mixed into one per-instance map; this is not a shared multi-track editor.
- Scala import supports ascending 12-step octave scales with offsets within +/-200 cents; MIDI export is a pitch-bend guide. Corrected audio is the deliverable for precise tuning.
- Advanced legacy Quantize/strength controls remain for old presets. Basic reference matching and relative direct shifts ignore those percentages. Old v0.1 states retain their original semantics.

## Build and validation

Windows: install Visual Studio 2022 Desktop C++, Windows SDK, CMake 3.24+, Git, then run `scripts/build-windows.ps1`. Dependencies are pinned and fetched automatically; a bundled source ZIP can use `vendor/JUCE`, `vendor/signalsmith-stretch`, `vendor/signalsmith-linear`, and `vendor/ARA_SDK` offline. `-DMP_ENABLE_ARA=OFF` builds a regular FX variant if required. The delivered VST3 has ARA enabled.

Tests cover actual DSP output, live callbacks, note/cents fidelity, sidechain isolation, real reference/source auditions, state reload, a synthetic ARA host with reader-lifecycle checks, and actual Windows VST3 DLL loading/audio processing. See [test report](docs/TEST-REPORT.md) for completed results and limits; [Sonar test protocol](docs/SONAR-TEST.md) lists the native host checks still required. Wine results do not establish native Windows or Sonar compatibility.

AGPL-3.0-or-later. The complete corresponding source and dependency licenses accompany the binary release.
