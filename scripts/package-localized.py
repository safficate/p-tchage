#!/usr/bin/env python3
"""Package a verified localized x64 VST3 and its corresponding source."""
import argparse
import hashlib
import json
import re
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
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['stage','vendor','validation','out']:
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    root=Path(__file__).resolve().parent.parent
    version=re.search(r'project\(MicrotonalPitchMapPro VERSION ([0-9.]+)',(root/'CMakeLists.txt').read_text()).group(1)
    report=json.loads((args.validation/'TEST-RESULTS.json').read_text())
    assert report['version']==version and report['native_linux_groups_passed']==9
    for name in ['state','live','actual-vst3','smoother','localization']:
        assert (args.validation/'windows'/(name+'.code')).read_text().strip()=='0',name
    dll=args.stage/'Microtonal Pitch Map Pro.vst3/Contents/x86_64-win/Microtonal Pitch Map Pro.vst3'
    assert dll.read_bytes()[:2]==b'MZ' and sha(dll)==report['final_dll_sha256']
    for name in ['Chinese-OFL.txt','Arabic-OFL.txt','Latin-OFL.txt']:
        assert (args.stage/'licenses'/('Fonts-'+name)).read_bytes()==(root/'Assets/Fonts'/name).read_bytes(),name
    commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
    manifest=dict(**report,source_commit=commit)
    (args.stage/'BUILD-MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (args.stage/'SOURCE.txt').write_text('Corresponding source (AGPL-3.0-or-later):\nhttps://github.com/safficate/p-tchage/tree/'+commit+'\n\nThe offline source ZIP including pinned dependencies is delivered alongside this plugin.\nBuild with scripts/build-windows.ps1. See LICENSE and licenses/.\n')
    args.out.mkdir(parents=True,exist_ok=True)
    archive(args.out/('Pitchmap-Pro-'+version+'-Windows-x64.zip'),[(p,p.relative_to(args.stage)) for p in sorted(args.stage.rglob('*')) if p.is_file()])
    names=subprocess.check_output(['git','ls-files','-z'],cwd=root).decode().split('\0')
    source=[(root/n,n) for n in names if n and not n.startswith(('releases/','deliveries/','dist/','build')) and (root/n).is_file()]
    for name in ['JUCE','signalsmith-stretch','signalsmith-linear','ARA_SDK']:
        base=args.vendor/name
        assert base.is_dir()
        for path in sorted(base.rglob('*')):
            parts=path.relative_to(base).parts
            if '.git' not in parts and path.is_file() and not (name=='ARA_SDK' and parts[0]=='ARA_Examples'):
                source.append((path,Path('vendor')/name/path.relative_to(base)))
    archive(args.out/('Pitchmap-Pro-'+version+'-Source.zip'),source)
    archives=sorted(args.out.glob('*.zip'))
    artifacts={p.name:dict(bytes=p.stat().st_size,sha256=sha(p)) for p in archives}
    (args.out/'ARTIFACTS.json').write_text(json.dumps(dict(**manifest,artifacts=artifacts),indent=2)+'\n')
    (args.out/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in archives))
    print(json.dumps(artifacts,indent=2))


if __name__=='__main__':
    main()
