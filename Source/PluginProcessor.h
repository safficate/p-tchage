#pragma once
#include "PitchEngine.h"
#include <juce_audio_utils/juce_audio_utils.h>

class PitchProcessor final : public juce::AudioProcessor, private juce::Timer {
public:
    PitchProcessor();
    ~PitchProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Microtonal Pitch Map Pro"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState parameters;
    mp::Settings settings() const;
    std::shared_ptr<const mp::Clip> clip() const;
    void load(const juce::File&);
    void analyse();
    void render();
    void capture(bool start);
    void preview(bool start);
    void editNotes(std::vector<mp::Note>);
    void undo(bool redo);
    void importScale(const juce::File&);
    void exportAudio(const juce::File&);
    void exportMidi(const juce::File&);
    void preset(const juce::File&, bool save);
    void setParameter(const juce::String&, float);
    juce::String status() const;
    std::atomic<bool> busy{false}, capturing{false}, previewing{false};
    std::atomic<double> progress{0};
    std::atomic<int> revision{0};

private:
    struct Rendered { juce::AudioBuffer<float> audio; std::shared_ptr<const mp::Clip> source; double rate = 48000, anchor = 0; };
    std::atomic<Rendered*> current{nullptr}, hazard{nullptr};
    std::vector<std::unique_ptr<Rendered>> retained;
    std::atomic<bool> inCallback{false};
    juce::AudioBuffer<float> captureBuffer;
    std::atomic<int> captured{0};
    std::atomic<bool> captureFull{false};
    std::atomic<double> captureAnchor{0};
    double runtimeRate = 48000;
    std::atomic<int64_t> previewPosition{0};
    mutable juce::CriticalSection modelLock, statusLock;
    std::shared_ptr<mp::Clip> model;
    juce::String message = "Load WAV or capture a Sonar track. Quantize defaults to 0%.";
    std::vector<std::vector<mp::Note>> history, future;
    juce::ThreadPool worker{1};
    mp::Settings lastRenderedSettings;
    juce::String settingsFingerprint;
    std::atomic<bool> needsRender{false};
    void setStatus(juce::String);
    void install(std::shared_ptr<mp::Clip>);
    void publish(std::unique_ptr<Rendered>);
    void timerCallback() override;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchProcessor)
};
