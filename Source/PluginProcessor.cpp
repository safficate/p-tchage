#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ARAIntegration.h"

juce::AudioProcessorValueTreeState::ParameterLayout PitchProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    auto add = [&](const juce::String& id, const juce::String& name, float lo, float hi, float step, float initial) {
        juce::AudioParameterFloatAttributes attributes;
        if (lo == 0 && hi == 1) attributes = attributes
            .withStringFromValueFunction([](float v, int) { return juce::String(v * 100, 1) + "%"; })
            .withValueFromStringFunction([](const juce::String& t) { return t.getFloatValue() / 100; });
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name, juce::NormalisableRange<float>(lo, hi, step), initial, attributes));
    };
    for (int i = 0; i < 12; ++i) add("cent" + juce::String(i), mp::noteName(i + 60).dropLastCharacters(1) + " target cents", -200, 200, 0.01f, 0);
    for (int i = 0; i < 12; ++i) p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"step" + juce::String(i), 1}, mp::noteName(i + 60).dropLastCharacters(1) + " note shift semitones", -12, 12, 0));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"adaptive", 1}, "Adaptive regional correction (offline)", true));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"minimumIntervention", 1}, "Minimum intervention", true));
    add("precision", "Requested centre precision cents", 0.1f, 25, 0.1f, 1);
    add("globalCents", "Relative cents for the entire source", -200, 200, 0.01f, 0);
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"direct", 1}, "Direct cent and note shifts", false));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"enabled", 1}, "Correction enabled", false));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"referenceMatch", 1}, "Match tuned reference tones", true));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"follow", 1}, "Sidechain reference mode", 0, 2, 0));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"preserveOctave", 1}, "Preserve source octave", true));
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
PitchProcessor::PitchProcessor() : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withInput("Reference (sidechain)", juce::AudioChannelSet::stereo(), false).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
parameters(*this, nullptr, "Parameters", layout()) {
    auto raw = [this](const juce::String& id) { return parameters.getRawParameterValue(id); };
    for (int i = 0; i < 12; ++i) { liveParameters.cents[size_t(i)] = raw("cent" + juce::String(i)); liveParameters.groups[size_t(i)] = raw("gq" + juce::String(i)); liveParameters.steps[size_t(i)] = raw("step" + juce::String(i)); }
    for (int i = 0; i < 128; ++i) liveParameters.octaves[size_t(i)] = raw("oct" + juce::String(i));
    auto& p = liveParameters;
    p.strength = raw("quantize"); p.tightness = raw("tightness"); p.vibrato = raw("vibrato"); p.smoothing = raw("smoothing");
    p.humanize = raw("humanize"); p.seed = raw("seed"); p.reference = raw("reference"); p.wet = raw("wet");
    p.octave = raw("octave"); p.formants = raw("formants"); p.bypass = raw("bypass"); p.original = raw("original");
    p.useRender = raw("useRender"); p.offset = raw("offset");
    p.direct = raw("direct");
    p.globalCents=raw("globalCents"); p.enabled = raw("enabled"); p.matchReference = raw("referenceMatch"); p.follow = raw("follow"); p.preserveOctave = raw("preserveOctave");
    startTimer(500);
}
PitchProcessor::~PitchProcessor() { stopTimer(); cancelRequested = true; worker.removeAllJobs(true, -1); }
bool PitchProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto reference = l.inputBuses.size() > 1 ? l.inputBuses[1] : juce::AudioChannelSet::disabled();
    return l.getMainInputChannelSet() == l.getMainOutputChannelSet() && (l.getMainOutputChannelSet() == juce::AudioChannelSet::mono() || l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo())
        && (reference.isDisabled() || reference == juce::AudioChannelSet::mono() || reference == juce::AudioChannelSet::stereo());
}
void PitchProcessor::prepareToPlay(double sr, int block) {
    runtimeRate = sr; previewPosition = 0; capturing = false; captured = 0;
    captureBuffer.setSize(0, 0);
    realtime.prepare(sr, getTotalNumInputChannels());
    referenceAnalyzer.prepare(sr, 2); processedSamples = 0; traceFifo.reset();
    setLatencySamples(realtime.latency());
#if JucePlugin_Enable_ARA
    if (prepareToPlayForARA(sr, block, getMainBusNumOutputChannels(), getProcessingPrecision())) setLatencySamples(0);
#endif
}
void PitchProcessor::releaseResources() {
    audition.stop();
#if JucePlugin_Enable_ARA
    releaseResourcesForARA();
#endif
}
void PitchProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& midi) {
    sidechainPitch = 0;
    if (getBusCount(true) > 1 && getBus(true, 1)->isEnabled()) {
        auto reference = getBusBuffer(b, true, 1);
        mp::RealtimePitch::Controls controls; controls.analysisOnly = true;
        referenceAnalyzer.process(reference, controls); sidechainPitch = referenceAnalyzer.detectedMidi();
    }
    auto main = getBusBuffer(b, true, 0); processAudio(main, midi, false);
}
void PitchProcessor::processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer& midi) {
    auto main = getBusBuffer(b, true, 0); processAudio(main, midi, true);
}
void PitchProcessor::processAudio(juce::AudioBuffer<float>& b, juce::MidiBuffer& midi, bool forceBypass) {
    juce::ScopedNoDenormals noDenormals; midi.clear(); inCallback.store(true);
    if (b.getNumChannels() == 0 || b.getNumSamples() == 0) { inCallback.store(false); return; }
    const auto position = getPlayHead() ? getPlayHead()->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo>{};
    const auto sampleTime = position ? position->getTimeInSamples() : juce::Optional<int64_t>{};
    const bool playing = position && position->getIsPlaying();
    if(sampleTime) { hostSeconds.store(double(*sampleTime)/runtimeRate); hostPositionKnown=true; }
#if JucePlugin_Enable_ARA
    if (araConnected() && position) processBlockForARA(b, isRealtime(), *position);
#endif
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
    const auto value = [](const std::atomic<float>* p) { return p->load(std::memory_order_relaxed); };
    const auto& p = liveParameters;
    const bool preview = previewing.load();
    const bool bypass = forceBypass || value(p.bypass) > 0.5f || value(p.enabled) < 0.5f;
    const bool original = value(p.original) > 0.5f;
    const bool cached = rendered && (preview || (value(p.follow) == 0 && (araConnected() || value(p.useRender) > 0.5f) && playing && sampleTime));
    mp::RealtimePitch::Controls controls;
    for (size_t i = 0; i < 12; ++i) { controls.cents[i] = value(p.cents[i]); controls.groupStrength[i] = value(p.groups[i]); controls.semitones[i] = int(value(p.steps[i])); }
    controls.directShift = value(p.direct) > 0.5f; controls.globalCents=value(p.globalCents);
    controls.matchReference = value(p.matchReference) > 0.5f;
    controls.followReference = int(value(p.follow)); controls.referenceMidi = sidechainPitch.load(); controls.preserveOctave = value(p.preserveOctave) > 0.5f;
    for (size_t i = 0; i < 128; ++i) controls.octaveCents[i] = value(p.octaves[i]);
    controls.strength = value(p.strength); controls.tightness = value(p.tightness); controls.vibrato = value(p.vibrato);
    controls.smoothingMs = value(p.smoothing); controls.humanize = value(p.humanize); controls.seed = int(value(p.seed));
    controls.reference = value(p.reference); controls.wet = value(p.wet); controls.octaveMode = value(p.octave) > 0.5f;
    controls.formants = value(p.formants) > 0.5f; controls.dry = bypass || original || cached;
    // Always advance the same delay line: dry/wet, A/B, host bypass and cached
    // playback must all match the latency reported to the DAW.
    if (!araConnected() || controls.followReference > 0) {
        if (cached) realtime.processDelay(b); else realtime.process(b, controls);
    }
    if (cached && (!bypass || (preview && !forceBypass))) {
        const double delaySeconds = getLatencySamples() / runtimeRate;
        const double start = (preview ? double(previewPosition.load()) / runtimeRate : double(*sampleTime) / runtimeRate - rendered->anchor - (araConnected() ? 0 : value(p.offset))) - delaySeconds;
        const float wet = controls.wet;
        for (int i = 0; i < b.getNumSamples(); ++i) {
            const double sourcePosition = (start + i / runtimeRate) * rendered->rate;
            if (sourcePosition < 0 || sourcePosition >= rendered->audio.getNumSamples()) { if (preview) for (int ch = 0; ch < b.getNumChannels(); ++ch) b.setSample(ch, i, 0); continue; }
            const int a = int(std::floor(sourcePosition)), next = std::min(a + 1, rendered->audio.getNumSamples() - 1);
            if (a >= 0 && a < rendered->audio.getNumSamples()) for (int ch = 0; ch < b.getNumChannels(); ++ch) {
                const int sourceChannel = std::min(ch, rendered->audio.getNumChannels() - 1);
                const float x = rendered->audio.getSample(sourceChannel, a), y = rendered->audio.getSample(sourceChannel, next);
                const float corrected = x + float(sourcePosition - a) * (y - x);
                const float originalA = rendered->source->audio.getSample(sourceChannel, a), originalB = rendered->source->audio.getSample(sourceChannel, next);
                const float dry = preview ? originalA + float(sourcePosition - a) * (originalB - originalA) : b.getSample(ch, i);
                b.setSample(ch, i, original || bypass ? dry : dry + wet * (corrected - dry));
            }
        }
        if (preview) { previewPosition += b.getNumSamples(); if ((double(previewPosition.load()) - getLatencySamples()) / runtimeRate >= rendered->audio.getNumSamples() / rendered->rate) previewing = false; }
    }
    if (!forceBypass) audition.process(b, runtimeRate, controls.cents, controls.reference);
    processedSamples += uint64_t(b.getNumSamples());
    if (const auto write = traceFifo.write(1); write.blockSize1 + write.blockSize2 > 0) {
        float reference = controls.referenceMidi;
        const float source = realtime.detectedMidi();
        if (controls.followReference == 0 && source > 0) {
            const int sourceKey = juce::jlimit(0,127,int(std::lround(source)));
            const auto group = size_t(sourceKey % 12);
            const int key = sourceKey + controls.semitones[group];
            reference = controls.directShift
                ? float(source + controls.semitones[group] + (controls.globalCents + (controls.octaveMode ? controls.octaveCents[size_t(sourceKey)] : controls.cents[group])) / 100)
                : float(key + controls.cents[size_t((key % 12 + 12) % 12)] / 100 + 12 * std::log2(controls.reference / 440));
        } else if (reference > 0 && source > 0 && controls.preserveOctave) reference += float(12 * std::round((source - reference) / 12));
        const double time = (sampleTime ? double(*sampleTime + b.getNumSamples()) : double(processedSamples)) / runtimeRate - 2048.0 / runtimeRate;
        trace[size_t(write.blockSize1 > 0 ? write.startIndex1 : write.startIndex2)] = {time, source, reference};
    }
    hazard.store(nullptr); inCallback.store(false);
}
juce::AudioProcessorParameter* PitchProcessor::getBypassParameter() const { return parameters.getParameter("bypass"); }
mp::Settings PitchProcessor::settings() const {
    auto p = [&](const juce::String& name) { return double(parameters.getRawParameterValue(name)->load()); };
    mp::Settings s; s.adaptive = p("adaptive") > 0.5; s.minimumIntervention = p("minimumIntervention") > 0.5; s.precisionCents = p("precision");
    for (int i = 0; i < 12; ++i) s.cents[size_t(i)] = p("cent" + juce::String(i));
    for (int i = 0; i < 12; ++i) s.semitones[size_t(i)] = int(p("step" + juce::String(i)));
    s.directShift = p("direct") > 0.5; s.globalCents=p("globalCents");
    s.matchReference = p("referenceMatch") > 0.5;
    for (int i = 0; i < 12; ++i) s.groupStrength[size_t(i)] = p("gq" + juce::String(i));
    for (int i = 0; i < 128; ++i) s.octaveCents[i] = p("oct" + juce::String(i));
    s.octaveMode = p("octave") > 0.5; s.formants = p("formants") > 0.5;
    s.strength = p("quantize"); s.tightness = p("tightness"); s.vibrato = p("vibrato");
    s.smoothingMs = p("smoothing"); s.humanize = p("humanize"); s.seed = int(p("seed")); s.reference = p("reference"); return s;
}
void PitchProcessor::setParameter(const juce::String& name, float value) {
    if (auto* p = parameters.getParameter(name)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(value)); p->endChangeGesture(); }
}
void PitchProcessor::applyCorrection() {
    setParameter("enabled", 1);
    // Explicit user action enables the entered targets. Merely loading a file
    // or moving a target must not silently change a saved 0% strength.
    if (liveParameters.matchReference->load() < 0.5f && liveParameters.direct->load() < 0.5f && liveParameters.strength->load() == 0) setParameter("quantize", 1);
    setParameter("bypass", 0); setParameter("original", 0); setParameter("wet", 1);
    if (liveParameters.follow->load() > 0) {
        setParameter("useRender", 0);
        setStatus("Sidechain referansi izleniyor. Yalniz bu kanal degisir; sonucu Sonar Bounce ile kaydedin."); return;
    }
    if (auto c = clip()) {
        setParameter("useRender", 1);
        needsRender = true; renderRequested=true;
        if (c->notes.empty() || (settings().adaptive && !c->adaptiveAnalysis)) analyse(); else render();
    } else {
        setParameter("useRender", 0);
        setStatus("Live correction active on DAW input. No WAV load, Capture or Render needed.");
    }
}
std::shared_ptr<const mp::Clip> PitchProcessor::clip() const { const juce::ScopedLock lock(modelLock); return model; }
void PitchProcessor::setStatus(juce::String s) { const juce::ScopedLock lock(statusLock); message = std::move(s); }
juce::String PitchProcessor::status() const { const juce::ScopedLock lock(statusLock); return message; }
std::shared_ptr<const PitchProcessor::Result> PitchProcessor::correctionResult() const {
    const juce::ScopedLock lock(resultLock); return latestResult;
}
void PitchProcessor::auditionTarget(int group,int octave,int sourceNote) {
    const auto s=settings();
    if(!s.directShift){auditionTone(group,octave);return;}
    auto c=clip(); int index=sourceNote;
    if(c && (index<0 || index>=int(c->notes.size()) || c->notes[size_t(index)].key%12!=group)) {
        index=-1;for(size_t i=0;i<c->notes.size();++i)if(c->notes[i].key%12==group){index=int(i);break;}
    }
    if(!c || index<0 || index>=int(c->notes.size())){setStatus("Goreli hedefi dinlemek icin once bu kaynak notayi analiz edin.");return;}
    mp::Settings constant=s;constant.smoothingMs=0;
    const auto& n=c->notes[size_t(index)];
    previewing=false; audition.frequency(mp::midiToHz(n.centre+mp::correctionAt(*c,constant,(n.start+n.end)/2)));
}
void PitchProcessor::publish(std::unique_ptr<Rendered> r) {
    auto* fresh = r.get(); retained.push_back(std::move(r)); current.store(fresh);
    const auto* active = hazard.load();
    std::erase_if(retained, [&](const auto& item) { return item.get() != fresh && item.get() != active; });
}
void PitchProcessor::install(std::shared_ptr<mp::Clip> c) {
    const juce::ScopedLock lock(modelLock); model = std::move(c); history.clear(); future.clear(); ++revision;
    // Previously rendered audio must not play against a new source.
    current.store(nullptr); needsRender = true; renderRequested=false;
    const juce::ScopedLock resultGuard(resultLock); latestResult.reset();
}
void PitchProcessor::load(const juce::File& f) {
    if (capturing || busy.exchange(true)) return;
    const double position=hostSeconds.load(); const bool known=hostPositionKnown.load();
    worker.addJob([this, f, position, known] { juce::String error; auto c=mp::loadAudio(f,error);
        if(c){install(c);setParameter("useRender",1);setParameter("offset",known?float(std::max(0.0,position)):0);
            setStatus("WAV yuklendi. Baslangic "+juce::String(known?position:0,3)+" s"+(known?" (DAW imleci). ":" (DAW konumu yok; gerekirse WAV baslangicini gir). ")+"Analiz et, farki incele, Uygula.");}
        else setStatus(error); progress=0;busy=false; });
}
void PitchProcessor::analyse() {
    if (capturing) return;
    if (busy.exchange(true)) return;
    auto c = clip(); if (!c) { busy = false; setStatus("Load or capture audio first"); return; }
    cancelRequested = false; const bool adaptive = settings().adaptive; const bool applyAfter=renderRequested.exchange(false);
    worker.addJob([this, c, adaptive, applyAfter] {
        setStatus("Analyzing monophonic pitch offline..."); auto next = std::make_shared<mp::Clip>(*c);
        auto update = [this](double p) { progress = p; return !cancelRequested.load(); };
        if (adaptive) mp::analyseAdaptive(*next, update); else mp::analyse(*next, update);
        if (cancelRequested) { setStatus("Analiz iptal edildi; onceki kaynak korundu."); busy = false; return; }
        install(next); renderRequested=applyAfter;
        if (next->notes.empty()) { needsRender = false; setStatus("No reliable notes detected. Use a longer, isolated instrument or vocal; correction was not applied."); }
        else setStatus(juce::String(next->notes.size()) + " bolge olculdu. Onerinin dayanagini ve cent farklarini inceleyin; Uygula ile sesi degistirin.");
        busy = false;
    });
}
void PitchProcessor::render() {
    if (capturing) return;
    if (busy.exchange(true)) return;
    auto c = clip(); if (!c) { busy = false; return; }
    if (c->notes.empty()) { needsRender = false; busy = false; setStatus("Analyze the source first: no reliable note map is available for correction."); return; }
    const auto s = settings();
    needsRender = false; cancelRequested = false; renderRequested=false;
    worker.addJob([this, c, s] {
        setStatus("Rendering corrected audio offline..."); auto r = std::make_unique<Rendered>(); r->rate = c->sampleRate; r->anchor = c->anchorSeconds; r->source = c;
        mp::CorrectionPlan plan; auto update = [this](double p) { progress = p; return !cancelRequested.load(); };
        r->audio = s.adaptive ? mp::renderAdaptive(*c, s, update, &plan) : mp::render(*c, s, update);
        if (cancelRequested || r->audio.getNumSamples() != c->audio.getNumSamples()) { setStatus("Render iptal edildi; kismi ses yayinlanmadi."); busy = false; return; }
        auto verified=std::make_shared<Result>();verified->plan=std::move(plan);verified->source=c;verified->fingerprint=mp::settingsToTree(s).toXmlString();
        {const juce::ScopedLock lock(resultLock);latestResult=verified;}
        publish(std::move(r)); lastRenderedSettings = s; renderedNotify = true;
        setStatus(s.adaptive ? verified->plan.summary() : "Render ready. Map playback uses edited notes; Live input processes incoming audio."); busy = false;
    });
}
void PitchProcessor::refreshHostClips() {
#if JucePlugin_Enable_ARA
    if (araConnected() && !busy && !capturing) if (auto* renderer = getPlaybackRenderer<PitchARAPlaybackRenderer>()) {
        renderer->connectProcessor(*this);
        juce::String error;
        if (auto c = renderer->readHostClips(error)) {
            bool preserved = false;
            if (restoredClipPending) if (auto previous = clip()) {
                if (previous->sampleRate == c->sampleRate && std::abs(previous->anchorSeconds - c->anchorSeconds) < 1e-6
                    && previous->audio.getNumSamples() == c->audio.getNumSamples()) {
                    bool same = true;
                    for (int ch = 0; ch < c->audio.getNumChannels() && same; ++ch)
                        same = previous->audio.getNumChannels() == c->audio.getNumChannels()
                            && std::memcmp(previous->audio.getReadPointer(ch), c->audio.getReadPointer(ch), size_t(c->audio.getNumSamples()) * sizeof(float)) == 0;
                    if (same) { c->notes = previous->notes; c->frames = previous->frames; c->adaptiveAnalysis = previous->adaptiveAnalysis; preserved = true; }
                }
            }
            restoredClipPending = false; install(c); setParameter("useRender", 1);
            if (preserved) { if (liveParameters.enabled->load() > 0.5f) render(); }
            else analyse();
        }
        else if (error.isNotEmpty()) setStatus(error);
        else if (!renderer->hasClips() && clip()) { install(nullptr); setStatus("ARA: Atanmis klip kalmadi."); }
        if (renderedNotify.exchange(false)) renderer->notifyRendered();
    }
#endif
}
void PitchProcessor::timerCallback() {
    const int latency = araConnected() && liveParameters.follow->load() == 0 ? 0 : realtime.latency();
    if (latency != getLatencySamples()) setLatencySamples(latency);
    refreshHostClips();
    if (captureFull.exchange(false)) capture(false);
    const auto fingerprint = mp::settingsToTree(settings()).toXmlString();
    if (fingerprint != settingsFingerprint) {
        if(settingsFingerprint.isNotEmpty() && clip() && !busy && !renderRequested)
            setStatus("Hedefler degisti; yeni degerler henuz sese uygulanmadi. Oneriyi inceleyip Uygula.");
        settingsFingerprint=fingerprint;needsRender=true;
    }
    if (!busy && !capturing && !cancelRequested && clip() && needsRender && renderRequested && liveParameters.follow->load() == 0 && (liveParameters.enabled->load() > 0.5f || previewing) && (liveParameters.useRender->load() > 0.5f || previewing)) render();
}
void PitchProcessor::capture(bool start) {
    if (start) {
        if (busy || capturing) return;
        captureBuffer.setSize(2, int(std::min(runtimeRate * mp::maxSeconds, double(mp::maxAudioBytes / 8)))); captureBuffer.clear();
        captured = 0; captureFull = false; capturing = true; setStatus("Capturing DAW input (maximum 600 seconds). Stop capture before Analyze.");
    } else {
        capturing = false;
        if (busy.exchange(true)) return;
        worker.addJob([this] {
            while (inCallback.load()) juce::Thread::yield();
            const int count = captured.load();
            if (count > 0) { auto c = std::make_shared<mp::Clip>(); c->sampleRate = runtimeRate; c->anchorSeconds = captureAnchor; c->name = "Captured Sonar track"; c->audio.setSize(2, count); for (int ch = 0; ch < 2; ++ch) c->audio.copyFrom(ch, 0, captureBuffer, ch, 0, count); install(c); setParameter("useRender",1);setParameter("offset",0);setStatus("Capture stored. Click Analyze."); }
            else setStatus("No audio captured. Start DAW playback while capture is active."); busy = false;
        });
    }
}
void PitchProcessor::preview(bool start) {
    if(start && busy)return;
    if(start && !hasRender())if(auto c=clip()) {
        auto r=std::make_unique<Rendered>();r->audio=c->audio;r->source=c;r->rate=c->sampleRate;r->anchor=c->anchorSeconds;publish(std::move(r));
    }
    if(!previewing && !inCallback)previewPosition=0;previewing=start;
}
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
    if (!root.hasType("MicrotonalPitchMapPro") || int(root["version"]) < 1 || int(root["version"]) > mp::stateVersion) return;
    // Restore source/map synchronously; expensive audio rendering remains on the worker.
    cancelRequested = true;
    worker.removeAllJobs(true, -1);
    cancelRequested = false;
    auto state = root.getChildWithName("Parameters"); if (state.isValid()) parameters.replaceState(state);
    if (int(root["version"]) < 4) setParameter("globalCents",0);
    if (int(root["version"]) < 3) setParameter("adaptive", 0);
    if (int(root["version"]) == 1) {
        setParameter("direct", 0); setParameter("referenceMatch", 0); setParameter("enabled", 1);
        for (int i = 0; i < 12; ++i) setParameter("step" + juce::String(i), 0);
    }
    install(mp::clipFromTree(root.getChildWithName("Clip"))); renderRequested=clip()!=nullptr && liveParameters.enabled->load()>0.5f; restoredClipPending = clip() != nullptr; busy = false; setStatus("Project restored; offline render will refresh.");
}
std::vector<PitchProcessor::TracePoint> PitchProcessor::takeTrace() {
    std::vector<TracePoint> result;
    const auto read = traceFifo.read(traceFifo.getNumReady());
    result.reserve(size_t(read.blockSize1 + read.blockSize2));
    for (int i = 0; i < read.blockSize1; ++i) result.push_back(trace[size_t(read.startIndex1 + i)]);
    for (int i = 0; i < read.blockSize2; ++i) result.push_back(trace[size_t(read.startIndex2 + i)]);
    return result;
}
void PitchProcessor::importScale(const juce::File& file) {
    auto s = settings(); juce::String error;
    if (!mp::importScala(file, s, error)) { setStatus(error); return; }
    for (int i = 0; i < 12; ++i) setParameter("cent" + juce::String(i), float(s.cents[size_t(i)]));
    setStatus("12-step Scala scale imported with C as root.");
}
void PitchProcessor::exportAudio(const juce::File& f) {
    if (liveParameters.follow->load() > 0) { setStatus("Canli sidechain eslestirmesini Sonar Bounce/Render ile kaydedin. Referanssiz WAV uretimi yapilmadi."); return; }
    if (busy.exchange(true)) return;
    auto c = clip();
    if (!c) { busy = false; setStatus("Capture the Sonar input or load a WAV before exporting a file."); return; }
    cancelRequested = false;
    const float wet = liveParameters.wet->load();
    const bool dry = liveParameters.enabled->load() < 0.5f || liveParameters.bypass->load() > 0.5f || liveParameters.original->load() > 0.5f || wet == 0;
    auto* applied = current.load();
    if (!dry && (!applied || !correctionResult())) {
        busy = false; setStatus("Duzeltilmis WAV icin once ONERIYI UYGULA. Henuz islenmis sonuc yok."); return;
    }
    // Export the same immutable PCM as playback. Re-rendering current controls
    // here could silently apply a pending proposal or select a different engine.
    auto source = !dry ? applied->source : c;
    worker.addJob([this, source, applied, f, wet, dry] {
        auto audio = dry ? source->audio : applied->audio;
        if (cancelRequested) { setStatus("Disa aktarma iptal edildi; dosya yazilmadi."); busy = false; return; }
        if (!dry && wet < 1) for (int ch = 0; ch < audio.getNumChannels(); ++ch) for (int i = 0; i < audio.getNumSamples(); ++i) {
            const float original = source->audio.getSample(ch, i); audio.setSample(ch, i, original + wet * (audio.getSample(ch, i) - original));
        }
        setStatus(mp::writeWav(f, audio, source->sampleRate) ? "Duyulan son uygulanmis sonuc WAV olarak kaydedildi; bekleyen hedef degisiklikleri dahil degil." : "Export failed: choose a new writable filename"); busy = false;
    });
}
void PitchProcessor::exportMidi(const juce::File& f) { auto c = clip(); if (c) setStatus(mp::writeMidi(f, *c, settings()) ? "Exported MIDI with pitch-bend data (+/-2 semitone range)" : "MIDI export failed: choose a new filename"); }
void PitchProcessor::preset(const juce::File& f, bool save) {
    if (busy) return;
    if (save) { if (f.existsAsFile()) { setStatus("Choose a new preset filename"); return; } juce::MemoryBlock b; getStateInformation(b); setStatus(f.replaceWithData(b.getData(), b.getSize()) ? "Preset saved including source audio and map" : "Preset save failed"); }
    else { juce::MemoryBlock b; if (f.getSize() > 256 * 1024 * 1024 || !f.loadFileAsData(b)) { setStatus("Preset read failed"); return; } setStateInformation(b.getData(), int(b.getSize())); }
}
juce::AudioProcessorEditor* PitchProcessor::createEditor() { return new PitchEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PitchProcessor(); }
