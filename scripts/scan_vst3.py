#!/usr/bin/env python3
"""Load a real Linux VST3 module and inspect its exported Steinberg factory.

This is an ABI/load/metadata smoke check, not a Sonar compatibility test.
"""
import ctypes
import pathlib
import sys

class ClassInfo(ctypes.Structure):
    _fields_ = [("cid", ctypes.c_char * 16), ("cardinality", ctypes.c_int32),
                ("category", ctypes.c_char * 32), ("name", ctypes.c_char * 64)]

def main():
    bundle = pathlib.Path(sys.argv[1]).resolve()
    matches = list(bundle.glob("Contents/x86_64-linux/*.so"))
    if len(matches) != 1:
        raise RuntimeError("Expected exactly one Linux x64 module in the VST3 bundle")
    module = ctypes.CDLL(str(matches[0]))
    module.ModuleEntry.argtypes = [ctypes.c_void_p]
    module.ModuleEntry.restype = ctypes.c_bool
    if not module.ModuleEntry(module._handle):
        raise RuntimeError("VST3 ModuleEntry failed")
    module.GetPluginFactory.restype = ctypes.c_void_p
    factory = module.GetPluginFactory()
    if not factory:
        raise RuntimeError("No Steinberg plugin factory")
    vtable = ctypes.cast(factory, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p)))[0]
    count = ctypes.CFUNCTYPE(ctypes.c_int32, ctypes.c_void_p)(vtable[4])(factory)
    get_info = ctypes.CFUNCTYPE(ctypes.c_int32, ctypes.c_void_p, ctypes.c_int32, ctypes.POINTER(ClassInfo))(vtable[5])
    release = ctypes.CFUNCTYPE(ctypes.c_uint32, ctypes.c_void_p)(vtable[2])
    audio_classes = 0
    try:
        for index in range(count):
            info = ClassInfo()
            if get_info(factory, index, ctypes.byref(info)) != 0:
                raise RuntimeError("Factory getClassInfo failed")
            category, name = info.category.decode(), info.name.decode()
            print(f"{index}: {category}: {name}")
            if category == "Audio Module Class":
                audio_classes += 1
        if audio_classes != 1:
            raise RuntimeError("Expected one real VST3 audio effect")
        print("VST3 dynamic load, module entry and factory scan passed")
    finally:
        release(factory)
        module.ModuleExit()

if __name__ == "__main__":
    main()
