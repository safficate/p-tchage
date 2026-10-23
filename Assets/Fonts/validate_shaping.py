#!/usr/bin/env python3
"""Validate embedded Arabic glyph shaping using the system HarfBuzz C API."""
import argparse
import ctypes
import ctypes.util
import json
from pathlib import Path
from fontTools.ttLib import TTFont

ROOT=Path(__file__).resolve().parent


def validate(original=None):
    hb=ctypes.CDLL(ctypes.util.find_library('harfbuzz') or 'libharfbuzz.so.0')
    ptr=ctypes.c_void_p;uint=ctypes.c_uint
    hb.hb_blob_create.argtypes=[ctypes.c_char_p,uint,ctypes.c_int,ptr,ptr];hb.hb_blob_create.restype=ptr
    hb.hb_face_create.argtypes=[ptr,uint];hb.hb_face_create.restype=ptr
    hb.hb_font_create.argtypes=[ptr];hb.hb_font_create.restype=ptr
    hb.hb_ot_font_set_funcs.argtypes=[ptr]
    hb.hb_buffer_create.restype=ptr
    hb.hb_buffer_add_utf8.argtypes=[ptr,ctypes.c_char_p,ctypes.c_int,uint,ctypes.c_int]
    hb.hb_buffer_guess_segment_properties.argtypes=[ptr]
    hb.hb_shape.argtypes=[ptr,ptr,ptr,uint]
    hb.hb_buffer_get_direction.argtypes=[ptr];hb.hb_buffer_get_direction.restype=ctypes.c_int
    class Info(ctypes.Structure):
        _fields_=[('codepoint',uint),('mask',uint),('cluster',uint),('var1',uint),('var2',uint)]
    hb.hb_buffer_get_glyph_infos.argtypes=[ptr,ctypes.POINTER(uint)];hb.hb_buffer_get_glyph_infos.restype=ctypes.POINTER(Info)
    class Position(ctypes.Structure):
        _fields_=[('x_advance',ctypes.c_int),('y_advance',ctypes.c_int),('x_offset',ctypes.c_int),('y_offset',ctypes.c_int),('var',uint)]
    hb.hb_buffer_get_glyph_positions.argtypes=[ptr,ctypes.POINTER(uint)];hb.hb_buffer_get_glyph_positions.restype=ctypes.POINTER(Position)
    for name in ['hb_buffer_destroy','hb_font_destroy','hb_face_destroy','hb_blob_destroy']:getattr(hb,name).argtypes=[ptr]
    data=(ROOT/'ArabicUI.ttf').read_bytes();blob=hb.hb_blob_create(data,len(data),0,None,None);face=hb.hb_face_create(blob,0);font=hb.hb_font_create(face);hb.hb_ot_font_set_funcs(font)
    tt=TTFont(ROOT/'ArabicUI.ttf');cmap=tt.getBestCmap();results=[]
    try:
        for text in ['العربية','سلام','لا','التصحيح الطبيعي']:
            buffer=hb.hb_buffer_create();raw=text.encode('utf-8');hb.hb_buffer_add_utf8(buffer,raw,len(raw),0,len(raw));hb.hb_buffer_guess_segment_properties(buffer);hb.hb_shape(font,buffer,None,0)
            length=uint();info=hb.hb_buffer_get_glyph_infos(buffer,ctypes.byref(length));glyphs=[info[i].codepoint for i in range(length.value)];direction=hb.hb_buffer_get_direction(buffer)
            nominal=[tt.getGlyphID(cmap[ord(c)]) for c in text]
            assert all(glyphs),(text,'Missing shaped glyph')
            assert direction==5,(text,'Expected right-to-left direction')
            assert glyphs!=nominal[::-1],(text,'No contextual substitution was applied')
            positions=hb.hb_buffer_get_glyph_positions(buffer,ctypes.byref(length));metrics=[(positions[i].x_advance,positions[i].y_advance,positions[i].x_offset,positions[i].y_offset) for i in range(length.value)]
            if original:
                reference=original.read_bytes();rb=hb.hb_blob_create(reference,len(reference),0,None,None);rf=hb.hb_face_create(rb,0);rfont=hb.hb_font_create(rf);hb.hb_ot_font_set_funcs(rfont);buf=hb.hb_buffer_create();hb.hb_buffer_add_utf8(buf,raw,len(raw),0,len(raw));hb.hb_buffer_guess_segment_properties(buf);hb.hb_shape(rfont,buf,None,0);count=uint();rp=hb.hb_buffer_get_glyph_positions(buf,ctypes.byref(count));rmetrics=[(rp[i].x_advance,rp[i].y_advance,rp[i].x_offset,rp[i].y_offset) for i in range(count.value)];assert metrics==rmetrics,(text,'Subset changed Arabic shaping positions');hb.hb_buffer_destroy(buf);hb.hb_font_destroy(rfont);hb.hb_face_destroy(rf);hb.hb_blob_destroy(rb)
            results.append(dict(text=text,direction='RTL',input_codepoints=len(text),shaped_glyphs=glyphs,contextual_substitution=True,original_position_match=bool(original)))
            hb.hb_buffer_destroy(buffer)
    finally:
        hb.hb_font_destroy(font);hb.hb_face_destroy(face);hb.hb_blob_destroy(blob)
    report=dict(backend='System HarfBuzz C API',font='ArabicUI.ttf',checks=results,limitations='Font shaping test only; actual plugin painting is verified separately.')
    (ROOT/'SHAPING-VALIDATION.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
    print(json.dumps(report,indent=2,ensure_ascii=False))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--original',type=Path);args=parser.parse_args();validate(args.original)
