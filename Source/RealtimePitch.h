#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <memory>

namespace mp {
// A bounded streaming path, independent of the editor, message loop and transport.
// All DSP storage is allocated in prepare(), never in process().
class RealtimePitch {
public:
    struct Controls {
        std::array<float, 12> cents{}, groupStrength{};
        std::array<int, 12> semitones{};
        bool modernTargets=false, adaptiveTransitions=false, fixedSourceCentres=false;
        std::array<float,12> learnedSource{},targets{},relativeCents{};
        std::array<bool,12> sourceKnown{},targetActive{};
        double regionCents=0;
        bool pitchSmooth=true,pitchAuto=true,amountAuto=false,natural=true;
        float amount=1,centreMs=250,glideMs=80,maxRate=1200,centreTightness=0;
        float vibratoKeep=1;
        bool humanOn=false,humanAuto=true;float humanDepth=2,humanTime=700;double timelineSeconds=0;
        int expression=0;
        bool directShift = true;
        double globalCents = 0;
        bool matchReference = false, analysisOnly = false, preserveOctave = true;
        int followReference = 0;
        float referenceMidi = 0;
        std::array<float, 128> octaveCents{};
        float strength = 0, tightness = 0, vibrato = 1, smoothingMs = 30;
        float humanize = 0, reference = 440, wet = 1;
        int seed = 1;
        bool octaveMode = false, formants = true, dry = false;
    };
    RealtimePitch();
    ~RealtimePitch();
    void prepare(double sampleRate, int channels);
    void reset();
    void process(juce::AudioBuffer<float>&, const Controls&);
    void processDelay(juce::AudioBuffer<float>&);
    int latency() const;
    float detectedMidi() const { return midi.load(std::memory_order_relaxed); }
    float appliedCents() const { return cents.load(std::memory_order_relaxed); }
    float inputLevel() const { return level.load(std::memory_order_relaxed); }
    float detectedCentre() const {return centre.load(std::memory_order_relaxed);}
    float confidence() const {return quality.load(std::memory_order_relaxed);}
    void clearAnalysisCache(){clearCache=true;}
    bool reusedAnalysis()const{return cacheHit.load();}
    float smoothingTimeMs() const {return smoothingTime.load();}
    float effectiveAmount() const {return amountApplied.load();}
    uint64_t analysisSerial() const {return serial.load(std::memory_order_relaxed);}
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::atomic<float> midi{0}, cents{0}, level{0}, centre{0}, quality{0},smoothingTime{0},amountApplied{1};
    std::atomic<uint64_t> serial{0};
    std::atomic<bool> clearCache{false},cacheHit{false};
};
}
