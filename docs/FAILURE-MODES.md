# Known failure modes and conservative responses

| Situation | Why a plausible result can be wrong | Implemented response / remaining limit |
|---|---|---|
| Strong second harmonic / missing F0 | First periodic trough can be an octave high | Selective long-window and odd-harmonic evidence; report uncertainty. Even-only ambiguity remains unsolved |
| Breath/bow noise | Weak periodicity is not necessarily poor performance | Omit unreliable anchors; no invented target/AI diagnosis |
| Rapid ornament / continuous slide | No stable centre exists locally | Join stable plateaus with separate C(t), or leave an unresolvable region unchanged |
| Reattack of same pitch | Same pitch class does not imply one musical event | Onset evidence and silence prevent unconditional centre pooling; onset heuristic still needs real validation |
| Resonant oud/guitar body mode | Strong narrow resonance can masquerade as a note | Multi-harmonic evidence and uncertainty; a complex resonant fixture may legitimately be declined |
| Overlapping notes with different offsets | One whole-signal shifter affects both | Decline the conflict; no independent separation claim. Weak/harmonic tails may evade detection |
| String ensemble / double stop / chord | One F0 cannot identify independent intonation | Common-mode texture only; independent correction deferred |
| Piano inharmonicity/stretch tuning | Harmonic comb/12-TET target can be inappropriate | Explicit user targets, protected attack, conservative confidence; advanced piano modeling deferred |
| Very short transition | Smooth math still makes an artificial glide | Bounded-slope pair projection/reduced accuracy; global optimum remains future work; nonconvergent constraints decline the clip |
| Dry/wet attack bridge | Different phases can color or cancel | Explicitly expose compromise in evidence; real listening required |
| Unnecessary correction | DSP can modify a correct signal | Precision/uncertainty gate; whole inactive render bit-exact tests |
| Cancel/state/host change | Partial/stale render could play | Reject cancelled buffers, source/settings invalidation and compatibility state controls |
| ARA unsupported in host | Ordinary insert cannot read all project clips | Use host-provided ARA or explicit WAV/capture; no automatic project access claim |
| Real-time reference stops | Live following has no reusable learned map in 0.2 | Reusable learning is a separate implementation milestone; never label following as learning |
| AI invents listening observations | Text interpretation cannot establish facts about audio | Typed, evidence-linked design only; no network/AI calls in DSP callback |

The native Windows GUI, latest stable Sonar, target internal audio driver, CPU/RAM/dropouts and real instrument listening remain unverified. Wine and an ARA test host are evidence about those implementations, not certifications of Sonar.

The preview makes decisions per immutable offline plan. It does not yet implement a temporal activation/deactivation hysteresis state machine across successive reanalyses, a force-process override for low-confidence centres, or a calibrated perceptual artifact model. User-selected minimum intervention can be disabled, but the confidence/overlap safety rules still apply. Render verification reports missing/out-of-precision measurements; it is not an automatic claim of guaranteed cents on an arbitrary recording.
