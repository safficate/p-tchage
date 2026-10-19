#pragma once
#include "PitchEngine.h"
#include "RealtimePitch.h"
#include "Audition.h"
#include <juce_audio_utils/juce_audio_utils.h>

class PitchProcessor final : public juce::AudioProcessor,
#if JucePlugin_Enable_ARA
                             public juce::AudioProcessorARAExtension,
#endif
                             private juce::Timer {
public:
    PitchProcessor();
    ~PitchProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override { realtime.reset(); }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Microtonal Pitch Map Pro"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return getLatencySamples() / runtimeRate; }
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
    void applyCorrection();
    void refreshHostClips();
    void auditionTone(int group, int octave) { previewing = false; audition.tone(group, octave); }
    int auditionSource(int group) { previewing = false; const auto n = audition.source(clip(), group); if (n < 0) { audition.stop(); setStatus("Bu nota kaynak klipte bulunmadi; sahte kaynak tonu calinmadi."); } return n; }
    bool hasRender() const { return current.load() != nullptr; }
    void stopAudition() { audition.stop(); previewing = false; }
    bool araConnected() const {
#if JucePlugin_Enable_ARA
        return isBoundToARA();
#else
        return false;
#endif
    }
    float referencePitch() const { return sidechainPitch.load(); }
    struct TracePoint { double time = 0; float source = 0, reference = 0; };
    std::vector<TracePoint> takeTrace();
    juce::String status() const;
    float liveMidi() const { return realtime.detectedMidi(); }
    float liveCents() const { return realtime.appliedCents(); }
    float liveInputLevel() const { return realtime.inputLevel(); }
    std::atomic<bool> busy{false}, capturing{false}, previewing{false};
    std::atomic<double> progress{0};
    std::atomic<int> revision{0};

private:
    mp::RealtimePitch realtime;
    mp::RealtimePitch referenceAnalyzer;
    mp::Audition audition;
    std::atomic<float> sidechainPitch{0};
    juce::AbstractFifo traceFifo{1024};
    std::array<TracePoint, 1024> trace{};
    uint64_t processedSamples = 0;
    std::atomic<bool> renderedNotify{false};
    struct LiveParameters {
        std::array<std::atomic<float>*, 12> cents{}, groups{}, steps{};
        std::array<std::atomic<float>*, 128> octaves{};
        std::atomic<float>* strength = nullptr, *tightness = nullptr, *vibrato = nullptr, *smoothing = nullptr;
        std::atomic<float>* humanize = nullptr, *seed = nullptr, *reference = nullptr, *wet = nullptr;
        std::atomic<float>* octave = nullptr, *formants = nullptr, *bypass = nullptr, *original = nullptr;
        std::atomic<float>* useRender = nullptr, *offset = nullptr;
        std::atomic<float>* direct = nullptr;
        std::atomic<float>* enabled = nullptr, *matchReference = nullptr, *follow = nullptr, *preserveOctave = nullptr;
    } liveParameters;
    void processAudio(juce::AudioBuffer<float>&, juce::MidiBuffer&, bool forceBypass);
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
    juce::String message = "Ust tuslar: referans tonu. Alt tuslar: gercek kaynak nota. Eslestir, sonra Uygula.";
    std::vector<std::vector<mp::Note>> history, future;
    juce::ThreadPool worker{1};
    mp::Settings lastRenderedSettings;
    juce::String settingsFingerprint;
    std::atomic<bool> needsRender{false};
    bool restoredClipPending = false;
    void setStatus(juce::String);
    void install(std::shared_ptr<mp::Clip>);
    void publish(std::unique_ptr<Rendered>);
    void timerCallback() override;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchProcessor)
};
