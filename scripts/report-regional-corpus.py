#!/usr/bin/env python3
"""Measure exported synthetic PCM. Optional analysis tools: numpy/scipy/matplotlib.
This does not listen to audio, certify timbre, or validate a native DAW.
"""
import argparse
import csv
import json
import re
from pathlib import Path
import numpy as np
from scipy.io import wavfile
from scipy.signal import welch
from scipy.ndimage import gaussian_filter1d
from scipy.fft import rfft, rfftfreq, next_fast_len
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


def read(path):
    rate, audio = wavfile.read(path)
    if audio.dtype.kind == 'i':
        audio = audio.astype(np.float64) / (2 ** (audio.dtype.itemsize * 8 - 1))
    else:
        audio = audio.astype(np.float64)
    return rate, audio


def fundamental(audio, rate, expected):
    data = audio[int(.8 * rate):int(2.3 * rate), 0]
    size = next_fast_len(len(data) * 8)
    spectrum = abs(rfft(data * np.hanning(len(data)), n=size))
    frequencies = rfftfreq(size, 1 / rate)
    candidates = np.flatnonzero((frequencies > expected * .985) & (frequencies < expected * 1.015))
    i = candidates[np.argmax(spectrum[candidates])]
    a, b, c = np.log(np.maximum(spectrum[i - 1:i + 2], 1e-30))
    delta = .5 * (a - c) / (a - 2 * b + c)
    return (i + delta) * rate / size


def measure(base):
    rate, source = read(Path(str(base) + '-original.wav'))
    output_rate, output = read(Path(str(base) + '-corrected.wav'))
    assert rate == output_rate and source.shape == output.shape
    assert np.isfinite(output).all()
    table = np.genfromtxt(Path(str(base) + '-analysis.csv'), delimiter=',', names=True)
    plan = json.loads(Path(str(base) + '-plan.json').read_text(encoding='utf-8-sig'))
    ms = lambda x: float(np.mean(x * x))
    source_energy, output_energy = ms(source), ms(output)
    width = lambda x: ms(x[:, 0] - x[:, 1]) / max(ms(x), 1e-30)
    valid = (table['confidence'] > .85) & (table['output_confidence'] > .85) & (table['source_midi'] > 0) & (table['output_midi'] > 0)
    valid &= (table['time'] > .3) & (table['time'] < 2.7)
    delta = 100 * (table['output_midi'] - table['source_midi'])
    residual = delta[valid] - table['planned_cents'][valid]
    frequencies, a = welch(source[:, 0], rate, nperseg=4096)
    _, b = welch(output[:, 0], rate, nperseg=4096)
    a, b = gaussian_filter1d(a, 8), gaussian_filter1d(b, 8)
    bins = (frequencies > 80) & (frequencies < 8000) & (a > a.max() * 1e-5)
    envelope = 10 * np.log10(np.maximum(b[bins], 1e-20) / np.maximum(a[bins], 1e-20))
    row = dict(case=base.name, rate=rate, samples=len(output), peak=float(abs(output).max()),
               rms_ratio_db=float(10 * np.log10(max(output_energy, 1e-30) / max(source_energy, 1e-30))),
               stereo_width_source=width(source), stereo_width_output=width(output),
               stereo_correlation_output=float(np.corrcoef(output.T)[0, 1]),
               bit_exact=bool(np.array_equal(source, output)),
               median_abs_contour_residual_cents=float(np.median(abs(residual))) if len(residual) else None,
               envelope_median_abs_change_db=float(np.median(abs(envelope))),
               regions=len(plan['regions']),
               rendered_precision_passes=sum(bool(r['verificationPassed']) for r in plan['regions']),
               rendered_precision_unconfirmed=sum(abs(r['appliedCents']) > 1e-9 and not r['verificationPassed'] for r in plan['regions']))
    if base.name.endswith('baglama-plucks'):
        errors = [float(abs(output[int(np.ceil(t * rate)):int((t + .015) * rate)] - source[int(np.ceil(t * rate)):int((t + .015) * rate)]).max()) for t in [0, .8, 1.6]]
        row['known_attack_max_absolute_difference'] = max(errors)
        assert max(errors) == 0, 'Known pick attack changed in the first 15 ms'
    match = re.search(r'fine-(-?\d+)c$', base.name)
    if match:
        target = float(match.group(1))
        actual = 1200 * np.log2(fundamental(output, rate, 440 * 2 ** (target / 1200)) / fundamental(source, rate, 440))
        row.update(requested_cents=target, independent_spectral_cents=float(actual), independent_spectral_error_cents=float(actual - target))
    return row, table


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('corpus', type=Path)
    parser.add_argument('out', type=Path)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    rows, tables = [], {}
    for original in sorted(args.corpus.glob('*-original.wav')):
        base = Path(str(original)[:-len('-original.wav')])
        row, table = measure(base)
        rows.append(row)
        tables[base.name] = table
    assert rows, 'No exported corpus found'
    fields = sorted(set().union(*(r.keys() for r in rows)))
    with (args.out / 'audio-metrics.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fields)
        writer.writeheader()
        writer.writerows(rows)
    (args.out / 'audio-metrics.json').write_text(json.dumps(rows, indent=2) + '\n')
    examples = ['violin-vibrato', 'violin-legato', 'ney-slide-noise', 'baglama-plucks', 'missing-fundamental', 'changing-harmonics']
    fig, axes = plt.subplots(3, 2, figsize=(12, 10), constrained_layout=True)
    for name, axis in zip(examples, axes.flat):
        table = tables['48000-' + name]
        valid = (table['source_midi'] > 0) & (table['output_midi'] > 0)
        ref = round(float(np.median(table['source_midi'][valid])))
        t = table['time'][valid]
        axis.plot(t, 100 * (table['source_midi'][valid] - ref), color='#a67c00', label='Source analysis')
        axis.plot(t, 100 * (table['source_midi'][valid] - ref) + table['planned_cents'][valid], '--', color='#c54472', label='Planned P+C')
        axis.plot(t, 100 * (table['output_midi'][valid] - ref), color='#2068ad', linewidth=1, alpha=.8, label='Rendered reanalysis')
        axis.set(title=name + ' (synthetic, 48 kHz)', xlabel='Time (s)', ylabel=f'Cents from MIDI {ref}')
        axis.grid(alpha=.2)
    axes.flat[0].legend(fontsize=8)
    fig.savefig(args.out / 'pitch-contours.pdf')
    fig.savefig(args.out / 'pitch-contours.png', dpi=140)
    plt.close(fig)
    fine = [r for r in rows if 'independent_spectral_error_cents' in r]
    fig, axis = plt.subplots(figsize=(10, 4), constrained_layout=True)
    axis.bar(range(len(fine)), [r['independent_spectral_error_cents'] for r in fine], color='#2068ad')
    axis.axhline(1, color='red', linestyle='--'); axis.axhline(-1, color='red', linestyle='--')
    axis.set_xticks(range(len(fine)), [f"{r['rate']/1000:g}k / {r['requested_cents']:+g}c" for r in fine], rotation=45, ha='right')
    axis.set(ylabel='Spectral fundamental error (cents)', title='Independent measurement of rendered stable harmonic fixtures')
    axis.grid(axis='y', alpha=.2)
    fig.savefig(args.out / 'fine-cent-errors.pdf'); fig.savefig(args.out / 'fine-cent-errors.png', dpi=140)
    plt.close(fig)
    rate, source = read(args.corpus / '48000-baglama-plucks-original.wav')
    _, output = read(args.corpus / '48000-baglama-plucks-corrected.wav')
    fig, axes = plt.subplots(2, 2, figsize=(11, 6), constrained_layout=True)
    for column, attack in enumerate([.8, 1.6]):
        begin, end = int(round(attack * rate)), int(round((attack + .08) * rate))
        t = (np.arange(begin, end) / rate - attack) * 1000
        axes[0, column].plot(t, source[begin:end, 0], color='#a67c00', label='Source')
        axes[0, column].plot(t, output[begin:end, 0], color='#2068ad', alpha=.7, label='Rendered')
        axes[0, column].set(title=f'Synthetic pick at {attack:g} s', ylabel='Amplitude')
        axes[1, column].plot(t, output[begin:end, 0] - source[begin:end, 0], color='#2068ad')
        axes[1, column].set(xlabel='Time after known attack (ms)', ylabel='Rendered minus source')
        for axis in axes[:, column]:
            axis.axvline(15, color='#c54472', linestyle='--', label='15 ms identity gate')
            axis.grid(alpha=.2)
    axes[0, 0].legend(fontsize=8)
    fig.savefig(args.out / 'attack-protection.pdf'); fig.savefig(args.out / 'attack-protection.png', dpi=140)
    plt.close(fig)
    summary = dict(cases=len(rows), synthetic=True, real_recordings=False,
                   peak=max(r['peak'] for r in rows),
                   max_independent_spectral_error_cents=max(abs(r['independent_spectral_error_cents']) for r in fine),
                   known_attack_max_absolute_difference=max(r.get('known_attack_max_absolute_difference', 0) for r in rows),
                   bit_exact_cases=[r['case'] for r in rows if r['bit_exact']],
                   limitations='Spectral envelope change and contour residuals are diagnostic, not perceptual quality scores; no human listening or native Sonar test.')
    (args.out / 'METRICS-SUMMARY.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2))

if __name__ == '__main__':
    main()
