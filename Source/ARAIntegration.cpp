#include "ARAIntegration.h"
#include "PluginProcessor.h"

#if JucePlugin_Enable_ARA
namespace {
class PitchARADocument final : public juce::ARADocumentControllerSpecialisation {
public:
    using ARADocumentControllerSpecialisation::ARADocumentControllerSpecialisation;
    std::atomic<uint64_t> revision{1};
    std::atomic<bool> editing{false};
    std::map<std::string, juce::MemoryBlock> restoredStates;
    juce::ARAAudioSourceReader* readerFor(juce::ARAAudioSource* source) {
        auto it = readers.find(source); return it != readers.end() ? it->second.get() : nullptr;
    }
protected:
    void willBeginEditing(juce::ARADocument*) override { editing = true; }
    void didEndEditing(juce::ARADocument*) override {
        // ARA permits creating host audio readers only during source management
        // callbacks or endEditing(), never from an arbitrary UI timer.
        for (auto* source : getDocument()->getAudioSources()) if (source->isSampleAccessEnabled()) {
            auto& reader = readers[source];
            if (!reader || !reader->isValid()) reader = std::make_unique<juce::ARAAudioSourceReader>(source);
        }
        ++revision; editing = false;
    }
    void didEnableAudioSourceSamplesAccess(juce::ARAAudioSource* source, bool enable) override {
        if (enable && !readers.count(source)) readers[source] = std::make_unique<juce::ARAAudioSourceReader>(source);
        ++revision;
    }
    void willEnableAudioSourceSamplesAccess(juce::ARAAudioSource* source, bool enable) override { if (!enable) readers.erase(source); }
    void willDestroyAudioSource(juce::ARAAudioSource* source) override { readers.erase(source); ++revision; }
    juce::ARAPlaybackRenderer* doCreatePlaybackRenderer() noexcept override { return new PitchARAPlaybackRenderer(getDocumentController()); }
    bool doRestoreObjectsFromStream(juce::ARAInputStream& input, const juce::ARARestoreObjectsFilter* filter) override {
        if (input.readInt() != 2 || input.getTotalLength() > 1024LL * 1024 * 1024) return false;
        const auto root = juce::ValueTree::readFromStream(input);
        if (!root.hasType("ARAStates") || input.failed()) return false;
        for (auto entry : root) {
            const auto id = entry["id"].toString();
            auto* modification = filter->getAudioModificationToRestoreStateWithID(id.toRawUTF8());
            const auto* state = entry["state"].getBinaryData();
            if (modification && state && state->getSize() <= 256 * 1024 * 1024) restoredStates[modification->getPersistentID()] = *state;
        }
        return true;
    }
    bool doStoreObjectsToStream(juce::ARAOutputStream& output, const juce::ARAStoreObjectsFilter* filter) override {
        juce::ValueTree root("ARAStates");
        for (const auto* modification : filter->getAudioModificationsToStore()) {
            juce::MemoryBlock state;
            for (auto* renderer : getDocumentController()->getPlaybackRenderers<PitchARAPlaybackRenderer>()) {
                for (auto* region : renderer->getPlaybackRegions()) if (region->getAudioModification() == modification) { state = renderer->archiveState(); break; }
                if (state.getSize() > 0) break;
            }
            if (state.getSize() == 0) if (auto it = restoredStates.find(modification->getPersistentID()); it != restoredStates.end()) state = it->second;
            if (state.getSize() > 0) { juce::ValueTree entry("State"); entry.setProperty("id", juce::String(modification->getPersistentID()), nullptr); entry.setProperty("state", juce::var(state), nullptr); root.addChild(entry, -1, nullptr); }
        }
        if (!output.writeInt(2)) return false;
        juce::MemoryOutputStream encoded; root.writeToStream(encoded);
        return output.write(encoded.getData(), encoded.getDataSize());
    }
private:
    std::map<juce::ARAAudioSource*, std::unique_ptr<juce::ARAAudioSourceReader>> readers;
};
}
const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory() {
    return juce::ARADocumentControllerSpecialisation::createARAFactory<PitchARADocument>();
}
void PitchARAPlaybackRenderer::prepareToPlay(double sr, int, int, juce::AudioProcessor::ProcessingPrecision, AlwaysNonRealtime) { rate = sr; }
void PitchARAPlaybackRenderer::connectProcessor(PitchProcessor& p) {
    processor = &p;
    auto* controller = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<PitchARADocument>(getDocumentController());
    for (auto* region : getPlaybackRegions()) {
        const auto id = region->getAudioModification()->getPersistentID();
        if (auto it = controller->restoredStates.find(id); it != controller->restoredStates.end()) {
            auto state = std::move(it->second); controller->restoredStates.erase(it);
            p.setStateInformation(state.getData(), int(state.getSize())); break;
        }
    }
}
juce::MemoryBlock PitchARAPlaybackRenderer::archiveState() const {
    juce::MemoryBlock result; if (processor) processor->getStateInformation(result); return result;
}
std::shared_ptr<mp::Clip> PitchARAPlaybackRenderer::readHostClips(juce::String& error) {
    auto* controller = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<PitchARADocument>(getDocumentController());
    if (controller->editing) return {};
    if (getPlaybackRegions().empty()) { raw = nullptr; fingerprint.clear(); return {}; }
    juce::String nextFingerprint = juce::String(controller->revision.load()) + ":" + juce::String(rate.load(), 0) + ":";
    double begin = std::numeric_limits<double>::max(), end = std::numeric_limits<double>::lowest();
    for (auto* region : getPlaybackRegions()) {
        auto* source = region->getAudioModification()->getAudioSource();
        if (!source->isSampleAccessEnabled()) { error = "ARA: Sonar klip erisimini henuz acmadi."; return {}; }
        if (std::abs(region->getDurationInAudioModificationTime() - region->getDurationInPlaybackTime()) > 1e-6) {
            error = "ARA: Once zaman esnetilmis klibi Sonar'da bounce edin."; return {};
        }
        begin = std::min(begin, region->getStartInPlaybackTime()); end = std::max(end, region->getStartInPlaybackTime() + region->getDurationInPlaybackTime());
        nextFingerprint += juce::String::toHexString(juce::pointer_sized_int(region)) + ":" + juce::String(region->getStartInPlaybackTime(), 8) + ":" + juce::String(region->getDurationInPlaybackTime(), 8) + ":" + juce::String(region->getStartInAudioModificationTime(), 8) + ";";
    }
    if (fingerprint == nextFingerprint) return {};
    if (end <= begin || end - begin > mp::maxSeconds || (end - begin) * rate.load() * 8 > mp::maxAudioBytes) { error = "ARA: Klip araligi en fazla 600 saniye / 240 MiB PCM olabilir."; return {}; }
    const double sr = rate.load();
    auto clip = std::make_shared<mp::Clip>(); clip->sampleRate = sr; clip->anchorSeconds = begin; clip->name = "Sonar ARA clips";
    clip->audio.setSize(2, int(std::ceil((end - begin) * sr))); clip->audio.clear();
    for (auto* region : getPlaybackRegions()) {
        auto* source = region->getAudioModification()->getAudioSource();
        if (source->getChannelCount() < 1 || source->getChannelCount() > 2) { error = "ARA: Mono veya stereo klip kullanin."; return {}; }
        auto* reader = controller->readerFor(source);
        if (!reader || !reader->isValid()) { error = "ARA: Klip okuyucusu icin Sonar erisimi bekleniyor."; return {}; }
        const auto sourceStart = int64_t(std::llround(region->getStartInAudioModificationTime() * source->getSampleRate()));
        const auto sourceCount = int(std::ceil(region->getDurationInAudioModificationTime() * source->getSampleRate())) + 4;
        juce::AudioBuffer<float> input(source->getChannelCount(), sourceCount);
        if (!reader->read(&input, 0, sourceCount, sourceStart, true, true)) { error = "ARA: Sonar'dan klip okunamadi; erisim acildiginda tekrar denenecek."; return {}; }
        const int destinationStart = int(std::llround((region->getStartInPlaybackTime() - begin) * sr));
        const int count = std::min(int(std::llround(region->getDurationInPlaybackTime() * sr)), clip->audio.getNumSamples() - destinationStart);
        const double ratio = source->getSampleRate() / sr;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < count; ++i) {
            const double at = i * ratio; const int a = int(at), b = std::min(a + 1, sourceCount - 1);
            const auto* samples = input.getReadPointer(std::min(ch, input.getNumChannels() - 1));
            clip->audio.addSample(ch, destinationStart + i, samples[a] + float(at - a) * (samples[b] - samples[a]));
        }
    }
    retained.push_back(clip); raw.store(clip.get()); const auto* active = hazard.load();
    std::erase_if(retained, [&](const auto& item) { return item.get() != clip.get() && item.get() != active; });
    fingerprint = nextFingerprint;
    return clip;
}
bool PitchARAPlaybackRenderer::processBlock(juce::AudioBuffer<float>& buffer, juce::AudioProcessor::Realtime, const juce::AudioPlayHead::PositionInfo& position) noexcept {
    buffer.clear(); mp::Clip* clip;
    do { clip = raw.load(); hazard.store(clip); } while (clip != raw.load());
    if (clip && position.getIsPlaying()) {
        const auto start = position.getTimeInSeconds().orFallback(double(position.getTimeInSamples().orFallback(0)) / rate.load());
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            const double at = (start - clip->anchorSeconds + i / rate.load()) * clip->sampleRate;
            if (at < 0 || at >= clip->audio.getNumSamples()) continue;
            const int a = int(at), b = std::min(a + 1, clip->audio.getNumSamples() - 1);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                const auto* samples = clip->audio.getReadPointer(std::min(ch, clip->audio.getNumChannels() - 1));
                buffer.setSample(ch, i, samples[a] + float(at - a) * (samples[b] - samples[a]));
            }
        }
    }
    hazard.store(nullptr); return true;
}
void PitchARAPlaybackRenderer::notifyRendered() {
    for (auto* region : getPlaybackRegions()) region->notifyContentChanged(juce::ARAContentUpdateScopes::samplesAreAffected(), true);
}
#endif
