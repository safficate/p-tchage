#pragma once
#include "PluginProcessor.h"

class LiveEditor final : public juce::AudioProcessorEditor {
public:
    explicit LiveEditor(PitchProcessor&);
    ~LiveEditor()override;
    void resized()override;
    void applyScale(float);
private:
    struct Page;
    juce::Viewport viewport;
    juce::Component extent;
    std::unique_ptr<Page> page;
};
