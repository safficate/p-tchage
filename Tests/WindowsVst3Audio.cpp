#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Queue final : IParamValueQueue {
    ParamID id{}; ParamValue value{};
    tresult PLUGIN_API queryInterface(const TUID, void** object) override { *object = nullptr; return kNoInterface; }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    ParamID PLUGIN_API getParameterId() override { return id; }
    int32 PLUGIN_API getPointCount() override { return 1; }
    tresult PLUGIN_API getPoint(int32 index, int32& offset, ParamValue& result) override { offset = 0; result = value; return index == 0 ? kResultOk : kInvalidArgument; }
    tresult PLUGIN_API addPoint(int32, ParamValue v, int32& index) override { value = v; index = 0; return kResultOk; }
};
struct Changes final : IParameterChanges {
    std::array<Queue, 8> queues;
    tresult PLUGIN_API queryInterface(const TUID, void** object) override { *object = nullptr; return kNoInterface; }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    int32 PLUGIN_API getParameterCount() override { return int32(queues.size()); }
    IParamValueQueue* PLUGIN_API getParameterData(int32 index) override { return index >= 0 && index < int32(queues.size()) ? &queues[size_t(index)] : nullptr; }
    IParamValueQueue* PLUGIN_API addParameterData(const ParamID&, int32&) override { return nullptr; }
};
static std::string narrow(const TChar* text) {
    std::string result; for (; *text; ++text) result += char(*text); return result;
}
static ParamID find(IEditController* controller, const char* name) {
    for (int32 i = 0; i < controller->getParameterCount(); ++i) {
        ParameterInfo info{};
        if (controller->getParameterInfo(i, info) == kResultOk && narrow(info.title) == name) return info.id;
    }
    throw std::runtime_error(std::string("Missing real VST3 parameter: ") + name);
}
static ParamValue enteredValue(IEditController* controller, ParamID id, double value) {
    // JUCE's VST3 continuous plain units are normalized host units. Engineering
    // units such as cents and signed semitones must use the exposed text parser.
    const auto text = std::to_string(value); String128 utf16{};
    for (size_t i = 0; i < text.size() && i < 127; ++i) utf16[i] = TChar(text[i]);
    ParamValue result{};
    require(controller->getParamValueByString(id, utf16, result) == kResultOk, "Actual VST3 parameter text parsing failed");
    return result;
}
static void audioCase(IPluginFactory* factory, const TUID cid, double rate, double strength, double target, bool offline, bool direct = false, int step = 0, int sourceKey = 69, bool match = false, int follow = 0, double referenceMidi = 56.95) {
    IComponent* component = nullptr;
    require(factory->createInstance(cid, INLINE_UID_OF(IComponent), reinterpret_cast<void**>(&component)) == kResultOk, "VST3 component creation failed");
    require(component->initialize(nullptr) == kResultOk, "VST3 component initialize failed");
    IAudioProcessor* processor = nullptr;
    require(component->queryInterface(INLINE_UID_OF(IAudioProcessor), reinterpret_cast<void**>(&processor)) == kResultOk, "VST3 has no audio processor");
    TUID controllerId{}; require(component->getControllerClassId(controllerId) == kResultOk, "Controller ID missing");
    IEditController* controller = nullptr;
    require(factory->createInstance(controllerId, INLINE_UID_OF(IEditController), reinterpret_cast<void**>(&controller)) == kResultOk, "Controller creation failed");
    require(controller->initialize(nullptr) == kResultOk, "Controller initialize failed");
    IConnectionPoint *componentConnection = nullptr, *controllerConnection = nullptr;
    require(component->queryInterface(INLINE_UID_OF(IConnectionPoint), reinterpret_cast<void**>(&componentConnection)) == kResultOk, "Component connection missing");
    require(controller->queryInterface(INLINE_UID_OF(IConnectionPoint), reinterpret_cast<void**>(&controllerConnection)) == kResultOk, "Controller connection missing");
    componentConnection->connect(controllerConnection); controllerConnection->connect(componentConnection);
    Changes changes;
    static const char* centsNames[] = {"C target cents", "C# target cents", "D target cents", "D# target cents", "E target cents", "F target cents", "F# target cents", "G target cents", "G# target cents", "A target cents", "A# target cents", "B target cents"};
    const int targetGroup = ((sourceKey + (match ? step : 0)) % 12 + 12) % 12;
    changes.queues[0].id = find(controller, centsNames[targetGroup]); changes.queues[0].value = enteredValue(controller, changes.queues[0].id, target);
    changes.queues[1].id = find(controller, "Quantize"); changes.queues[1].value = strength;
    changes.queues[2].id = find(controller, "formants"); changes.queues[2].value = 0;
    changes.queues[3].id = find(controller, "Direct cent and note shifts"); changes.queues[3].value = direct ? 1 : 0;
    changes.queues[4].id = find(controller, sourceKey == 60 ? "C note shift semitones" : "A note shift semitones"); changes.queues[4].value = enteredValue(controller, changes.queues[4].id, step);
    changes.queues[5].id = find(controller, "Correction enabled"); changes.queues[5].value = 1;
    changes.queues[6].id = find(controller, "Match tuned reference tones"); changes.queues[6].value = match ? 1 : 0;
    changes.queues[7].id = find(controller, "Sidechain reference mode"); changes.queues[7].value = enteredValue(controller, changes.queues[7].id, follow);
    SpeakerArrangement arrangement = SpeakerArr::kStereo;
    SpeakerArrangement inputs[2] = {SpeakerArr::kStereo, follow ? SpeakerArr::kStereo : SpeakerArr::kEmpty};
    require(processor->setBusArrangements(inputs, 2, &arrangement, 1) == kResultOk, "Stereo bus arrangement failed");
    component->activateBus(kAudio, kInput, 0, true); component->activateBus(kAudio, kOutput, 0, true);
    if (follow) component->activateBus(kAudio, kInput, 1, true);
    ProcessSetup setup{offline ? kOffline : kRealtime, kSample32, 256, rate};
    require(processor->setupProcessing(setup) == kResultOk, "setupProcessing failed");
    require(component->setActive(true) == kResultOk, "setActive failed");
    processor->setProcessing(true);
    const int latency = int(processor->getLatencySamples()); require(latency > 0, "VST3 must report its actual streaming latency");
    std::array<std::array<float, 256>, 2> input{}, output{};
    float* ip[2] = {input[0].data(), input[1].data()}; float* op[2] = {output[0].data(), output[1].data()};
    AudioBusBuffers ib, ob; ib.numChannels = ob.numChannels = 2; ib.channelBuffers32 = ip; ob.channelBuffers32 = op;
    AudioBusBuffers inputBuses[2]; inputBuses[0] = ib;
    std::array<std::array<float, 256>, 2> reference{};
    float* rp[2] = {reference[0].data(), reference[1].data()};
    if (follow) { inputBuses[1].numChannels = 2; inputBuses[1].channelBuffers32 = rp; }
    ProcessData data; data.processMode = setup.processMode; data.numInputs = 2; data.numOutputs = 1; data.inputs = inputBuses; data.outputs = &ob; data.inputParameterChanges = &changes;
    // Deliberately no editor, captured file, playhead/process context, or message pump.
    const int length = int(rate * 2.2); std::vector<float> result(size_t(length), 0);
    const double baseHz = 440 * std::exp2((sourceKey - 69) / 12.0);
    const double hz = baseHz * std::exp2(10.0 / 1200), pi = std::acos(-1.0);
    for (int position = 0; position < length; position += data.numSamples) {
        data.numSamples = std::min(256, length - position);
        for (int i = 0; i < data.numSamples; ++i) input[0][size_t(i)] = input[1][size_t(i)] = float(0.2 * std::sin(2 * pi * hz * (position + i) / rate));
        for (int i = 0; i < data.numSamples; ++i) reference[0][size_t(i)] = reference[1][size_t(i)] = float(0.2 * std::sin(2 * pi * 440 * std::exp2((referenceMidi - 69) / 12) * (position + i) / rate));
        const auto beforeReference = reference;
        require(processor->process(data) == kResultOk, "Actual VST3 process call failed");
        require(reference == beforeReference, "Actual VST3 must not change sidechain samples");
        std::copy_n(output[0].data(), data.numSamples, result.data() + position);
    }
    double first = 0, last = 0; int crossings = 0;
    for (int i = int(rate * 1.5) + 1; i < length; ++i) {
        require(std::isfinite(result[size_t(i)]), "Non-finite VST3 audio output");
        if (result[size_t(i - 1)] < 0 && result[size_t(i)] >= 0) {
            const double at = i - 1 - result[size_t(i - 1)] / double(result[size_t(i)] - result[size_t(i - 1)]);
            if (crossings++ == 0) first = at; last = at;
        }
    }
    require(crossings > 100 && last > first, "VST3 output has no measurable tone");
    const double measuredHz = (crossings - 1) * rate / (last - first), measuredCents = 1200 * std::log2(measuredHz / baseHz);
    const double expected = follow == 1 ? (referenceMidi - std::round(referenceMidi)) * 100 : follow == 2 ? (referenceMidi + 12 * std::round((sourceKey - referenceMidi) / 12) - sourceKey) * 100 : match ? target + step * 100 : direct ? 10 + target + step * 100 : 10 + strength * (target - 10);
    std::cout << "ACTUAL VST3 rate=" << rate << " direct=" << direct << " strength=" << strength << " fine=" << target << " step=" << step << " expected=" << expected << " measured=" << measuredCents << " cents latency=" << latency << " mode=" << (offline ? "offline" : "realtime") << '\n';
    require(std::abs(measuredCents - expected) < 1, "Actual VST3 output missed entered target by more than 1 cent");
    processor->setProcessing(false); component->setActive(false);
    controllerConnection->disconnect(componentConnection); componentConnection->disconnect(controllerConnection);
    controllerConnection->release(); componentConnection->release(); controller->terminate(); controller->release();
    processor->release(); component->terminate(); component->release();
}
int wmain(int argc, wchar_t** argv) {
    std::cout << std::unitbuf;
    try {
        require(argc == 2, "Pass the real Windows x64 VST3 DLL path");
        auto module = LoadLibraryW(argv[1]); require(module != nullptr, "LoadLibraryW failed");
        using Entry = bool (*)();
        const auto init = reinterpret_cast<Entry>(GetProcAddress(module, "InitDll")); require(!init || init(), "InitDll failed");
        using FactoryFunction = IPluginFactory* (PLUGIN_API*)();
        auto function = reinterpret_cast<FactoryFunction>(GetProcAddress(module, "GetPluginFactory")); require(function != nullptr, "GetPluginFactory missing");
        auto* factory = function(); require(factory != nullptr, "Factory missing");
        bool tested = false;
        for (int i = 0; i < factory->countClasses(); ++i) {
            PClassInfo info{}; factory->getClassInfo(i, &info);
            if (std::strcmp(info.category, "Audio Module Class") != 0) continue;
            for (double rate : {44100.0, 48000.0}) for (double strength : {0.0, 0.5, 1.0}) audioCase(factory, info.cid, rate, strength, -20, false);
            audioCase(factory, info.cid, 48000, 1, 20, true); tested = true;
            for (double rate : {44100.0, 48000.0}) {
                for (double fine : {-3.0, 5.0, 10.0}) audioCase(factory, info.cid, rate, 0, fine, false, true);
                audioCase(factory, info.cid, rate, 0, 0, false, true, 1, 60);
                audioCase(factory, info.cid, rate, 0, 0, false, true, 2, 60);
                for (int step : {0, 1, 2}) audioCase(factory, info.cid, rate, 0, 5, false, false, step, 60, true);
                audioCase(factory, info.cid, rate, 0, 0, false, false, 0, 69, false, 1, 56.95);
                audioCase(factory, info.cid, rate, 0, 0, false, false, 0, 69, false, 1, 22.95);
                audioCase(factory, info.cid, rate, 0, 0, true, false, 0, 69, false, 2, 55.07);
            }
        }
        require(tested, "No VST3 audio effect tested"); factory->release();
        const auto exit = reinterpret_cast<Entry>(GetProcAddress(module, "ExitDll")); require(!exit || exit(), "ExitDll failed"); FreeLibrary(module);
        std::cout << "29 actual Windows VST3 streaming target/shift/reference/sidechain cases passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
