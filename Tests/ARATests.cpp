#include "PluginProcessor.h"
#include "ARAIntegration.h"
#include <iostream>
#include <stdexcept>
#include <cstring>

using namespace ARA;
extern const ARAFactory* JUCE_CALLTYPE createARAFactory();
static int assertions = 0;
static void check(bool ok, const char* text) { ++assertions; if (!ok) throw std::runtime_error(text); }
static void ARA_CALL araAssertion(ARAAssertCategory, const void*, const char* diagnosis) {
    std::cerr << "ARA protocol assertion: " << diagnosis << '\n'; std::abort();
}
static ARAAssertFunction assertFunction = araAssertion;
struct Host {
    struct Reader { Host* host; bool use64; };
    double rate;
    std::vector<float> samples;
    std::vector<ARAByte> archive;
    bool legalReaderManagement = false, inProcess = false, badReaderCall = false, realtimeRead = false;
    int reads = 0, readers = 0;
    const ARADocumentControllerInterface* api = nullptr;
    ARADocumentControllerRef doc = nullptr;
    ARAMusicalContextRef context = nullptr;
    ARARegionSequenceRef sequence = nullptr;
    ARAAudioSourceRef source = nullptr;
    ARAAudioModificationRef modification = nullptr;
    ARAPlaybackRegionRef region = nullptr;
    ARAAudioAccessControllerInterface audio{};
    ARAArchivingControllerInterface archiving{};
    ARADocumentControllerHostInstance host{};
    static Host& audioHost(ARAAudioAccessControllerHostRef ref) { return *reinterpret_cast<Host*>(ref); }
    static Host& archiveHost(ARAArchivingControllerHostRef ref) { return *reinterpret_cast<Host*>(ref); }
    explicit Host(double sr) : rate(sr), samples(size_t(sr * 2)) {
        for (size_t i = 0; i < samples.size(); ++i) samples[i] = float(0.2 * std::sin(2 * juce::MathConstants<double>::pi * mp::midiToHz(69.1) * i / rate));
        audio.structSize = sizeof(audio);
        audio.createAudioReaderForSource = [](ARAAudioAccessControllerHostRef ref, ARAAudioSourceHostRef, ARABool use64) -> ARAAudioReaderHostRef {
            auto& h = audioHost(ref); if (!h.legalReaderManagement) h.badReaderCall = true; ++h.readers;
            return reinterpret_cast<ARAAudioReaderHostRef>(new Reader{&h, use64 != 0});
        };
        audio.readAudioSamples = [](ARAAudioAccessControllerHostRef, ARAAudioReaderHostRef ref, ARASamplePosition start, ARASampleCount count, void* const buffers[]) -> ARABool {
            auto& reader = *reinterpret_cast<Reader*>(ref); auto& h = *reader.host; ++h.reads; h.realtimeRead |= h.inProcess;
            for (ARASampleCount i = 0; i < count; ++i) {
                const auto at = start + i; const float sample = at >= 0 && at < ARASamplePosition(h.samples.size()) ? h.samples[size_t(at)] : 0;
                if (reader.use64) static_cast<double*>(buffers[0])[i] = sample; else static_cast<float*>(buffers[0])[i] = sample;
            }
            return kARATrue;
        };
        audio.destroyAudioReader = [](ARAAudioAccessControllerHostRef ref, ARAAudioReaderHostRef reader) {
            auto& h = audioHost(ref); if (!h.legalReaderManagement) h.badReaderCall = true; --h.readers; delete reinterpret_cast<Reader*>(reader);
        };
        archiving.structSize = sizeof(archiving);
        archiving.getArchiveSize = [](ARAArchivingControllerHostRef ref, ARAArchiveReaderHostRef) -> ARASize { return archiveHost(ref).archive.size(); };
        archiving.readBytesFromArchive = [](ARAArchivingControllerHostRef ref, ARAArchiveReaderHostRef, ARASize at, ARASize count, ARAByte buffer[]) -> ARABool {
            auto& h = archiveHost(ref); if (at + count > h.archive.size()) return kARAFalse; std::memcpy(buffer, h.archive.data() + at, count); return kARATrue;
        };
        archiving.writeBytesToArchive = [](ARAArchivingControllerHostRef ref, ARAArchiveWriterHostRef, ARASize at, ARASize count, const ARAByte buffer[]) -> ARABool {
            auto& h = archiveHost(ref); h.archive.resize(std::max(h.archive.size(), size_t(at + count))); std::memcpy(h.archive.data() + at, buffer, count); return kARATrue;
        };
        archiving.notifyDocumentArchivingProgress = archiving.notifyDocumentUnarchivingProgress = [](ARAArchivingControllerHostRef, float) {};
        archiving.getDocumentArchiveID = [](ARAArchivingControllerHostRef, ARAArchiveReaderHostRef) -> ARAPersistentID { return createARAFactory()->documentArchiveID; };
        host.structSize = sizeof(host); host.audioAccessControllerHostRef = reinterpret_cast<ARAAudioAccessControllerHostRef>(this); host.audioAccessControllerInterface = &audio;
        host.archivingControllerHostRef = reinterpret_cast<ARAArchivingControllerHostRef>(this); host.archivingControllerInterface = &archiving;
        ARADocumentProperties documentProperties{sizeof(documentProperties), "Clip transfer test"};
        const auto* instance = createARAFactory()->createDocumentControllerWithDocument(&host, &documentProperties);
        api = instance->documentControllerInterface; doc = instance->documentControllerRef;
        api->beginEditing(doc);
        ARAMusicalContextProperties contextProperties{sizeof(contextProperties), "Song", 0, nullptr};
        context = api->createMusicalContext(doc, reinterpret_cast<ARAMusicalContextHostRef>(this), &contextProperties);
        ARARegionSequenceProperties sequenceProperties{sizeof(sequenceProperties), "Instrument", 0, context, nullptr};
        sequence = api->createRegionSequence(doc, reinterpret_cast<ARARegionSequenceHostRef>(this), &sequenceProperties);
        ARAAudioSourceProperties sourceProperties{sizeof(sourceProperties), "A plus 10 cents", "test-source", ARASampleCount(samples.size()), sr, 1, kARAFalse, kARAChannelArrangementUndefined, nullptr};
        source = api->createAudioSource(doc, reinterpret_cast<ARAAudioSourceHostRef>(this), &sourceProperties);
        ARAAudioModificationProperties modProperties{sizeof(modProperties), "Edited instrument", "test-modification"};
        modification = api->createAudioModification(doc, source, reinterpret_cast<ARAAudioModificationHostRef>(this), &modProperties);
        ARAPlaybackRegionProperties regionProperties{}; regionProperties.structSize = sizeof(regionProperties);
        regionProperties.startInModificationTime = 0.1; regionProperties.durationInModificationTime = 1.8;
        regionProperties.startInPlaybackTime = 1; regionProperties.durationInPlaybackTime = 1.8;
        regionProperties.regionSequenceRef = sequence; regionProperties.name = "Trimmed region";
        region = api->createPlaybackRegion(doc, modification, reinterpret_cast<ARAPlaybackRegionHostRef>(this), &regionProperties);
        legalReaderManagement = true; api->endEditing(doc); api->enableAudioSourceSamplesAccess(doc, source, kARATrue); legalReaderManagement = false;
    }
    ~Host() {
        legalReaderManagement = true;
        api->enableAudioSourceSamplesAccess(doc, source, kARAFalse);
        api->beginEditing(doc); api->destroyPlaybackRegion(doc, region); api->destroyAudioModification(doc, modification); api->destroyAudioSource(doc, source);
        api->destroyRegionSequence(doc, sequence); api->destroyMusicalContext(doc, context); api->endEditing(doc); api->destroyDocumentController(doc);
    }
};
struct PlayHead : juce::AudioPlayHead {
    int64_t sample = 0; double rate = 48000;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setTimeInSamples(sample); p.setTimeInSeconds(sample / rate); p.setIsPlaying(true); return p; }
};
static void waitWorker(PitchProcessor& p) {
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 30000;
    while (p.busy && juce::Time::getMillisecondCounterHiRes() < deadline) juce::Thread::sleep(5);
    check(!p.busy, "ARA offline worker timed out");
}
static void bind(PitchProcessor& p, Host& h) {
    const auto roles = kARAPlaybackRendererRole | kARAEditorRendererRole | kARAEditorViewRole;
    p.bindToARA(h.doc, roles, kARAPlaybackRendererRole | kARAEditorViewRole);
    p.getPlaybackRenderer()->addPlaybackRegion(h.region); p.prepareToPlay(h.rate, 256);
    p.refreshHostClips(); waitWorker(p);
}
static double bounce(PitchProcessor& p, Host& h, bool original) {
    PlayHead play; play.rate = h.rate; p.setPlayHead(&play); p.setParameter("original", original ? 1 : 0);
    juce::AudioBuffer<float> block(2, 256), result(2, int(h.rate)); juce::MidiBuffer midi;
    const int before = h.reads;
    for (int offset = 0; offset < result.getNumSamples(); offset += 256) {
        const int count = std::min(256, result.getNumSamples() - offset); block.setSize(2, count, false, false, true); block.clear();
        play.sample = int64_t(h.rate * 1.3) + offset; h.inProcess = true; p.processBlock(block, midi); h.inProcess = false;
        for (int ch = 0; ch < 2; ++ch) result.copyFrom(ch, offset, block, ch, 0, count);
    }
    check(h.reads == before && !h.realtimeRead, "ARA playback must read preloaded memory, never host audio IO");
    p.setPlayHead(nullptr);
    const auto [hz, confidence] = mp::detectPitch(result.getReadPointer(0, int(h.rate * 0.5)), 4096, h.rate);
    check(hz > 0 && confidence > 0.95, "ARA bounce must produce actual voiced audio without track input");
    return (mp::hzToMidi(hz) - 69) * 100;
}
int main() {
    std::cout << std::unitbuf; juce::ScopedJuceInitialiser_GUI gui;
    const auto* factory = createARAFactory(); ARAInterfaceConfiguration configuration{sizeof(configuration), kARAAPIGeneration_2_0_Final, &assertFunction}; factory->initializeARAWithConfiguration(&configuration);
    try {
        for (double rate : {44100.0, 48000.0}) {
            Host host(rate);
            {
                PitchProcessor p; bind(p, host);
                check(host.reads > 0 && p.clip() && !p.clip()->notes.empty(), "ARA must import and analyze host clips BEFORE any playback or processBlock");
                check(!host.badReaderCall, "Host readers must be created/destroyed only inside ARA source management/endEditing");
                check(p.getLatencySamples() == 0, "Preloaded ARA map rendering must have zero host latency");
                check(std::abs(p.clip()->anchorSeconds - 1) < 1e-9 && p.clip()->audio.getNumSamples() == int(rate * 1.8), "ARA trimmed source must retain its exact DAW timeline position");
                check(std::abs(p.clip()->audio.getSample(0, 0) - host.samples[size_t(rate * 0.1)]) < 1e-6f, "ARA must use the host clip's trimmed source start");
                const double raw = bounce(p, host, true); check(std::abs(raw - 10) < 1, "ARA original audition must preserve source pitch");
                p.setParameter("step9", 2); p.setParameter("cent11", 5); p.applyCorrection(); waitWorker(p);
                const double corrected = bounce(p, host, false);
                std::cout << "ARA rate=" << rate << " source=" << raw << " corrected=" << corrected << " expected=205 cents\n";
                check(std::abs(corrected - 205) < 1, "ARA Bounce must produce mapped B plus 5 cents, without playback capture or Quantize");
                auto notes = p.clip()->notes; notes[0].detached = true; notes[0].target = 212; p.editNotes(notes);
                juce::MemoryBlock state; p.getStateInformation(state);
                check(host.api->storeObjectsToArchive(host.doc, reinterpret_cast<ARAArchiveWriterHostRef>(&host), nullptr), "ARA document archive must store correction state");
                {
                    PitchProcessor restored; restored.setStateInformation(state.getData(), int(state.getSize())); bind(restored, host);
                    check(restored.clip()->notes[0].detached && restored.clip()->notes[0].target == 212, "Host clip refresh must preserve restored manual note edits when source PCM is unchanged");
                    check(restored.parameters.getRawParameterValue("step9")->load() == 2 && restored.parameters.getRawParameterValue("cent11")->load() == 5, "ARA component parameter state must restore note mapping and cents");
                    restored.releaseResources();
                }
                host.api->beginEditing(host.doc);
                check(host.api->restoreObjectsFromArchive(host.doc, reinterpret_cast<ARAArchiveReaderHostRef>(&host), nullptr), "ARA document archive must restore correction state");
                host.legalReaderManagement = true; host.api->endEditing(host.doc); host.legalReaderManagement = false;
                {
                    PitchProcessor fromArchive; bind(fromArchive, host);
                    check(fromArchive.clip()->notes[0].detached && fromArchive.clip()->notes[0].target == 212, "ARA archive alone must restore manual edits without separate component state");
                    check(fromArchive.parameters.getRawParameterValue("cent11")->load() == 5, "ARA archive alone must restore tuned reference cents");
                    fromArchive.releaseResources();
                }
                p.releaseResources();
            }
            check(!host.badReaderCall, "ARA reader lifecycle must remain legal after plugin destruction");
        }
        factory->uninitializeARA(); std::cout << assertions << " ARA transfer, trim, bounce and state assertions passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; factory->uninitializeARA(); return 1; }
}
