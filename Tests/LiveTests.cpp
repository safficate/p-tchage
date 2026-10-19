#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <new>

static thread_local bool inAudioCallback = false;
static thread_local size_t audioAllocations = 0;
void* operator new(std::size_t n) {
    if (inAudioCallback) ++audioAllocations;
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
static int assertions = 0;
static void check(bool ok, const char* message) { ++assertions; if (!ok) throw std::runtime_error(message); }

static juce::AudioBuffer<float> run(PitchProcessor& p, double rate, float strength, float wet = 1, bool hostBypass = false, bool antiPhase = false, double sourceMidi = 69.1, bool activate = true) {
    p.setParameter("quantize", strength); p.setParameter("wet", wet);
    if (activate) p.setParameter("enabled", 1);
    const int length = int(rate * 2.2);
    juce::AudioBuffer<float> result(2, length);
    constexpr int maximumBlock = 257;
    juce::AudioBuffer<float> block(2, maximumBlock); juce::MidiBuffer midi;
    const double hz = mp::midiToHz(sourceMidi);
    const int sizes[] = {64, 257, 1, 128, 31, 256};
    int n = 0;
    const auto start = juce::Time::getMillisecondCounterHiRes();
    for (int offset = 0; offset < length; ++n) {
        const int count = std::min(sizes[n % 6], length - offset);
        block.setSize(2, count, false, false, true);
        for (int i = 0; i < count; ++i) {
            const float v = float(0.2 * std::sin(2 * juce::MathConstants<double>::pi * hz * (offset + i) / rate));
            block.setSample(0, i, v); block.setSample(1, i, antiPhase ? -v : v);
        }
        inAudioCallback = true;
        if (hostBypass) p.processBlockBypassed(block, midi); else p.processBlock(block, midi);
        inAudioCallback = false;
        for (int ch = 0; ch < 2; ++ch) result.copyFrom(ch, offset, block, ch, 0, count);
        offset += count;
    }
    std::cout << "rate=" << rate << " strength=" << strength << " elapsed_ms=" << juce::Time::getMillisecondCounterHiRes() - start << '\n';
    check(audioAllocations == 0, "C++ allocation occurred inside the audio callback");
    return result;
}
static double measure(const juce::AudioBuffer<float>& result, double rate, double referenceMidi = 69) {
    const int offset = int(rate * 1.5);
    const auto [frequency, confidence] = mp::detectPitch(result.getReadPointer(0, offset), 4096, rate);
    check(frequency > 0 && confidence > 0.95, "Corrected output must contain a stable voiced tone");
    return (mp::hzToMidi(frequency) - referenceMidi) * 100;
}
static void checkDry(const juce::AudioBuffer<float>& result, double rate, int latency) {
    const double hz = mp::midiToHz(69.1);
    bool exact = true;
    for (int i = 0; i < result.getNumSamples(); ++i) {
        const float expected = i < latency ? 0 : float(0.2 * std::sin(2 * juce::MathConstants<double>::pi * hz * (i - latency) / rate));
        exact &= result.getSample(0, i) == expected;
    }
    check(exact, "Dry/zero/bypass path must match the original bit-exactly after reported latency");
}
static void sidechainTest(double rate, int follow, double referenceMidi, double expectedCents) {
    PitchProcessor p; auto layout = p.getBusesLayout(); layout.inputBuses.set(1, juce::AudioChannelSet::stereo());
    check(p.setBusesLayout(layout), "Optional stereo sidechain must be supported");
    p.prepareToPlay(rate, 256); p.setParameter("follow", float(follow)); p.setParameter("enabled", 1);
    const int length = int(rate * 2.2); juce::AudioBuffer<float> result(2, length), block(4, 256); juce::MidiBuffer midi;
    bool referenceUnchanged = true;
    for (int offset = 0; offset < length; offset += 256) {
        const int count = std::min(256, length - offset); block.setSize(4, count, false, false, true);
        for (int i = 0; i < count; ++i) {
            const double time = (offset + i) / rate;
            const float source = float(0.2 * std::sin(2 * juce::MathConstants<double>::pi * mp::midiToHz(69.1) * time));
            const float reference = float(0.2 * std::sin(2 * juce::MathConstants<double>::pi * mp::midiToHz(referenceMidi) * time));
            block.setSample(0, i, source); block.setSample(1, i, source); block.setSample(2, i, reference); block.setSample(3, i, reference);
        }
        inAudioCallback = true; p.processBlock(block, midi); inAudioCallback = false;
        for (int i = 0; i < count; ++i) {
            const float reference = float(0.2 * std::sin(2 * juce::MathConstants<double>::pi * mp::midiToHz(referenceMidi) * ((offset + i) / rate)));
            referenceUnchanged &= block.getSample(2, i) == reference && block.getSample(3, i) == reference;
        }
        for (int ch = 0; ch < 2; ++ch) result.copyFrom(ch, offset, block, ch, 0, count);
    }
    const double measured = measure(result, rate);
    std::cout << "SIDECHAIN mode=" << follow << " measured=" << measured << " expected=" << expectedCents << '\n';
    check(std::abs(measured - expectedCents) < 1, "Only the target channel must follow the reference pitch/tuning within 1 cent");
    check(referenceUnchanged, "Sidechain/reference audio must remain bit-exactly unchanged");
    check(audioAllocations == 0, "Sidechain audio callback must not allocate C++ storage");
    check(!p.takeTrace().empty(), "Measured source/reference pitch traces must be available to the editor");
}
static void auditionTest(double rate) {
    PitchProcessor p; p.prepareToPlay(rate, 257); p.setParameter("cent0", 75);
    p.auditionTone(0, 4);
    check(std::abs(measure(run(p, rate, 0), rate, 60) - 75) < 1, "Upper reference key must sound its actual tuned frequency");
    p.stopAudition();
    mp::Clip clip; clip.sampleRate = rate; clip.audio.setSize(1, int(rate * 2));
    for (int i = 0; i < clip.audio.getNumSamples(); ++i) {
        const double phase = 2 * juce::MathConstants<double>::pi * mp::midiToHz(60.1) * i / rate;
        clip.audio.setSample(0, i, float(0.2 * std::sin(phase) + 0.03 * std::sin(phase * 3)));
    }
    mp::analyse(clip); juce::ValueTree state("MicrotonalPitchMapPro"); state.setProperty("version", mp::stateVersion, nullptr); state.addChild(mp::clipToTree(clip), -1, nullptr);
    juce::MemoryOutputStream stream; state.writeToStream(stream); p.setStateInformation(stream.getData(), int(stream.getDataSize()));
    const int selected = p.auditionSource(0); check(selected >= 0, "Lower C key must find an actual C in the source clip");
    auto heard = run(p, rate, 0);
    check(std::abs(measure(heard, rate, 60) - 10) < 1, "Lower key must play the original recorded source, not the tuned reference synthesizer");
    check(p.auditionSource(2) < 0, "A missing source note must never be replaced with a fake synthesized sample");
    p.stopAudition();
}
static void stereoTailTest(double rate) {
    PitchProcessor p; p.prepareToPlay(rate, 256); p.setParameter("enabled", 1); p.setParameter("cent9", -20);
    const int length = int(rate * 1.3), silenceAt = int(rate); juce::AudioBuffer<float> output(2, length), block(2, 256); juce::MidiBuffer midi;
    for (int offset = 0; offset < length; offset += 256) {
        const int count = std::min(256, length - offset); block.setSize(2, count, false, false, true);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < count; ++i)
            block.setSample(ch, i, offset + i < silenceAt ? float(0.2 * std::sin(2 * juce::MathConstants<double>::pi * mp::midiToHz(69.1) * (offset + i) / rate + ch * 0.5)) : 0);
        inAudioCallback = true; p.processBlock(block, midi); inAudioCallback = false;
        for (int ch = 0; ch < 2; ++ch) output.copyFrom(ch, offset, block, ch, 0, count);
    }
    double energy = 0, difference = 0;
    for (int i = int(rate * 1.02); i < int(rate * 1.08); ++i) {
        const double left = output.getSample(0, i), right = output.getSample(1, i);
        energy += left * left + right * right; difference += (left - right) * (left - right);
    }
    check(energy > 1 && difference / energy > 0.02, "Incoming silence must not collapse the delayed stereo tail to mono");
    check(audioAllocations == 0, "Stereo transitions must not allocate inside the audio callback");
}
int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        for (double rate : {44100.0, 48000.0}) {
            PitchProcessor fresh; fresh.prepareToPlay(rate, 257); fresh.setParameter("cent9", -20);
            checkDry(run(fresh, rate, 0, 1, false, false, 69.1, false), rate, fresh.getLatencySamples());
            fresh.applyCorrection();
            check(std::abs(measure(run(fresh, rate, 0, 1, false, false, 69.1, false), rate) + 20) < 1, "Apply must enable full reference matching without a percentage");
            for (float strength : {0.0f, 0.5f, 1.0f}) {
                PitchProcessor p; p.setParameter("direct", 0); p.setParameter("referenceMatch", 0); p.prepareToPlay(rate, 257);
                auto* quantize = p.parameters.getParameter("quantize");
                check(quantize->getText(0.5f, 0) == "50.0%", "Correction amount must be displayed as a percentage");
                check(quantize->getValueForText("50%") == 0.5f, "Typing 50% must set half correction, not clamp to full strength");
                check(p.getLatencySamples() > 0, "Streaming latency must be reported to the host");
                check(!p.clip(), "Live test must not load, capture, analyze or render a source file");
                p.setParameter("cent9", -20); p.setParameter("formants", 0);
                auto result = run(p, rate, strength);
                if (argc == 2) {
                    const auto folder = juce::File(juce::String(argv[1])); folder.createDirectory();
                    juce::AudioBuffer<float> aligned(2, int(rate * 2));
                    for (int ch = 0; ch < 2; ++ch) aligned.copyFrom(ch, 0, result, ch, p.getLatencySamples(), aligned.getNumSamples());
                    const auto file = folder.getChildFile("live-" + juce::String(int(rate)) + "-strength-" + juce::String(int(strength * 100)) + ".wav");
                    file.deleteFile(); check(mp::writeWav(file, aligned, rate), "Write actual callback output as a latency-aligned WAV");
                }
                const double cents = measure(result, rate), expected = 10 - strength * 30;
                std::cout << "LIVE target=-20c measured=" << cents << "c expected=" << expected << "c latency=" << p.getLatencySamples() << '\n';
                check(std::abs(cents - expected) < 1.0, "Direct DAW callback pitch centre missed target by more than 1 cent");
                if (strength == 0) checkDry(result, rate, p.getLatencySamples());
                float error = 0;
                for (int i = int(rate * 0.5); i < result.getNumSamples(); ++i) error = std::max(error, std::abs(result.getSample(0, i) - result.getSample(1, i)));
                check(error < 1e-5f, "Equal stereo channels must remain equal");
            }
            PitchProcessor p; p.setParameter("direct", 0); p.setParameter("referenceMatch", 0); p.prepareToPlay(rate, 257); p.setParameter("cent9", 20); p.setParameter("formants", 1);
            auto positive = run(p, rate, 1, 1, false, true);
            const double up = measure(positive, rate); std::cout << "LIVE target=+20c measured=" << up << "c\n";
            check(std::abs(up - 20) < 1, "Positive target/formant path must reach +20 cents");
            float error = 0;
            for (int i = int(rate * 0.5); i < positive.getNumSamples(); ++i) error = std::max(error, std::abs(positive.getSample(0, i) + positive.getSample(1, i)));
            check(error < 1e-5f, "Anti-phase stereo must not cancel or lose pitch tracking");
            p.reset(); checkDry(run(p, rate, 1, 0), rate, p.getLatencySamples());
            p.reset(); checkDry(run(p, rate, 1, 1, true), rate, p.getLatencySamples());
            p.reset(); p.setParameter("original", 1); checkDry(run(p, rate, 1), rate, p.getLatencySamples()); p.setParameter("original", 0);
            p.reset(); p.setParameter("bypass", 1); checkDry(run(p, rate, 1), rate, p.getLatencySamples()); p.setParameter("bypass", 0);
            p.reset(); p.setParameter("gq9", 0); check(std::abs(measure(run(p, rate, 1), rate) - 10) < 1, "Group strength 0 must leave that note unchanged"); p.setParameter("gq9", 1);
            p.reset(); p.setParameter("octave", 1); p.setParameter("oct69", -35);
            check(std::abs(measure(run(p, rate, 1), rate) + 35) < 1, "Octave-specific target must override pitch-class target");
            juce::MemoryBlock state; p.getStateInformation(state); PitchProcessor restored; restored.setStateInformation(state.getData(), int(state.getSize())); restored.prepareToPlay(rate, 257);
            check(std::abs(measure(run(restored, rate, 1), rate) + 35) < 1, "Live target and mode must survive host state restore");
            restored.setParameter("useRender", 1); restored.reset();
            check(std::abs(measure(run(restored, rate, 1), rate) + 35) < 1, "Missing cache or host transport must not silently disable live processing");
            for (float fine : {-20.0f, -5.0f, -3.0f, 3.0f, 5.0f, 10.0f, 37.5f}) {
                PitchProcessor direct; direct.setParameter("referenceMatch", 0); direct.setParameter("direct", 1); direct.prepareToPlay(rate, 257); direct.setParameter("cent9", fine);
                const double measured = measure(run(direct, rate, 0), rate);
                std::cout << "DIRECT fine=" << fine << " measured=" << measured << " expected=" << 10 + fine << '\n';
                check(std::abs(measured - (10 + fine)) < 1, "Entered cent amount must be added exactly, without Quantize");
            }
            for (int step : {1, 2, -1}) {
                PitchProcessor direct; direct.setParameter("referenceMatch", 0); direct.setParameter("direct", 1); direct.prepareToPlay(rate, 257); direct.setParameter("step0", float(step)); direct.setParameter("cent0", 5);
                const double measured = measure(run(direct, rate, 0, 1, false, false, 60.1), rate, 60);
                std::cout << "DIRECT C note step=" << step << " fine=5 measured=" << measured << " expected=" << 15 + step * 100 << '\n';
                check(std::abs(measured - (15 + step * 100)) < 1, "C to destination note plus fine cents must be exact without a percentage");
            }
            for (int step : {0, 1, 2}) {
                PitchProcessor tuned; tuned.prepareToPlay(rate, 257); tuned.setParameter("step0", float(step)); tuned.setParameter("cent" + juce::String(step), 5);
                const double measured = measure(run(tuned, rate, 0, 1, false, false, 60.1), rate, 60);
                std::cout << "REFERENCE C destination=" << step << " fine=5 measured=" << measured << " expected=" << 5 + step * 100 << '\n';
                check(std::abs(measured - (5 + step * 100)) < 1, "Source must match the destination reference tone exactly, independent of Quantize");
            }
            sidechainTest(rate, 1, 57 - 0.05, -5);
            sidechainTest(rate, 1, 23 - 0.05, -5);
            sidechainTest(rate, 2, 55 + 0.07, -193);
            auditionTest(rate);
            stereoTailTest(rate);
        }
        std::cout << assertions << " live audio assertions passed; no file, no transport and no message-loop dependency\n";
        return 0;
    } catch (const std::exception& e) { inAudioCallback = false; std::cerr << e.what() << '\n'; return 1; }
}
