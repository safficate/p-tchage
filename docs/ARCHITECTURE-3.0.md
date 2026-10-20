# PitchMap Pro 3.0 engineering architecture

First delivery is an offline regional-correction engineering preview built on 0.2.0, not a replacement application. Live DSP remains the bounded 0.2-style path; it is not advertised as the new offline adaptive optimizer. Native Sonar certification is outstanding.

## Audit of the actual 0.2.0 source

`PitchEngine.cpp::analyse` used one 4096-sample YIN-style window, a frame-to-frame 0.65-semitone segmentation threshold and an unweighted median centre. A smooth glide could remain one huge event; vibrato and harmonic changes could trigger inappropriate splits. Notes were trimmed to a nonoverlapping timeline; that did not separate the underlying overlapping audio. `correctionAt` faded each note toward zero at both ends using one fixed smoothing time. Adjacent nonzero corrections therefore unnecessarily returned to zero. `render` performed up to four centre-calibration passes; pitch accuracy did not establish transient/timbre quality. `RealtimePitch` had separate centre and instantaneous trajectories, but fixed smoothing and no reusable reference map. Its sidechain followed incoming F0 rather than learning a persistent tuning configuration. The UI placed reference above source, the reverse of the new brief. Existing state includes PCM and note edits, and ARA already supplies host clips before playback in the synthetic-host fixture.

The baseline was actually rebuilt. Five Linux test groups passed after ensuring tests ran after linking completed. A first concurrent build/test attempt temporarily could not execute relinking files; it was not treated as an application pass. The original Windows DLL's 29 audio scenarios were rerun through its factory under Wine. See saved baseline logs; these are not Sonar tests.

## Layers and ownership

1. Host/import: existing WAV, capture and ARA paths create immutable PCM snapshots.
2. `analyseAdaptive`: offline multi-window observations, explicit onset/agreement/uncertainty metadata, stable anchors and behavior hypotheses.
3. `planCorrection`: requested musical targets become separately modeled regional displacements. It applies minimum-intervention and overlap decisions before constructing the correction trajectory.
4. `renderAdaptive`: existing Signalsmith backend executes the planned common-mode displacement, aligns latency and protects selected attack samples. Bounded centre calibration runs only on reliable interiors; it does not flatten source F0.
5. Verification: independently reestimate source/output interior pitch and preserve actual measurements in the plan; corpus tests save analysis, plan, audio and A/B.
6. UI: source above target, meaningful basic controls, advanced precision/override controls, cancellation and reasons. No AI service or invented instrument classifier.

All expensive new work is outside `processBlock`. The callback still reads a published immutable render or uses the existing live path. Analysis/render snapshots use the existing worker and publication ownership. Cancellation rejects partial buffers. State version 3 persists adaptive metadata and controls; versions 1/2 retain legacy semantics with adaptive processing disabled after restore. Changing relevant settings invalidates the render via the existing settings fingerprint. Frame analysis is reused until the source is reanalyzed; plan/display fingerprints avoid per-refresh spectral work.

No new third-party DSP runtime is introduced. Instrument labels in test files describe stress-model inspiration, not an acoustic instrument recognizer. See the test plan, limitations and roadmap for boundaries between implemented behavior and research.

Attack protection uses short-window onset evidence rather than treating delayed stable-F0 onset as a pick attack. The known synthetic attack sample locations are independently asserted. Both onset lookup and wet-envelope lookup use sorted/bounded searches; rendering does not scan every detected event for every output sample. Actual output verification reports missing/failed precision instead of labeling every processed event as verified.

When a user explicitly switches an old preset into the adaptive UI workflow, the UI clears its legacy octave switch and selects reference matching unless relative direct mode was chosen. This avoids hidden octave/percentage controls steering the new pitch-class fields; simply restoring v1/v2 state keeps legacy semantics. Direct automation of conflicting hidden legacy parameters is not a supported adaptive workflow.
