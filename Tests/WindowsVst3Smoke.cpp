#include <windows.h>
#include <iostream>
#include <cstring>
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) { std::cerr << "Pass the actual x64 VST3 module path\n"; return 1; }
    auto module = LoadLibraryW(argv[1]);
    if (!module) { std::cerr << "LoadLibrary failed: " << GetLastError() << '\n'; return 2; }
    using Entry = bool (*)();
    const auto init = reinterpret_cast<Entry>(GetProcAddress(module, "InitDll"));
    const auto exit = reinterpret_cast<Entry>(GetProcAddress(module, "ExitDll"));
    if (init && !init()) return 3;
    using FactoryFn = Steinberg::IPluginFactory* (PLUGIN_API*)();
    auto getFactory = reinterpret_cast<FactoryFn>(GetProcAddress(module, "GetPluginFactory"));
    if (!getFactory) return 4;
    auto* factory = getFactory(); if (!factory) return 5;
    bool foundAudio = false;
    for (int i = 0; i < factory->countClasses(); ++i) {
        Steinberg::PClassInfo info{};
        if (factory->getClassInfo(i, &info) != Steinberg::kResultOk) return 6;
        std::cout << info.category << ": " << info.name << '\n';
        if (std::strcmp(info.category, "Audio Module Class") != 0) continue;
        Steinberg::Vst::IComponent* component = nullptr;
        if (factory->createInstance(info.cid, INLINE_UID_OF(Steinberg::Vst::IComponent), reinterpret_cast<void**>(&component)) != Steinberg::kResultOk || !component) return 7;
        const auto inputs = component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput);
        const auto outputs = component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput);
        Steinberg::Vst::BusInfo bus{};
        const auto result = component->getBusInfo(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput, 0, bus);
        if (inputs < 1 || outputs < 1 || result != Steinberg::kResultOk || bus.channelCount != 2) return 8;
        component->release(); foundAudio = true;
    }
    factory->release();
    if (exit && !exit()) return 9;
    FreeLibrary(module);
    if (!foundAudio) return 10;
    std::cout << "Windows x64 VST3 load, factory, component and stereo bus checks passed\n";
    return 0;
}
