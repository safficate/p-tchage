# Pitchmap Pro 3.1.0

Windows x64 VST3 built with JUCE and Signalsmith Stretch. This revision replaces the new-instance interface with a live source/target workspace. Legacy projects retain their previous editor and processing semantics.

Read [the Turkish usage guide](docs/KULLANIM-3.1-TR.md), [implementation and research](docs/SMOOTHER-RESEARCH-3.1.md) and [test report](docs/TEST-REPORT-3.1.md). Builds are delivered on the development branch; no GitHub Release is published.

## Workflow

Insert on an audio track and play the DAW. The plugin learns pitch centres and a timeline from audio that actually passes through it. Replaying the same position replaces its observations instead of appending duplicate history. ESITLE copies measured/manual source centres to editable targets. Target changes affect live processing immediately; no Capture or Apply step. Global cents and semitones remain separate from individual targets.

Small source and target Dinle buttons audition their tuned tones: a short click lasts approximately half a second; holding sustains the tone. An unread source has no fabricated measured tone. UI scale is saved independently of graph zoom.

TUM PARCA BURADA freezes the acquired interval and prepares regional audio in the background; restart playback or seek to hear the prepared path. Missing audio intervals are rejected. Customized live transition/centre/slope settings, human drift, centre narrowing and simultaneous host-sidechain following use live processing and Sonar bounce instead of the prepared cache.

Detailed pages separate correction amount/transition behavior, optional human drift, optional dynamic resonance smoothing and an optional final filter. Tone processing defaults off. The activity history describes actual DSP commands; a planned curve is not a measured output.

References may be learned from another instance in the same host process or supplied through the host's sidechain bus. Instances cannot retrieve arbitrary DAW clips or cross a process sandbox. No MIDI or external audio interface is required; the DAW owns the audio driver.

## Limits

This is a common-mode pitch shifter, not independent polyphonic note separation. Automatic behavior uses signal heuristics, not a trained instrument classifier or neural performer. It cannot guarantee artifact-free or human-replayed sound. Current stable Sonar, native Windows GUI and the specified HP laptop remain untested. Wine execution is not Sonar certification.

The AI page implements OpenAI API chat, bounded parameter commands, explicitly requested 12-second output audio capture, and copyable feedback. It requires a separate API key and Internet; no key is saved in project state. No request is made automatically. Service behavior remains unverified without a key. It does not connect directly to the current ChatGPT conversation.

44.1/48 kHz mono/stereo are covered by controlled tests. Live processing reports 150 ms latency for host compensation. Source histories/maps/parameters are saved with the project; complete-source PCM can enlarge project files. Acquisition is capped at 600 seconds and approximately 240 MiB of stored PCM per instance; rendering needs additional memory.

## Build and license

C++20, Visual Studio 2022 Desktop C++, Windows SDK and CMake 3.24+: run scripts/build-windows.ps1. The corresponding source archive includes pinned JUCE, Signalsmith and ARA dependency sources. See [build instructions and dependency pins](docs/BUILD-3.0.md). No additional runtime DSP dependency was introduced.

AGPL-3.0-or-later. See LICENSE and [dependency licensing](docs/LICENSING-3.0.md). Real-recording listening examples carry their own CC BY 4.0 attribution. Existing 0.2/3.0 delivery files are retained.
