#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout PitchProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    auto add = [&](const juce::String& id, const juce::String& name, float lo, float hi, float step, float initial) {
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name, juce::NormalisableRange<float>(lo, hi, step), initial));
    };
    for (int i = 0; i < 12; ++i) add("cent" + juce::String(i), mp::noteName(i + 60).dropLastCharacters(1) + " target cents", -200, 200, 0.01f, 0);
    for (int i = 0; i < 12; ++i) add("gq" + juce::String(i), mp::noteName(i + 60).dropLastCharacters(1) + " strength multiplier", 0, 1, 0.001f, 1);
    for (int i = 0; i < 128; ++i) add("oct" + juce::String(i), mp::noteName(i) + " target cents", -200, 200, 0.01f, 0);
    add("quantize", "Quantize", 0, 1, 0.001f, 0); add("tightness", "Intonation tightness", 0, 1, 0.001f, 0);
    add("vibrato", "Vibrato preservation", 0, 1, 0.001f, 1); add("smoothing", "Transition smoothing ms", 0, 250, 1, 30);
    add("humanize", "Humanize maximum cents", 0, 50, 0.1f, 0); add("seed", "Humanize seed", 1, 10000, 1, 1);
    add("reference", "Reference Hz", 400, 480, 0.1f, 440); add("wet", "Wet", 0, 1, 0.001f, 1);
    add("offset", "Timeline offset seconds", 0, 36000, 0.001f, 0);
    for (auto id : {"octave", "formants", "useRender", "bypass", "original"})
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id, 1}, id, juce::String(id) == "formants"));
    return p;
}
PitchProcessor::PitchProcessor() : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
parameters(*this, nullptr, "Parameters", layout()) { startTimer(500); }
PitchProcessor::~PitchProcessor() { stopTimer(); worker.removeAllJobs(true, -1); }
bool PitchProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainInputChannelSet() == l.getMainOutputChannelSet() && (l.getMainOutputChannelSet() == juce::AudioChannelSet::mono() || l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
}
void PitchProcessor::prepareToPlay(double sr, int) {
    runtimeRate = sr; previewPosition = 0; capturing = false; captured = 0;
    captureBuffer.setSize(2, int(sr * mp::maxSeconds)); captureBuffer.clear();
    setLatencySamples(0);
}
void PitchProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals; midi.clear(); inCallback.store(true);
    const auto position = getPlayHead() ? getPlayHead()->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo>{};
    const auto sampleTime = position ? position->getTimeInSamples() : juce::Optional<int64_t>{};
    const bool playing = position && position->getIsPlaying();
    if (capturing.load()) {
        const int offset = captured.load();
        if (offset == 0) captureAnchor.store(sampleTime ? double(*sampleTime) / runtimeRate : 0);
        const int count = std::min(b.getNumSamples(), captureBuffer.getNumSamples() - offset);
        if (count > 0) {
            for (int ch = 0; ch < 2; ++ch) captureBuffer.copyFrom(ch, offset, b, std::min(ch, b.getNumChannels() - 1), 0, count);
            captured.store(offset + count);
        }
        if (offset + count >= captureBuffer.getNumSamples()) { capturing.store(false); captureFull.store(true); }
    }
    Rendered* rendered;
    do { rendered = current.load(); hazard.store(rendered); } while (rendered != current.load());
    const auto param = [&](const char* id) { return parameters.getRawParameterValue(id)->load(); };
    const bool preview = previewing.load();
    if (rendered && param("bypass") < 0.5f && (preview || (param("useRender") > 0.5f && playing && sampleTime))) {
        const double start = preview ? double(previewPosition.load()) / runtimeRate : double(*sampleTime) / runtimeRate - rendered->anchor - param("offset");
    const float wet = param("wet");
        const bool original = param("original") > 0.5f;
        for (int i = 0; i < b.getNumSamples(); ++i) {
            const double p = (start + i / runtimeRate) * rendered->rate;
            if (p < 0 || p >= rendered->audio.getNumSamples()) continue;
            const int a = int(std::floor(p)), next = std::min(a + 1, rendered->audio.getNumSamples() - 1);
            if (a >= 0 && a < rendered->audio.getNumSamples()) for (int ch = 0; ch < b.getNumChannels(); ++ch) {
                const int sourceChannel = std::min(ch, rendered->audio.getNumChannels() - 1);
                const float x = rendered->audio.getSample(sourceChannel, a), y = rendered->audio.getSample(sourceChannel, next);
                const float corrected = x + float(p - a) * (y - x);
                const float originalA = rendered->source->audio.getSample(sourceChannel, a), originalB = rendered->source->audio.getSample(sourceChannel, next);
                const float dry = preview ? originalA + float(p - a) * (originalB - originalA) : b.getSample(ch, i);
                b.setSample(ch, i, original ? dry : dry + wet * (corrected - dry));
            }
        }
        if (preview) { previewPosition += b.getNumSamples(); if (double(previewPosition.load()) / runtimeRate >= rendered->audio.getNumSamples() / rendered->rate) previewing = false; }
    }
    hazard.store(nullptr); inCallback.store(false);
}
juce::AudioProcessorParameter* PitchProcessor::getBypassParameter() const { return parameters.getParameter("bypass"); }
mp::Settings PitchProcessor::settings() const {
    auto p = [&](const juce::String& name) { return double(parameters.getRawParameterValue(name)->load()); };
    mp::Settings s;
    for (int i = 0; i < 12; ++i) s.cents[size_t(i)] = p("cent" + juce::String(i));
    for (int i = 0; i < 12; ++i) s.groupStrength[size_t(i)] = p("gq" + juce::String(i));
    for (int i = 0; i < 128; ++i) s.octaveCents[i] = p("oct" + juce::String(i));
    s.octaveMode = p("octave") > 0.5; s.formants = p("formants") > 0.5;
    s.strength = p("quantize"); s.tightness = p("tightness"); s.vibrato = p("vibrato");
    s.smoothingMs = p("smoothing"); s.humanize = p("humanize"); s.seed = int(p("seed")); s.reference = p("reference"); return s;
}
void PitchProcessor::setParameter(const juce::String& name, float value) {
    if (auto* p = parameters.getParameter(name)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(value)); p->endChangeGesture(); }
}
std::shared_ptr<const mp::Clip> PitchProcessor::clip() const { const juce::ScopedLock lock(modelLock); return model; }
void PitchProcessor::setStatus(juce::String s) { const juce::ScopedLock lock(statusLock); message = std::move(s); }
juce::String PitchProcessor::status() const { const juce::ScopedLock lock(statusLock); return message; }
void PitchProcessor::publish(std::unique_ptr<Rendered> r) {
    auto* fresh = r.get(); retained.push_back(std::move(r)); current.store(fresh);
    const auto* active = hazard.load();
    std::erase_if(retained, [&](const auto& item) { return item.get() != fresh && item.get() != active; });
}
void PitchProcessor::install(std::shared_ptr<mp::Clip> c) {
    const juce::ScopedLock lock(modelLock); model = std::move(c); history.clear(); future.clear(); ++revision;
    // Previously rendered audio must not play against a new source.
    current.store(nullptr); needsRender = true;
}
void PitchProcessor::load(const juce::File& f) {
    if (busy.exchange(true) || capturing) { busy = worker.getNumJobs() != 0; return; }
    worker.addJob([this, f] { juce::String error; auto c = mp::loadAudio(f, error); if (c) { install(c); setStatus("Loaded " + c->name + ". Click Analyze."); } else setStatus(error); progress = 0; busy = false; });
}
void PitchProcessor::analyse() {
    if (capturing) return;
    if (busy.exchange(true)) return;
    auto c = clip(); if (!c) { busy = false; setStatus("Load or capture audio first"); return; }
    worker.addJob([this, c] {
        setStatus("Analyzing monophonic pitch offline..."); auto next = std::make_shared<mp::Clip>(*c);
        mp::analyse(*next, [this](double p) { progress = p; return true; }); install(next);
        setStatus(juce::String(next->notes.size()) + " notes detected. Low-confidence frames remain uncorrected."); busy = false;
    });
}
void PitchProcessor::render() {
    if (capturing) return;
    if (busy.exchange(true)) return;
    auto c = clip(); if (!c) { busy = false; return; }
    const auto s = settings();
    needsRender = false;
    worker.addJob([this, c, s] {
        setStatus("Rendering corrected audio offline..."); auto r = std::make_unique<Rendered>(); r->rate = c->sampleRate; r->anchor = c->anchorSeconds; r->source = c;
        r->audio = mp::render(*c, s, [this](double p) { progress = p; return true; });
        publish(std::move(r)); lastRenderedSettings = s;
        setStatus("Render ready. Enable DAW Render to use it on the host timeline, or Preview."); busy = false;
    });
}
void PitchProcessor::timerCallback() {
    if (captureFull.exchange(false)) capture(false);
    const auto fingerprint = mp::settingsToTree(settings()).toXmlString();
    if (fingerprint != settingsFingerprint) { settingsFingerprint = fingerprint; needsRender = true; }
    if (!busy && !capturing && clip() && needsRender) render();
}
void PitchProcessor::capture(bool start) {
    if (start) {
        if (busy || capturing) return;
        captured = 0; captureFull = false; capturing = true; setStatus("Capturing DAW input (maximum 120 seconds). Stop capture before Analyze.");
    } else {
        capturing = false;
        if (busy.exchange(true)) return;
        worker.addJob([this] {
            while (inCallback.load()) juce::Thread::yield();
            const int count = captured.load();
            if (count > 0) { auto c = std::make_shared<mp::Clip>(); c->sampleRate = runtimeRate; c->anchorSeconds = captureAnchor; c->name = "Captured Sonar track"; c->audio.setSize(2, count); for (int ch = 0; ch < 2; ++ch) c->audio.copyFrom(ch, 0, captureBuffer, ch, 0, count); install(c); setStatus("Capture stored. Click Analyze."); }
            else setStatus("No audio captured. Start DAW playback while capture is active."); busy = false;
        });
    }
}
void PitchProcessor::preview(bool start) { if (!previewing && !inCallback) previewPosition = 0; previewing = start; }
void PitchProcessor::editNotes(std::vector<mp::Note> notes) {
    if (busy) return;
    const juce::ScopedLock lock(modelLock); if (!model) return;
    history.push_back(model->notes); if (history.size() > 32) history.erase(history.begin()); future.clear();
    auto c = std::make_shared<mp::Clip>(*model); c->notes = std::move(notes); model = c; ++revision; needsRender = true;
}
void PitchProcessor::undo(bool redo) {
    if (busy) return;
    const juce::ScopedLock lock(modelLock); if (!model) return;
    auto& from = redo ? future : history; auto& to = redo ? history : future;
    if (from.empty()) return; to.push_back(model->notes);
    auto c = std::make_shared<mp::Clip>(*model); c->notes = std::move(from.back()); from.pop_back(); model = c; ++revision; needsRender = true;
}
void PitchProcessor::getStateInformation(juce::MemoryBlock& block) {
    juce::ValueTree root("MicrotonalPitchMapPro"); root.setProperty("version", mp::stateVersion, nullptr);
    root.addChild(parameters.copyState(), -1, nullptr); auto c = clip(); if (c) root.addChild(mp::clipToTree(*c), -1, nullptr);
    juce::MemoryOutputStream stream(block, false); root.writeToStream(stream);
}
void PitchProcessor::setStateInformation(const void* data, int bytes) {
    if (bytes <= 0 || bytes > 256 * 1024 * 1024) return;
    const auto root = juce::ValueTree::readFromData(data, size_t(bytes));
    if (!root.hasType("MicrotonalPitchMapPro") || int(root["version"]) != mp::stateVersion) return;
    // Restore source/map synchronously; expensive audio rendering remains on the worker.
    worker.removeAllJobs(true, -1);
    auto state = root.getChildWithName("Parameters"); if (state.isValid()) parameters.replaceState(state);
    install(mp::clipFromTree(root.getChildWithName("Clip"))); busy = false; setStatus("Project restored; offline render will refresh.");
}
void PitchProcessor::importScale(const juce::File& file) {
    auto s = settings(); juce::String error;
    if (!mp::importScala(file, s, error)) { setStatus(error); return; }
    for (int i = 0; i < 12; ++i) setParameter("cent" + juce::String(i), float(s.cents[size_t(i)]));
    setStatus("12-step Scala scale imported with C as root.");
}
void PitchProcessor::exportAudio(const juce::File& f) {
    if (busy.exchange(true)) return; auto c = clip(); const auto s = settings();
    if (!c) { busy = false; return; }
    worker.addJob([this, c, s, f] { const auto audio = mp::render(*c, s, [this](double p) { progress = p; return true; }); setStatus(mp::writeWav(f, audio, c->sampleRate) ? "Exported 24-bit WAV" : "Export failed: choose a new writable filename"); busy = false; });
}
void PitchProcessor::exportMidi(const juce::File& f) { auto c = clip(); if (c) setStatus(mp::writeMidi(f, *c, settings()) ? "Exported MIDI with pitch-bend data (+/-2 semitone range)" : "MIDI export failed: choose a new filename"); }
void PitchProcessor::preset(const juce::File& f, bool save) {
    if (busy) return;
    if (save) { if (f.existsAsFile()) { setStatus("Choose a new preset filename"); return; } juce::MemoryBlock b; getStateInformation(b); setStatus(f.replaceWithData(b.getData(), b.getSize()) ? "Preset saved including source audio and map" : "Preset save failed"); }
    else { juce::MemoryBlock b; if (f.getSize() > 256 * 1024 * 1024 || !f.loadFileAsData(b)) { setStatus("Preset read failed"); return; } setStateInformation(b.getData(), int(b.getSize())); }
}
juce::AudioProcessorEditor* PitchProcessor::createEditor() { return new PitchEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PitchProcessor(); }
