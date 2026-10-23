#!/usr/bin/env python3
"""Package the 3.1 x64 VST3, corresponding source and attributed audio evidence."""
import argparse
import hashlib
import json
import subprocess
import zipfile
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def archive(path, files):
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED, 6) as output:
        for source, name in files:
            output.write(source, str(name))
    with zipfile.ZipFile(path) as output:
        assert output.testzip() is None
    assert path.stat().st_size < 95_000_000


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--vendor', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    codes = {name: (args.evidence / (name+'-release-windows.code')).read_text().strip()
             for name in ['state', 'live', 'actual-vst3', 'smoother']}
    assert all(code == '0' for code in codes.values()), codes
    assert '100% tests passed, 0 tests failed out of 8' in (args.evidence/'linux-ctest-release.log').read_text()
    assert '47 actual Windows VST3' in (args.evidence/'actual-vst3-release-windows.log').read_text()
    for name in ['smoother-release-linux.log','smoother-release-windows.log']:
        assert '135 smoother/workspace assertions passed; callback allocations=0' in (args.evidence/name).read_text(), name
    args.out.mkdir(parents=True, exist_ok=True)
    commit = subprocess.check_output(['git','rev-parse','HEAD'], cwd=root, text=True).strip()
    dll = args.stage/'Microtonal Pitch Map Pro.vst3/Contents/x86_64-win/Microtonal Pitch Map Pro.vst3'
    assert dll.read_bytes()[:2] == b'MZ'
    manifest = dict(version='3.1.0',source_commit=commit,windows_execution='Wine 10 on Linux; not native Windows',
                    sonar_tested=False,native_windows_gui_tested=False,target_laptop_tested=False,
                    human_listening_verified=False,ai_service_verified=False,independent_polyphonic_notes=False,
                    native_linux_ctest_groups=8,actual_windows_vst3_cases=47,windows_exit_codes=codes,
                    binary_sha256=sha(dll))
    (args.stage/'BUILD-MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
    archive(args.out/'Pitchmap-Pro-3.1.0-Windows-x64.zip',[(p,p.relative_to(args.stage)) for p in sorted(args.stage.rglob('*')) if p.is_file()])
    names = subprocess.check_output(['git','ls-files','-z'],cwd=root).decode().split('\0')
    source = [(root/n,n) for n in names if n and not n.startswith(('releases/','deliveries/','dist/','build')) and (root/n).is_file()]
    for name in ['JUCE','signalsmith-stretch','signalsmith-linear','ARA_SDK']:
        base = args.vendor/name
        assert base.is_dir()
        for path in sorted(base.rglob('*')):
            parts=path.relative_to(base).parts
            if '.git' in parts or not path.is_file() or (name=='ARA_SDK' and parts[0]=='ARA_Examples'):
                continue
            source.append((path,Path('vendor')/name/path.relative_to(base)))
    archive(args.out/'Pitchmap-Pro-3.1.0-Source.zip',source)
    examples = args.evidence/'comparison-package'
    archive(args.out/'Pitchmap-Pro-3.1.0-Audio-Comparisons.zip',[(p,p.relative_to(examples)) for p in sorted(examples.rglob('*')) if p.is_file()])
    archives=sorted(args.out.glob('*.zip'))
    artifacts={p.name:dict(bytes=p.stat().st_size,sha256=sha(p)) for p in archives}
    (args.out/'ARTIFACTS.json').write_text(json.dumps(dict(**manifest,artifacts=artifacts),indent=2)+'\n')
    (args.out/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in archives))
    print(json.dumps(artifacts,indent=2))


if __name__ == '__main__':
    main()
