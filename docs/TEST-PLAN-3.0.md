# 3.0 first-delivery acceptance plan

Defined before adaptive DSP changes, 2026-10-20. Baseline: c2caa39ce3fb0604732db11e3a9de3b317a66729. Development is separate from the unchanged 0.2.0 release.

## Mandatory gates

- Existing engine, live, state, editor, ARA and actual Windows DLL checks still run; failures are investigated, not disabled.
- Zero requested correction and confidently already-tuned synthetic material return original PCM bit for bit in the offline adaptive path.
- A stable synthetic event translated by +/-3/5/20 cents achieves its requested centre within 1 cent after independently reanalyzing rendered output. Ambiguous material may be declined and must be reported as declined, not passed tuning accuracy.
- Constant regional translation preserves vibrato: source/output pitch-contour residual after subtracting their centres has correlation >=0.97 and RMS error <=3 cents in reliable interior frames.
- A connected phrase between two plateaus has continuous correction; maximum correction-induced step measured at 1 ms <=1 cent. Its transition occupies the detected transition interval. When this cannot meet both centres safely, the plan reports reduced correction/conflict.
- A discrete event has constant planned correction across its pitched interior. Protected attack samples are exactly original; the first 15 ms at independently known synthetic pick onsets are also checked, rather than testing only self-detected boundaries; any dry/processed bridge is explicitly measured and flagged as needing real-instrument listening.
- Conflicting corrections on detected overlapping tails are declined together. A single shared shifter must never be described as independent-note separation.
- Weak/missing fundamental and changing-harmonic synthetic cases are measured. Octave ambiguity must result in a safe decline or correct result, never an unbounded correction.
- Output sample count and sample rate are unchanged; no NaN/Inf, unintended clipping or stereo collapse. Identical/inverted stereo remains linked; true stereo phase is measured.
- Analysis/render can be cancelled without publishing partial audio. Parameters, analysis metadata, manual targets and undo/state behavior remain functional.

## Corpus and reproducibility

Generate instrument-inspired, explicitly synthetic signals for sustained vibrato, connected bowed/wind phrases, ensemble beating/common drift, irregular plucks, resonant plucks, overlapping arpeggios, stiff-string attack/decay, missing fundamental, harmonic dominance changes, microtonal variants and already-correct material. Save input WAV, frame analysis, correction plan, output WAV, A/B and metrics for every fixture at 44.1/48 kHz. They are controlled stress models, not recordings of the named instruments.

Permitted real recordings have not been supplied. Several academic/corpus sites are blocked by the environment's outbound proxy; document those access failures. Do not distribute audio of unknown/noncommercial license or claim critical human listening. Collect licensed real performances and a human listening panel as the next gate.

Native Windows/Sonar/HP EliteBook tests are separate, unrun acceptance gates. Wine and an ARA fixture do not certify them. First delivery is an engineering preview, not a commercial sound-quality certification.

Regression extension after diagnosing a delayed stable-note boundary: the protected samples must include the independently known first 15 ms of each synthesized pick attack at 0, 0.8 and 1.6 seconds, in both channels and project rates. This supplements the original detected-interval identity check.
