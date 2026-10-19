#pragma once
#include "PitchEngine.h"
#include <juce_audio_utils/juce_audio_utils.h>

#if JucePlugin_Enable_ARA
class PitchProcessor;
// The host supplies the clip samples through ARA. Nothing here captures playback.
class PitchARAPlaybackRenderer final : public juce::ARAPlaybackRenderer {
public:
    using ARAPlaybackRenderer::ARAPlaybackRenderer;
    void prepareToPlay(double, int, int, juce::AudioProcessor::ProcessingPrecision, AlwaysNonRealtime) override;
    bool processBlock(juce::AudioBuffer<float>&, juce::AudioProcessor::Realtime, const juce::AudioPlayHead::PositionInfo&) noexcept override;
    std::shared_ptr<mp::Clip> readHostClips(juce::String& error);
    void notifyRendered();
    void connectProcessor(PitchProcessor&);
    juce::MemoryBlock archiveState() const;
    bool hasClips() const { return !getPlaybackRegions().empty(); }
private:
    std::atomic<double> rate{48000};
    std::atomic<mp::Clip*> raw{nullptr}, hazard{nullptr};
    std::vector<std::shared_ptr<mp::Clip>> retained;
    juce::String fingerprint;
    PitchProcessor* processor = nullptr;
};
#endif
