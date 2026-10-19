#include "PitchEngine.h"
#include <iostream>
#include <stdexcept>

static int checks = 0;
static void require(bool condition, const char* message) { ++checks; if (!condition) throw std::runtime_error(message); }
static mp::Clip tone(double rate, double midi = 69.1, bool vibrato = false) {
    mp::Clip c; c.sampleRate = rate; c.name = "Generated test tone"; c.audio.setSize(2, int(rate * 2));
    double phase = 0;
    for (int i = 0; i < c.audio.getNumSamples(); ++i) {
        const double m = midi + (vibrato ? 0.25 * std::sin(2 * juce::MathConstants<double>::pi * 5 * i / rate) : 0);
        phase += 2 * juce::MathConstants<double>::pi * mp::midiToHz(m) / rate;
        const float v = float(0.2 * std::sin(phase)); c.audio.setSample(0, i, v); c.audio.setSample(1, i, v);
    }
    return c;
}
static double measure(const juce::AudioBuffer<float>& b, double rate, double seconds = 1) { return mp::hzToMidi(mp::detectPitch(b.getReadPointer(0, int(seconds * rate)), 4096, rate).first); }
int main(int argc, char** argv) {
    try {
        for (double rate : {44100.0, 48000.0}) {
            auto c = tone(rate, 69); mp::analyse(c);
            require(c.notes.size() == 1, "440Hz must produce exactly one note");
            require(std::abs(c.notes[0].centre - 69) < 0.01, "440Hz centre detection within one cent");
            auto detuned = tone(rate); mp::analyse(detuned);
            auto bass = tone(rate, 23 - 0.05); mp::analyse(bass);
            require(bass.notes.size() == 1 && bass.notes[0].key == 23 && std::abs(bass.notes[0].centre - 22.95) < 0.01, "Low five-string bass B0 must be detected within one cent");
            const auto artifactDir = argc > 1 ? juce::File(argv[1]) : juce::File{};
            if (artifactDir != juce::File{}) { artifactDir.createDirectory(); const auto sourceFile = artifactDir.getChildFile(juce::String(int(rate)) + "Hz-source-A-plus10c.wav"); if (sourceFile.existsAsFile()) sourceFile.deleteFile(); require(mp::writeWav(sourceFile, detuned.audio, rate), "Test input WAV artifact"); }
            mp::Settings s; s.smoothingMs = 0; s.formants = false; s.cents[9] = -20;
            for (double strength : {0.0, 0.5, 1.0}) {
                s.strength = strength; const auto audio = mp::render(detuned, s);
                require(audio.getNumSamples() == detuned.audio.getNumSamples(), "Render duration preserved");
                const double expected = 69.1 + strength * (68.8 - 69.1);
                require(std::abs(measure(audio, rate) - expected) < 0.035, "Rendered centre meets quantize formula within 3.5 cents");
                double stereoError = 0;
                for (int i = 0; i < audio.getNumSamples(); ++i) stereoError = std::max(stereoError, std::abs(double(audio.getSample(0, i) - audio.getSample(1, i))));
                std::cout << "rate=" << rate << " strength=" << strength << " measured=" << measure(audio, rate) << " stereo error=" << stereoError << '\n';
                require(stereoError < 1e-5, "Stereo equality preserved");
                if (strength == 0) require(std::memcmp(audio.getReadPointer(0), detuned.audio.getReadPointer(0), size_t(audio.getNumSamples()) * sizeof(float)) == 0, "Zero quantize is bit-exact");
                if (artifactDir != juce::File{}) { const auto dest = artifactDir.getChildFile(juce::String(int(rate)) + "Hz-target-minus20c-quantize-" + juce::String(int(strength * 100)) + ".wav"); if (dest.existsAsFile()) dest.deleteFile(); require(mp::writeWav(dest, audio, rate), "Actual corrected WAV artifact"); }
            }
            s.strength = 1; s.cents[9] = 20;
            auto up = mp::render(c, s); std::cout << "Positive target measured=" << measure(up, rate) << '\n'; require(std::abs(measure(up, rate) - 69.2) < 0.035, "Positive 20 cent correction");
            for (bool relative : {false, true}) for (double fine : {-5.0, 3.0, 5.0, 10.0}) {
                mp::Settings fineSettings; fineSettings.directShift = relative; fineSettings.matchReference = !relative; fineSettings.cents[9] = fine;
                const auto fineAudio = mp::render(detuned, fineSettings);
                const double measured = (measure(fineAudio, rate) - 69) * 100, expected = fine + (relative ? 10 : 0);
                std::cout << "OFFLINE rate=" << rate << " relative=" << relative << " entered=" << fine << " measured=" << measured << " expected=" << expected << " cents\n";
                require(std::abs(measured - expected) < 1, "Offline fine-cent reference/relative correction must be within one cent without Quantize");
            }
            // Every instance of the same pitch class shares its group, including another octave.
            c.notes.push_back({3, 4, 81.1, 1, 81, false, 0, 1});
            require(std::abs(mp::correctionAt(c, s, 0.5) - 0.2) < 0.01, "First A group target");
            require(std::abs(mp::correctionAt(c, s, 3.5) - 0.1) < 0.01, "A in another octave follows shared group");
            s.groupStrength[9] = 0.5;
            require(std::abs(mp::correctionAt(c, s, 0.5) - 0.1) < 0.01, "Independent group strength scales master quantize");
            s.groupStrength[9] = 1;
            s.octaveMode = true; s.octaveCents[81] = -20;
            require(std::abs(mp::correctionAt(c, s, 3.5) + 0.3) < 0.01, "Octave-specific independent target");
            c.notes[1].detached = true; c.notes[1].target = 0; c.notes[1].strength = 0.5;
            require(std::abs(mp::correctionAt(c, s, 3.5) + 0.05) < 0.01, "Detached note has independent target/strength");
            s.octaveMode = false; s.cents[9] = -20;
            auto v = tone(rate, 69, true); mp::analyse(v); auto corrected = mp::render(v, s);
            double lo = 1000, hi = -1000;
            for (double t = 0.7; t < 1.5; t += 0.02) { const auto m = measure(corrected, rate, t); lo = std::min(lo, m); hi = std::max(hi, m); }
            require(hi - lo > 0.25, "Vibrato pitch motion survives centre correction");
            const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mp-render-test", ".wav");
            require(mp::writeWav(file, up, rate), "WAV export succeeds"); juce::String error; auto loaded = mp::loadAudio(file, error); file.deleteFile();
            require(loaded && loaded->audio.getNumSamples() == up.getNumSamples(), "WAV export reload duration");
            require(std::abs(measure(loaded->audio, rate) - 69.2) < 0.035, "WAV export contains corrected pitch");
            auto original = tone(rate); mp::analyse(original); original.anchorSeconds = 12.5; original.notes[0].detached = true; original.notes[0].target = -18;
            auto restored = mp::clipFromTree(mp::clipToTree(original));
            require(restored && restored->notes.size() == original.notes.size() && restored->notes[0].target == -18 && restored->anchorSeconds == 12.5, "Map/source state round-trip");
            require(restored->audio.getSample(0, 1000) == original.audio.getSample(0, 1000), "State retains source PCM exactly");
            s.humanize = 5; s.seed = 7; const auto settings = mp::settingsFromTree(mp::settingsToTree(s));
            require(settings.cents == s.cents && settings.seed == 7 && settings.humanize == 5, "All DSP settings round-trip");
            require(mp::correctionAt(original, s, 0.8) == mp::correctionAt(original, settings, 0.8), "Seeded humanize is deterministic after state restore");
            auto anti = tone(rate); anti.audio.applyGain(1, 0, anti.audio.getNumSamples(), -1); mp::analyse(anti);
            require(!anti.notes.empty(), "Anti-phase stereo analysis does not cancel");
            const auto antiRendered = mp::render(anti, s); double phaseError = 0;
            for (int i = 0; i < antiRendered.getNumSamples(); ++i) phaseError = std::max(phaseError, std::abs(double(antiRendered.getSample(0, i) + antiRendered.getSample(1, i))));
            require(phaseError < 1e-5, "Anti-phase stereo relationship survives rendering");
            auto wide = tone(rate, 69);
            for (int i = 0; i < wide.audio.getNumSamples(); ++i) wide.audio.setSample(1, i, float(0.16 * std::sin(2 * juce::MathConstants<double>::pi * 440 * (i + 1) / rate + 0.35)));
            mp::analyse(wide); mp::Settings stereoSettings; stereoSettings.strength = 1; stereoSettings.cents[9] = -20; stereoSettings.formants = false; stereoSettings.smoothingMs = 0;
            const auto wideOutput = mp::render(wide, stereoSettings);
            std::complex<double> left{}, right{};
            for (int i = int(rate * 0.5); i < int(rate * 1.5); ++i) { const auto phase = std::polar(1.0, -2 * juce::MathConstants<double>::pi * mp::midiToHz(68.8) * i / rate); left += double(wideOutput.getSample(0, i)) * phase; right += double(wideOutput.getSample(1, i)) * phase; }
            require(std::abs(std::arg(right * std::conj(left)) - 0.35) < 0.03, "General stereo inter-channel phase survives correction");
            const auto midiFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mp-midi-test", ".mid");
            require(mp::writeMidi(midiFile, original, s), "MIDI export succeeds");
            auto midiInput = midiFile.createInputStream(); juce::MidiFile midi;
            require(midiInput && midi.readFrom(*midiInput) && midi.getNumTracks() == 1, "MIDI output is a readable file");
            bool foundMicrotone = false;
            for (const auto* event : *midi.getTrack(0)) if (event->message.isPitchWheel() && event->message.getPitchWheelValue() != 8192) foundMicrotone = true;
            require(foundMicrotone, "MIDI retains microtonal pitch-bend data"); midiFile.deleteFile();
            const auto start = juce::Time::getMillisecondCounterHiRes(); mp::render(original, s);
            std::cout << "Sample rate " << rate << ": all checks passed; 2s offline render " << juce::Time::getMillisecondCounterHiRes() - start << " ms\n";
        }
        std::cout << checks << " assertions passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n'; return 1; }
}
