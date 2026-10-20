# Instrument acoustics and safe correction hypotheses

These are engineering models to test, not claims that a classifier recognizes all instruments. The consulted sources include STK's `Bowed`, `Plucked`, `StifKarp`, and `Flute` implementations and Şentürk's `tomato` methodology documentation. STK's flute is not a quantitative model of ney, and a generic plucked string is not a validated model of bağlama or oud. Primary instrument-acoustics websites were blocked by the outbound proxy. Instrument-specific recordings and specialist listening remain required; this evidence gap is explicit rather than filled with invented measurements.

## Solo violin

Bowed excitation continually supplies energy; STK's bowed model separates bow pressure/velocity/position, bridge/neck delays, body resonances and vibrato. Body-response peaks change harmonic amplitude without changing string period. A bow change can add noise/envelope discontinuity while the intended note continues. F0 must survive a temporary stronger upper partial; an amplitude onset alone does not prove a new musical pitch. Stable-centre estimation should cover vibrato cycles and exclude actual slides. Constant displacement preserves vibrato excursions; glide boundaries should come from stable anchors. Double stops violate the first milestone's single-centre model and require common displacement or review, not independent pitch assignments.

## String ensemble

Several nearby fundamentals, different vibrato phases, unison detuning and room/microphone phase are the signal. A single common transposition preserves relative frequency ratios; its multiplicative ratio also scales absolute beat frequencies slightly, so exact unchanged beating in Hz must not be promised. One strongest F0 is not the tuning of every player. Require coherent common drift evidence before automatic correction, preserve multichannel processing, and flag incompatible internal centres. Synthetic detuned voices test stereo retention/common displacement. Real ensemble naturalness has not been established.

## Bağlama

Course pairs/triples can beat; fret placement represents makam-oriented intervals rather than a compulsory 12-TET destination. A mızrap onset adds broadband energy and the relative amplitudes of course/string harmonics change rapidly in decay. Tremolo can reattack one pitch repeatedly; treating each spectral peak as a different note creates false bends. Rapid ornaments may offer no stable centre. Estimate after the noisy attack, retain the entire intra-event pitch trajectory and do not average distinct fret variants merely because they share a nominal pitch class. Synthetic irregular plucks cover attack/decay decisions, not the complete acoustics of a particular bağlama/course tuning.

## Oud

A fretless finger position, risha contact and pitch settling must be distinguished from resonance. A body mode at a nearly fixed frequency is not automatically the played fundamental; missing-fundamental evidence needs a family of harmonics. Long neighboring string tails may coexist with a new risha attack. Where target offsets differ, one shifter cannot tune both separately: decline the conflict or accept an explicitly shared offset. A narrow body-mode tone plus a harmonic string model is a stress test, not a resynthesis of a real oud. Low-confidence resonant mixtures must be allowed to remain unchanged.

## Ney

Breath/edge noise can lower periodicity confidence while upper bore harmonics remain strong. Breath pressure, embouchure and fingering can move pitch continuously, including microtonal approach and departure ornaments. Do not infer a fresh note at each semitone crossing. Use plateau anchors and preserve the intervening source contour. A stable endpoint may exist even when the slide itself has no reliable instantaneous F0. STK's jet/noise/vibrato separation motivates tests, but its side-blown flute geometry cannot establish ney-specific thresholds. The breath-noise/weak-fundamental synthetic glide is only a controlled approximation; actual ney recordings are a release gate.

## Guitar

A pluck starts with a sharp excitation and a time-changing harmonic envelope. Initial pitch sharpening, bends and relaxation toward a settled centre can be intentional or physical; a fixed regional transposition keeps those movements. Open strings and arpeggio tails produce mixed pitches even when only one new attack occurs. The pluck-position comb and string stiffness modeled in STK show why harmonic amplitude is not a stable F0 indicator. Begin with single events/common-mode transposition; chords and conflicting pedal-like tails are review cases. Evaluate spectral envelope, pitch drift, decay and stereo, not only one sustain frequency.

## Piano

Stiff strings have partials approximated by `f_n = n*f_0*sqrt(1+B*n^2)`; the inharmonicity coefficient depends on the string. Tuned unisons, hammer noise, soundboard modes, stretch tuning and sustain pedal defeat a single exact-harmonic template. A harmonic fit must not force all partials into equal spacing. Preserve the hammer region and decay, avoid legato correction ramps between struck notes, and decline conflicting overlaps. The synthetic stiff-partial fixture covers this failure pressure, not a piano-specific model. Advanced partial resynthesis/stretch-tuning inference is deferred.

References actually consulted: [STK Bowed](https://github.com/thestk/stk/blob/master/src/Bowed.cpp), [Plucked](https://github.com/thestk/stk/blob/master/src/Plucked.cpp), [StifKarp](https://github.com/thestk/stk/blob/master/src/StifKarp.cpp), [Flute](https://github.com/thestk/stk/blob/master/src/Flute.cpp), [tomato](https://github.com/sertansenturk/tomato). Source retrieval status/hashes are in `RESEARCH-SOURCES-3.0.json`.
