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
        bool directShift = true;
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
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::atomic<float> midi{0}, cents{0}, level{0};
};
}
