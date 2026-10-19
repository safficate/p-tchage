#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>
static void check(bool pass, const char* message) { if (!pass) throw std::runtime_error(message); }
struct TestPlayHead : juce::AudioPlayHead {
    int64_t sample = 48000;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo info; info.setTimeInSamples(sample); info.setIsPlaying(true); return info; }
};
int main() {
    std::cout << std::unitbuf;
    std::cout << "Stage: GUI runtime initialization\n";
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        std::cout << "Stage: processor and safe defaults\n";
        PitchProcessor p; p.prepareToPlay(48000, 256);
        check(p.parameters.getRawParameterValue("quantize")->load() == 0, "Safe default strength");
        p.setParameter("cent0", -20); p.setParameter("quantize", 0.5f); p.setParameter("reference", 442);
        juce::MemoryBlock state; p.getStateInformation(state);
        std::cout << "Stage: parameter state restore\n";
        PitchProcessor restored; restored.setStateInformation(state.getData(), int(state.getSize())); restored.prepareToPlay(48000, 256);
        check(restored.parameters.getRawParameterValue("cent0")->load() == -20, "Host target parameter state");
        check(restored.parameters.getRawParameterValue("quantize")->load() == 0.5f, "Host quantize parameter state");
        check(restored.parameters.getRawParameterValue("reference")->load() == 442, "Host reference parameter state");
        juce::AudioBuffer<float> buffer(2, 256); juce::MidiBuffer midi;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) buffer.setSample(ch, i, float(i) / 512);
        restored.processBlock(buffer, midi);
        check(buffer.getSample(0, 200) == float(200) / 512, "No source/render leaves host audio unchanged");
        std::cout << "Stage: source fixture and analysis\n";
        mp::Clip clip; clip.sampleRate = 48000; clip.audio.setSize(2, 96000);
        for (int i = 0; i < 96000; ++i) for (int ch = 0; ch < 2; ++ch) clip.audio.setSample(ch, i, float(0.2 * std::sin(2 * juce::MathConstants<double>::pi * 440 * i / 48000)));
        mp::analyse(clip);
        juce::ValueTree full("MicrotonalPitchMapPro"); full.setProperty("version", mp::stateVersion, nullptr); full.addChild(mp::clipToTree(clip), -1, nullptr);
        juce::MemoryOutputStream fullStream; full.writeToStream(fullStream);
        restored.setStateInformation(fullStream.getData(), int(fullStream.getDataSize()));
        restored.setParameter("cent9", -20); restored.setParameter("quantize", 1); restored.setParameter("reference", 440); restored.setParameter("formants", 0); restored.setParameter("useRender", 1);
        std::cout << "Stage: offline render\n";
        restored.render(); const auto deadline = juce::Time::getMillisecondCounter() + 30000;
        while (restored.busy && juce::Time::getMillisecondCounter() < deadline) juce::Thread::sleep(10);
        check(!restored.busy, "Offline render finishes");
        TestPlayHead playHead; restored.setPlayHead(&playHead);
        juce::AudioBuffer<float> playback(2, 4096); playback.clear(); restored.processBlock(playback, midi);
        const auto detected = mp::detectPitch(playback.getReadPointer(0), 4096, 48000).first;
        check(detected > 0 && std::abs(mp::hzToMidi(detected) - 68.8) < 0.04, "Host callback emits corrected -20 cent audio on timeline");
        restored.prepareToPlay(44100, 256); playHead.sample = 44100; playback.clear(); restored.processBlock(playback, midi);
        const auto rate441 = mp::detectPitch(playback.getReadPointer(0), 4096, 44100).first;
        check(rate441 > 0 && std::abs(mp::hzToMidi(rate441) - 68.8) < 0.04, "Host callback supports 44.1kHz with a 48kHz source");
        juce::AudioBuffer<float> benchmark(2, 256); benchmark.clear();
        const auto startTime = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < 2000; ++i) restored.processBlock(benchmark, midi);
        std::cout << "2000 stereo callback blocks, 256 frames: " << juce::Time::getMillisecondCounterHiRes() - startTime << " ms on cloud host\n";
        playHead.sample = 1000000000000LL; playback.clear(); playback.setSample(0, 0, 0.125f); restored.processBlock(playback, midi);
        check(playback.getSample(0, 0) == 0.125f, "Outside source timeline passes through safely"); playHead.sample = 44100;
        restored.setParameter("bypass", 1); playback.clear(); restored.processBlock(playback, midi);
        check(playback.getMagnitude(0, 4096) == 0, "Bypass returns host input");
        restored.getStateInformation(state); PitchProcessor roundTrip; roundTrip.setStateInformation(state.getData(), int(state.getSize()));
        check(roundTrip.clip() && roundTrip.clip()->notes.size() == 1 && roundTrip.clip()->audio.getNumSamples() == 96000, "Full plugin state restores PCM and note map");
        restored.setPlayHead(nullptr);
        std::cout << "Stage: editor construction\n";
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(restored.createEditor());
        check(editor && editor->getWidth() >= 1060, "Real editor constructs");
        std::cout << "Stage: editor paint\n";
        juce::Image image(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true, juce::SoftwareImageType());
        { juce::Graphics graphics(image); editor->paintEntireComponent(graphics, true); }
        const auto outputFile = juce::File::getCurrentWorkingDirectory().getChildFile("editor-test.png");
        outputFile.deleteFile();
        const auto output = outputFile.createOutputStream();
        if (output) juce::PNGImageFormat().writeImageToStream(image, *output);
        std::cout << "Plugin parameter state, pass-through processing and editor smoke tests passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
