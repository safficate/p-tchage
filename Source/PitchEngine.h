#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <functional>
#include <map>

namespace mp {
constexpr double maxSeconds = 600.0;
constexpr int64_t maxAudioBytes = 240LL * 1024 * 1024;
constexpr int stateVersion = 4;
struct Frame { double time = 0, midi = 0, confidence = 0, rms = 0; double uncertainty = 0, onset = 0, agreement = 1; };
struct Note {
    double start = 0, end = 0, centre = 69, confidence = 0;
    int key = 69;
    bool detached = false;
    double target = 0, strength = 1;
    double stableStart = 0, stableEnd = 0, uncertainty = 0;
    bool discrete = false, overlap = false;
};
struct Settings {
    std::array<double, 12> cents {};
    std::array<int, 12> semitones {};
    bool adaptive = false, minimumIntervention = true;
    double precisionCents = 1;
    double globalCents = 0;
    bool directShift = false;
    bool matchReference = false;
    std::array<double, 12> groupStrength {1,1,1,1,1,1,1,1,1,1,1,1};
    std::map<int, double> octaveCents;
    bool octaveMode = false, formants = true;
    double strength = 0, tightness = 0, vibrato = 1, smoothingMs = 30;
    double humanize = 0, reference = 440;
    int seed = 1;
};
struct Clip {
    juce::AudioBuffer<float> audio;
    double sampleRate = 48000, anchorSeconds = 0;
    juce::String name;
    std::vector<Frame> frames;
    std::vector<Note> notes;
    bool adaptiveAnalysis = false;
};
using Progress = std::function<bool(double)>;
double hzToMidi(double hz);
double midiToHz(double midi);
juce::String noteName(int key);
std::pair<double, double> detectPitch(const float* data, int size, double sampleRate);
void analyse(Clip&, const Progress& progress = {});
double correctionAt(const Clip&, const Settings&, double seconds);
juce::AudioBuffer<float> render(const Clip&, const Settings&, const Progress& progress = {});
std::shared_ptr<Clip> loadAudio(const juce::File&, juce::String& error);
bool writeWav(const juce::File&, const juce::AudioBuffer<float>&, double sampleRate);
bool writeMidi(const juce::File&, const Clip&, const Settings&);
bool importScala(const juce::File&, Settings&, juce::String& error);
juce::ValueTree clipToTree(const Clip&);
std::shared_ptr<Clip> clipFromTree(const juce::ValueTree&);
juce::ValueTree settingsToTree(const Settings&);
Settings settingsFromTree(const juce::ValueTree&);
}
