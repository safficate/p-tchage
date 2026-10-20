# AI control architecture — design only

Priority C is not implemented as a pretend assistant. There is no model, network service, API key, chat button or automated instrument diagnosis in this first engine milestone. Offline controls and DSP work without internet.

The future boundary is: optional language interpretation → validated typed proposal → immutable analysis/plan → explicit review where required → DSP execution → independent verification. The model receives only permitted analysis summaries and user-selected context. Uploading audio or enabling a remote service requires a specific privacy/authentication decision; this task does neither. Authentication belongs in a secure credential provider, never project/source/parameter state.

A proposal schema should include schema/source-analysis revisions; region IDs/time ranges; user instruction; instrument/behavior hypothesis with evidence; confidence and uncertainty; target frequency/cents and target-map revision; allowed operation (analyze, translate, shared correction, connected transition, reduce, leave unchanged); maximum displacement/slope; transient/timbre/stereo constraints; expected side effects; verification policy and per-control lock flags. Unknown fields, stale source revisions, nonfinite values, out-of-range cents, contradictory target assignments and operations outside an allowlist are rejected.

No free text is evaluated as code or shell commands. The realtime callback consumes only an already-validated immutable audio buffer or bounded local controls. It never calls a language model, accesses a network, allocates a plan or reads secrets. The language model cannot override locked user values or claim detected problems without associated actual frames/regions.

Examples: a ney preservation instruction can request connected-phrase planning with no tightness; a bağlama request can prefer constant event displacement and transient review; ensemble instructions can authorize only a common-mode region, not player separation; oud-tail conflicts must be surfaced as declined/review. These are proposed translations of intent, not evidence that this software currently recognizes spoken/written instrument instructions.

Per action display region, observed evidence, confidence, requested/effective correction, processing method, risks and actual verification. A language response saying “sounds natural” is not a measurement or a listening panel. Version typed actions and logs so undo/replay/state restoration remain reproducible.
