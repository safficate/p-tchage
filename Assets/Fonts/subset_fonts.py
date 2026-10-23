#!/usr/bin/env python3
"""Rebuild embedded OFL UI subsets from verified Google Fonts originals.

The plugin build uses the checked-in .ttf files and does not run this script.
To regenerate: python subset_fonts.py --source-dir /workspace/font-source --download
The catalog defaults to Source/Localization.cpp. FontTools 4.61.1 was used.
"""
import argparse
import copy
import hashlib
import json
import unicodedata
import urllib.request
from pathlib import Path
from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

ROOT = Path(__file__).resolve().parent
SOURCES = [
    ('NotoSansSC.ttf', 'https://raw.githubusercontent.com/google/fonts/main/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf', 'a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da'),
    ('Chinese-OFL.txt', 'https://raw.githubusercontent.com/google/fonts/main/ofl/notosanssc/OFL.txt', '1c05c68c34f9708415aada51f17e1b0092d2cea709bf4a94cd38114f9e73d7d9'),
    ('NotoSansArabic.ttf', 'https://raw.githubusercontent.com/google/fonts/main/ofl/notosansarabic/NotoSansArabic%5Bwdth,wght%5D.ttf', '63111b5b2e074dd48cc67692e0a2726d86ee94c1c37fe8598257b7b4e87e869e'),
    ('Arabic-OFL.txt', 'https://raw.githubusercontent.com/google/fonts/main/ofl/notosansarabic/OFL.txt', '07fc70bfeb985cc1a87a8587d0a0c80bab11c86c9dc3fd95b6f0cb332f983e96'),
    ('NotoSans.ttf', 'https://raw.githubusercontent.com/google/fonts/main/ofl/notosans/NotoSans%5Bwdth,wght%5D.ttf', 'bfb7bb691513f12e734dc346c03a03f784912432d7e3fa8e56efcf906fe86b3d'),
    ('Latin-OFL.txt', 'https://raw.githubusercontent.com/google/fonts/main/ofl/notosans/OFL.txt', 'cee9892f9f0cc8fe882c9e9537ee6a89621d86ee7ceaf70b02e2b2b1c25c061a'),
]
EXTRA = '中文（简体） العربية English Deutsch Türkçe Français Español Português Italiano 0123456789 +-−%.,:/#()[]→°∞'
EXTRA += '播放轨道。点击“匹配”将测量值复制到目标；修改会实时生效。'
EXTRA += 'شغّل المسار. استخدم «مطابقة» لنسخ القياسات إلى الأهداف؛ تُسمَع التغييرات مباشرة.'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rename(font, family):
    names = {1: family, 2: 'Regular', 3: family+' Regular UI subset', 4: family+' Regular', 6: family.replace(' ', '')+'-Regular', 16: family, 17: 'Regular'}
    for name in font['name'].names:
        if name.nameID in names:
            name.string = names[name.nameID].encode(name.getEncoding(), errors='replace')


def build(source, target, text, chinese):
    font = TTFont(source, recalcTimestamp=False)
    wanted = set(map(ord, text)) | set(range(0x20,0x250)) | set(range(0x2000,0x2070))
    if not chinese:
        wanted |= set(range(0x600,0x900)) | set(range(0xfb50,0xfe00)) | set(range(0xfe70,0xff00))
    options = subset.Options()
    options.layout_features = ['*']
    options.name_IDs = ['*']
    options.name_languages = ['*']
    options.notdef_glyph = True
    options.notdef_outline = True
    options.recalc_timestamp = False
    options.hinting = True
    subsetter = subset.Subsetter(options=options)
    subsetter.populate(unicodes=wanted)
    subsetter.subset(font)
    locations = {'wght':400}
    if any(axis.axisTag=='wdth' for axis in font['fvar'].axes):
        locations['wdth'] = 100
    font = instantiateVariableFont(font, locations, inplace=True, optimize=True)
    rename(font, 'Pitchmap UI CJK' if chinese else 'Pitchmap UI Arabic')
    font.save(target, reorderTables=True)
    cmap = TTFont(target).getBestCmap()
    required = {ord(c) for c in text if ((0x3000<=ord(c)<=0x9fff or 0xff00<=ord(c)<=0xffef) if chinese else (0x600<=ord(c)<=0x8ff or 0xfb50<=ord(c)<=0xfdff or 0xfe70<=ord(c)<=0xfeff)) and not unicodedata.category(c).startswith('C')}
    missing = sorted(required - cmap.keys())
    assert not missing, (target.name, [hex(c) for c in missing])
    return dict(file=target.name, bytes=target.stat().st_size,sha256=sha(target),glyph_count=len(font.getGlyphOrder()),required_script_codepoints=len(required),missing_script_codepoints=missing)


def non_latin_script(c):
    value=ord(c)
    return (0x600<=value<=0x8ff or 0xfb50<=value<=0xfdff or 0xfe70<=value<=0xfeff
            or 0x3000<=value<=0x9fff or 0xff00<=value<=0xffef)


def build_latin(source,target,text):
    font=TTFont(source,recalcTimestamp=False)
    required={ord(c) for c in text if not non_latin_script(c) and not unicodedata.category(c).startswith('C')}
    required|={n for n in range(0x20,0x250) if not unicodedata.category(chr(n)).startswith('C')}
    required|={0xb0,0x2026,0x2192,0x2212,0x221e}
    options=subset.Options();options.layout_features=['*'];options.name_IDs=['*'];options.name_languages=['*']
    options.notdef_glyph=True;options.notdef_outline=True;options.recalc_timestamp=False;options.hinting=True
    subsetter=subset.Subsetter(options=options);subsetter.populate(unicodes=required);subsetter.subset(font)
    font=instantiateVariableFont(font,{'wght':400,'wdth':100},inplace=True,optimize=True)
    # Noto Sans omits these conventional UI symbols. Reuse their OFL-licensed
    # outlines from the unchanged embedded Noto Sans SC subset (same 1000 UPM).
    donor=TTFont(ROOT/'ChineseUI.ttf',recalcTimestamp=False)
    assert donor['head'].unitsPerEm==font['head'].unitsPerEm
    cmap=font.getBestCmap();donor_cmap=donor.getBestCmap();glyph_order=list(font.getGlyphOrder())
    for codepoint in [0x2192,0x221e]:
        if codepoint in cmap:continue
        name=donor_cmap[codepoint];glyph=copy.deepcopy(donor['glyf'][name]);assert not glyph.isComposite()
        new_name='pitchmapSymbol'+str(codepoint)
        font['glyf'][new_name]=glyph;font['hmtx'].metrics[new_name]=donor['hmtx'].metrics[name];glyph_order.append(new_name)
        for table in font['cmap'].tables:
            if table.isUnicode():table.cmap[codepoint]=new_name
    font.setGlyphOrder(glyph_order)
    rename(font,'Pitchmap UI Latin');temporary=target.with_suffix('.tmp.ttf');font.save(temporary,reorderTables=True);temporary.replace(target)
    cmap=TTFont(target).getBestCmap();missing=sorted(required-cmap.keys())
    assert not missing,(target.name,[hex(c) for c in missing])
    return dict(file=target.name,bytes=target.stat().st_size,sha256=sha(target),glyph_count=len(font.getGlyphOrder()),required_script_codepoints=len(required),missing_script_codepoints=missing)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-dir',type=Path,required=True)
    parser.add_argument('--catalog',type=Path,default=ROOT.parents[1]/'Source/Localization.cpp')
    parser.add_argument('--download',action='store_true')
    parser.add_argument('--only-latin',action='store_true',help='Add the Latin asset while preserving existing Chinese/Arabic binary hashes')
    args=parser.parse_args();args.source_dir.mkdir(parents=True,exist_ok=True)
    records=[]
    for name,url,digest in SOURCES:
        path=args.source_dir/name
        if not path.exists() and args.download:
            with urllib.request.urlopen(url,timeout=60) as response:path.write_bytes(response.read())
        assert path.is_file(), 'Missing original; use --download: '+str(path)
        assert sha(path)==digest, 'Original changed; audit a new revision before regenerating: '+name
        records.append(dict(file=name,url=url,bytes=path.stat().st_size,sha256=digest))
    text=args.catalog.read_text()+EXTRA
    if args.only_latin:
        previous=json.loads((ROOT/'PROVENANCE.json').read_text())
        results=[item for item in previous['subsets'] if item['file'] in ['ChineseUI.ttf','ArabicUI.ttf']]
        assert len(results)==2
        for item in results:assert sha(ROOT/item['file'])==item['sha256']
    else:
        results=[build(args.source_dir/'NotoSansSC.ttf',ROOT/'ChineseUI.ttf',text,True),build(args.source_dir/'NotoSansArabic.ttf',ROOT/'ArabicUI.ttf',text,False)]
    results.append(build_latin(args.source_dir/'NotoSans.ttf',ROOT/'LatinUI.ttf',text))
    for name in ['Chinese-OFL.txt','Arabic-OFL.txt','Latin-OFL.txt']:(ROOT/name).write_bytes((args.source_dir/name).read_bytes())
    result=dict(retrieved_on='2026-10-23',fonttools_version='4.61.1',catalog_sha256=sha(args.catalog),sources=records,subsets=results,layout_features_preserved='all',weight=400,arabic_width=100,latin_width=100,latin_symbol_supplement={'donor':'ChineseUI.ttf','codepoints':['U+2192','U+221E']})
    (ROOT/'PROVENANCE.json').write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n')
    print(json.dumps(result,indent=2,ensure_ascii=False))


if __name__=='__main__':main()
