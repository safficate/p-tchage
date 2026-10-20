#!/usr/bin/env python3
"""Package a validated development preview, without creating a GitHub Release."""
import argparse
import hashlib
import json
import subprocess
import zipfile
from pathlib import Path


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def archive(path, files):
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED, 6) as target:
        for source, name in files:
            target.write(source, name)
    with zipfile.ZipFile(path) as target:
        assert target.testzip() is None, path
    assert path.stat().st_size < 95_000_000, 'Split artifacts before pushing files near GitHub size limits'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--vendor', type=Path, required=True)
    parser.add_argument('--corpus', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    validation = args.stage / 'validation'
    names = ['engine', 'regional', 'state', 'live', 'ara', 'actual-vst3', 'install', 'installed-vst3', 'uninstall']
    for name in names:
        assert (validation / 'windows' / (name + '.code')).read_text().strip() == '0', name
    assert '13040 regional rendered-audio assertions passed' in (validation / 'windows/regional.log').read_text()
    assert '29 actual Windows VST3' in (validation / 'windows/actual-vst3.log').read_text()
    assert '100% tests passed, 0 tests failed out of 6' in (validation / 'linux-ctest.log').read_text()
    assert len(list(args.corpus.glob('*-original.wav'))) == 36
    source_commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    binaries = ['Microtonal Pitch Map Pro.exe', 'Microtonal Pitch Map Pro.vst3/Contents/x86_64-win/Microtonal Pitch Map Pro.vst3', 'MicrotonalPitchMapPro-Setup-x64.exe']
    manifest = dict(version='3.0.0-engineering-preview', source_commit=source_commit,
                    windows_execution='Wine 10 on Linux; not native Windows',
                    native_linux_ctest_groups=6, regional_assertions=13040,
                    exported_corpus_assertions=13148, actual_vst3_cases=29,
                    sonar_tested=False, target_laptop_tested=False, native_windows_gui_tested=False,
                    real_recordings_tested=False, human_listening_panel=False,
                    windows_exit_codes={n: 0 for n in names},
                    binary_sha256={n: sha(args.stage / n) for n in binaries})
    (args.stage / 'BUILD-MANIFEST.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (args.stage / 'SHA256SUMS.txt').write_text(''.join(f'{sha(args.stage / n)}  {n}\n' for n in binaries))
    args.out.mkdir(parents=True, exist_ok=True)
    archive(args.out / 'Plugin-Windows-x64.zip', [(p, p.relative_to(args.stage)) for p in sorted(args.stage.rglob('*')) if p.is_file()])
    tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=root).decode().split('\0')
    source = [(root / n, n) for n in sorted(tracked) if n and not n.startswith(('releases/', 'deliveries/', 'dist/', 'build')) and (root / n).is_file()]
    for name in ['JUCE', 'signalsmith-stretch', 'signalsmith-linear', 'ARA_SDK']:
        base = args.vendor / name
        assert base.is_dir(), base
        for path in sorted(base.rglob('*')):
            parts = path.relative_to(base).parts
            if '.git' in parts or not path.is_file() or (name == 'ARA_SDK' and parts[0] == 'ARA_Examples'):
                continue
            source.append((path, Path('vendor') / name / path.relative_to(base)))
    archive(args.out / 'Source.zip', source)
    for rate in ['44100', '48000']:
        files = [(p, p.name) for p in sorted(args.corpus.glob(rate + '-*')) if p.is_file()]
        files.append((args.corpus / 'README.txt', 'README.txt'))
        archive(args.out / ('AudioExamples-' + rate + '.zip'), files)
    archives = sorted(args.out.glob('*.zip'))
    (args.out / 'SHA256SUMS.txt').write_text(''.join(f'{sha(path)}  {path.name}\n' for path in archives))
    print(json.dumps(dict(source_commit=source_commit, artifacts={p.name: dict(bytes=p.stat().st_size, sha256=sha(p)) for p in archives}), indent=2))

if __name__ == '__main__':
    main()
