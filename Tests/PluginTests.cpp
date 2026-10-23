#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>
static void check(bool pass, const char* message) { if (!pass) throw std::runtime_error(message); }
struct TestPlayHead : juce::AudioPlayHead {
    int64_t sample = 48000;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo info; info.setTimeInSamples(sample); info.setIsPlaying(true); return info; }
};
int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    std::cout << "Stage: GUI runtime initialization\n";
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        std::cout << "Stage: processor and safe defaults\n";
        PitchProcessor p(true); p.setParameter("adaptive", 0); p.setParameter("direct", 0); p.setParameter("referenceMatch", 0); p.setParameter("enabled", 1); p.prepareToPlay(48000, 256);
        check(p.parameters.getRawParameterValue("quantize")->load() == 0, "Safe default strength");
        p.setParameter("cent0", -20); p.setParameter("quantize", 0.5f); p.setParameter("reference", 442);
        juce::MemoryBlock state; p.getStateInformation(state);
        std::cout << "Stage: parameter state restore\n";
        PitchProcessor restored(true); restored.setStateInformation(state.getData(), int(state.getSize())); restored.prepareToPlay(48000, 256);
        check(restored.parameters.getRawParameterValue("cent0")->load() == -20, "Host target parameter state");
        check(restored.parameters.getRawParameterValue("quantize")->load() == 0.5f, "Host quantize parameter state");
        check(restored.parameters.getRawParameterValue("reference")->load() == 442, "Host reference parameter state");
        juce::AudioBuffer<float> buffer(2, 256); juce::MidiBuffer midi;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) buffer.setSample(ch, i, float(i) / 512);
        restored.processBlock(buffer, midi);
        check(buffer.getMagnitude(0, 256) == 0, "Streaming startup contains the reported delay, not unprocessed input");
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
        check(playback.getSample(0, 0) != 0.125f, "Outside cached timeline uses latency-aligned dry input"); playHead.sample = 44100;
        restored.setParameter("bypass", 1);
        for (int i = 0; i < 4; ++i) { playback.clear(); restored.processBlock(playback, midi); }
        check(playback.getMagnitude(0, 4096) == 0, "Bypass returns silent host input after draining reported latency");
        restored.getStateInformation(state); PitchProcessor roundTrip(true); roundTrip.setStateInformation(state.getData(), int(state.getSize()));
        check(roundTrip.clip() && roundTrip.clip()->notes.size() == 1 && roundTrip.clip()->audio.getNumSamples() == 96000, "Full plugin state restores PCM and note map");
        restored.setPlayHead(nullptr);
        std::cout << "Stage: adaptive defaults, legacy migration and analysis metadata\n";
        PitchProcessor adaptiveState(true);
        check(adaptiveState.parameters.getRawParameterValue("adaptive")->load() == 1, "New instances default to adaptive offline processing");
        juce::ValueTree legacy("MicrotonalPitchMapPro"); legacy.setProperty("version", 2, nullptr);
        legacy.addChild(adaptiveState.parameters.copyState(), -1, nullptr);
        juce::MemoryOutputStream legacyStream; legacy.writeToStream(legacyStream);
        adaptiveState.setStateInformation(legacyStream.getData(), int(legacyStream.getDataSize()));
        check(adaptiveState.parameters.getRawParameterValue("adaptive")->load() == 0, "Version 2 state keeps legacy DSP semantics");
        auto metadata = clip; metadata.adaptiveAnalysis = true;
        metadata.notes.front().stableStart = 0.12; metadata.notes.front().stableEnd = 1.8;
        metadata.notes.front().uncertainty = 0.7; metadata.notes.front().discrete = true; metadata.notes.front().overlap = true;
        metadata.frames.front().uncertainty = 0.8; metadata.frames.front().onset = 0.6; metadata.frames.front().agreement = 0.9;
        const auto decoded = mp::clipFromTree(mp::clipToTree(metadata));
        check(decoded && decoded->adaptiveAnalysis && decoded->notes.front().discrete && decoded->notes.front().overlap, "Adaptive event flags survive serialized state");
        check(std::abs(decoded->notes.front().stableStart - 0.12) < 1e-9 && std::abs(decoded->notes.front().uncertainty - 0.7) < 1e-9, "Stable bounds and centre uncertainty survive serialized state");
        check(std::abs(decoded->frames.front().onset - 0.6) < 1e-9 && std::abs(decoded->frames.front().agreement - 0.9) < 1e-9, "Frame onset and detector agreement survive serialized state");
        restored.setParameter("adaptive", 1); restored.setParameter("precision", 0.5f); restored.setParameter("minimumIntervention", 0);
        restored.getStateInformation(state); adaptiveState.setStateInformation(state.getData(), int(state.getSize()));
        check(adaptiveState.parameters.getRawParameterValue("adaptive")->load() == 1 && adaptiveState.parameters.getRawParameterValue("precision")->load() == 0.5f && adaptiveState.parameters.getRawParameterValue("minimumIntervention")->load() == 0, "Adaptive user controls survive full host state");
        const auto beforeCancel = restored.clip(); restored.analyse(); restored.cancelWork();
        const auto cancelDeadline = juce::Time::getMillisecondCounter() + 30000;
        while (restored.busy && juce::Time::getMillisecondCounter() < cancelDeadline) juce::Thread::sleep(1);
        check(!restored.busy && restored.clip() == beforeCancel, "Cancelled background analysis does not replace the previous source/map");
        std::cout << "Stage: adaptive rendered clip reaches the host callback\n";
        auto adaptiveClip = clip; mp::analyseAdaptive(adaptiveClip);
        adaptiveState.setParameter("adaptive", 1); adaptiveState.setParameter("precision", 1);
        adaptiveState.setParameter("direct", 1); adaptiveState.setParameter("referenceMatch", 0);
        adaptiveState.setParameter("cent9", -5); adaptiveState.setParameter("formants", 0);
        adaptiveState.setParameter("enabled", 1); adaptiveState.setParameter("bypass", 0);
        adaptiveState.setParameter("original", 0); adaptiveState.setParameter("wet", 1); adaptiveState.setParameter("useRender", 1);
        juce::ValueTree adaptiveRoot("MicrotonalPitchMapPro"); adaptiveRoot.setProperty("version", mp::stateVersion, nullptr);
        adaptiveRoot.addChild(adaptiveState.parameters.copyState(), -1, nullptr); adaptiveRoot.addChild(mp::clipToTree(adaptiveClip), -1, nullptr);
        juce::MemoryOutputStream adaptiveStream; adaptiveRoot.writeToStream(adaptiveStream);
        adaptiveState.setStateInformation(adaptiveStream.getData(), int(adaptiveStream.getDataSize()));
        adaptiveState.prepareToPlay(48000, 256); adaptiveState.render();
        const auto adaptiveDeadline = juce::Time::getMillisecondCounter() + 30000;
        while (adaptiveState.busy && juce::Time::getMillisecondCounter() < adaptiveDeadline) juce::Thread::sleep(1);
        check(!adaptiveState.busy && adaptiveState.hasRender(), "Adaptive processor publishes a complete offline render");
        TestPlayHead adaptiveHead; adaptiveHead.sample = 48000; adaptiveState.setPlayHead(&adaptiveHead);
        playback.clear(); adaptiveState.processBlock(playback, midi);
        const auto adaptivePitch = mp::detectPitch(playback.getReadPointer(0), 4096, 48000).first;
        std::cout << "Adaptive host callback shift: " << (mp::hzToMidi(adaptivePitch)-69)*100 << " cents\n";
        check(adaptivePitch > 0 && std::abs((mp::hzToMidi(adaptivePitch)-69)*100+5) < 1, "New adaptive -5 cent PCM actually reaches the host callback");
        adaptiveState.setPlayHead(nullptr);
        if (argc == 2 && juce::String(argv[1]) == "--audio-only") {
            std::cout << "Plugin parameter/source/map state and offline host playback checks passed\n"; return 0;
        }
        std::cout << "Stage: editor construction\n";
        restored.setParameter("adaptive",0); restored.setParameter("octave",1);

        restored.setParameter("referenceMatch", 1); restored.setParameter("direct", 0);
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(restored.createEditor());
        check(editor && editor->getWidth() >= 1060, "Real editor constructs");
        check(restored.liveWorkspaceEnabled()&&restored.parameters.getRawParameterValue("octave")->load()==0,"New editor enables the all-octave workspace");
        int percentages=0;bool scaleFound=false;
        std::function<void(juce::Component&)> inspect=[&](juce::Component& component){
            if(auto* slider=dynamic_cast<juce::Slider*>(&component);slider&&slider->getMaximum()==1){
                check(slider->getTextFromValue(.5).contains("50.0%"),"Attached percentage control displays 50 percent");
                check(slider->getValueFromText("50%")==.5,"Attached percentage control accepts text entry");++percentages;
            }
            if(auto* combo=dynamic_cast<juce::ComboBox*>(&component);combo&&combo->getNumItems()==5&&combo->getItemText(0)=="100%")scaleFound=true;
            for(auto* child:component.getChildren())inspect(*child);
        };inspect(*editor);
        check(percentages>=6,"Pitch, humanize, finishing and mix percentages are bound to actual parameters");
        check(scaleFound,"Editor exposes 100 to 200 percent scaling");
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
