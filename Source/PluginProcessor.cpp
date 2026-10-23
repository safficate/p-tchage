#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "LiveEditor.h"
#include "ARAIntegration.h"
#include "Localization.h"

namespace {
struct SharedReference {juce::String id,name;std::array<mp::TuningCentre,12> source;};
juce::CriticalSection referencesLock;
std::vector<SharedReference> sharedReferences;
}
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
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"workspace",1},"Live tuning workspace",true));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"referenceMode",1},"Workspace reference source",0,3,0));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"expression",1},"Musical transition behaviour",0,2,0));
    for(int i=0;i<12;++i){
        add("target"+juce::String(i),mp::noteName(i+60)+" workspace target cents",-2400,2400,.01f,0);
        add("relative"+juce::String(i),mp::noteName(i+60)+" migrated relative cents",-200,200,.01f,0);
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"targetOn"+juce::String(i),1},mp::noteName(i+60)+" manual target active",false));
    }
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"globalNotes",1},"Whole source semitones",-12,12,0));
    add("humanDepth","Expression deviation cents",0,20,.01f,2);
    add("humanTime","Expression deviation time ms",100,4000,1,700);
    add("finishCutoff","Repair filter cutoff Hz",300,18000,1,12000);
    add("finishAmount","Repair filter amount",0,1,.001f,.5f);
    add("finishStart","Repair region start seconds",0,36000,.001f,0);
    add("finishEnd","Repair region end seconds",0,36000,.001f,10);
    add("referenceBase","Reference alignment baseline cents",-200,200,.01f,0);
    add("uiScale","Interface scale percent",100,200,25,100);
    add("pitchAmount","Pitch correction amount",0,1,.001f,1);
    add("glideMs","Pitch smoother transition ms",5,500,1,80);
    add("centreMs","Pitch centre observation ms",20,1500,1,250);
    add("maxRate","Maximum correction slope cents per second",50,5000,1,1200);
    add("centreNarrow","Centre narrowing",0,1,.001f,0);
    add("vibratoKeep","Preserve vibrato",0,1,.001f,1);
    add("toneDepth","Tone maximum cut dB",0,12,.1f,4);
    add("toneThreshold","Tone prominence threshold dB",0,24,.1f,7);
    add("toneAttack","Tone attack ms",1,250,1,25);
    add("toneRelease","Tone release ms",20,1500,1,180);
    add("toneLow","Tone lower frequency Hz",80,4000,1,180);
    add("toneHigh","Tone upper frequency Hz",1000,16000,1,9000);
    add("toneMix","Tone smoother mix",0,1,.001f,1);
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"centreNote",1},"Alignment centre note",0,11,0));
    for(auto id:{"natural","pitchOn","pitchAuto","toneAuto","harmonicGuard","attackGuard","toneRange0","toneRange1","toneRange2","toneRange3","toneRange4","humanAuto","finishAuto"})
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id,1},id,true));
    for(auto id:{"toneOn","amountAuto","complete","humanOn","finishOn","finishRegion"})
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id,1},id,false));
    for(int i=0;i<12;++i){add("source"+juce::String(i),"Manual source cents",-200,200,.01f,0);p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"sourceOn"+juce::String(i),1},"Manual source present",false));}
    for (auto id : {"octave", "formants", "useRender", "bypass", "original"})
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id, 1}, id, false));
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"uiLanguage",1},"Interface language",0,mp::languageCount-1,0));
    return p;
}
PitchProcessor::PitchProcessor(bool legacyDefaults) : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withInput("Reference (sidechain)", juce::AudioChannelSet::stereo(), false).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
parameters(*this, nullptr, "Parameters", layout()) {
    auto raw = [this](const juce::String& id) { return parameters.getRawParameterValue(id); };
    for (int i = 0; i < 12; ++i) { liveParameters.cents[size_t(i)] = raw("cent" + juce::String(i)); liveParameters.groups[size_t(i)] = raw("gq" + juce::String(i)); liveParameters.steps[size_t(i)] = raw("step" + juce::String(i)); }
    for (int i = 0; i < 128; ++i) liveParameters.octaves[size_t(i)] = raw("oct" + juce::String(i));
    for(int i=0;i<12;++i){liveParameters.targets[size_t(i)]=raw("target"+juce::String(i));liveParameters.targetOn[size_t(i)]=raw("targetOn"+juce::String(i));liveParameters.relative[size_t(i)]=raw("relative"+juce::String(i));}
    auto& p = liveParameters;
    p.strength = raw("quantize"); p.tightness = raw("tightness"); p.vibrato = raw("vibrato"); p.smoothing = raw("smoothing");
    p.humanize = raw("humanize"); p.seed = raw("seed"); p.reference = raw("reference"); p.wet = raw("wet");
    p.octave = raw("octave"); p.formants = raw("formants"); p.bypass = raw("bypass"); p.original = raw("original");
    p.useRender = raw("useRender"); p.offset = raw("offset");
    p.direct = raw("direct");
    p.globalCents=raw("globalCents"); p.enabled = raw("enabled"); p.matchReference = raw("referenceMatch"); p.follow = raw("follow"); p.preserveOctave = raw("preserveOctave");
    p.workspace=raw("workspace");p.referenceMode=raw("referenceMode");p.expression=raw("expression");
    smooth.complete=raw("complete");smooth.referenceBase=raw("referenceBase");smooth.globalNotes=raw("globalNotes");smooth.humanOn=raw("humanOn");smooth.humanAuto=raw("humanAuto");smooth.humanDepth=raw("humanDepth");smooth.humanTime=raw("humanTime");smooth.finishOn=raw("finishOn");smooth.finishAuto=raw("finishAuto");smooth.finishRegion=raw("finishRegion");smooth.finishCutoff=raw("finishCutoff");smooth.finishAmount=raw("finishAmount");smooth.finishStart=raw("finishStart");smooth.finishEnd=raw("finishEnd");
    if(legacyDefaults){setParameter("workspace",0);setParameter("formants",1);}
    instanceId=juce::Uuid().toString();
    smooth.natural=raw("natural");smooth.pitchOn=raw("pitchOn");smooth.pitchAuto=raw("pitchAuto");smooth.glide=raw("glideMs");smooth.centre=raw("centreMs");smooth.rate=raw("maxRate");smooth.amount=raw("pitchAmount");smooth.amountAuto=raw("amountAuto");smooth.narrow=raw("centreNarrow");smooth.vibrato=raw("vibratoKeep");
    smooth.toneOn=raw("toneOn");smooth.toneAuto=raw("toneAuto");smooth.depth=raw("toneDepth");smooth.threshold=raw("toneThreshold");smooth.attack=raw("toneAttack");smooth.release=raw("toneRelease");smooth.low=raw("toneLow");smooth.high=raw("toneHigh");smooth.mix=raw("toneMix");smooth.harmonicGuard=raw("harmonicGuard");smooth.attackGuard=raw("attackGuard");
    for(int i=0;i<5;++i)smooth.ranges[size_t(i)]=raw("toneRange"+juce::String(i));
    for(auto* parameter:getParameters())if(auto* id=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))parameters.addParameterListener(id->paramID,this);
    updateReferenceRegistry();startTimerHz(20);
}
PitchProcessor::~PitchProcessor() { stopTimer();for(auto* parameter:getParameters())if(auto* id=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))parameters.removeParameterListener(id->paramID,this);assistantWorker.removeAllJobs(true,-1);{const juce::ScopedLock lock(referencesLock);std::erase_if(sharedReferences,[this](const auto& r){return r.id==instanceId;});} cancelRequested = true; worker.removeAllJobs(true, -1); }
bool PitchProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto reference = l.inputBuses.size() > 1 ? l.inputBuses[1] : juce::AudioChannelSet::disabled();
    return l.getMainInputChannelSet() == l.getMainOutputChannelSet() && (l.getMainOutputChannelSet() == juce::AudioChannelSet::mono() || l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo())
        && (reference.isDisabled() || reference == juce::AudioChannelSet::mono() || reference == juce::AudioChannelSet::stereo());
}
void PitchProcessor::prepareToPlay(double sr, int block) {
    worker.removeAllJobs(true,-1);memoryJob=false;busy=false;audioMemory.prepare(sr);cacheBlend=0;cacheLatch=previousPlaying=false;previousEnd=-1;cachePlaying=false;preparedReady=false;
    runtimeRate = sr; previewPosition = 0; capturing = false; captured = 0;
    captureBuffer.setSize(0, 0);
    realtime.prepare(sr, getMainBusNumOutputChannels());toneSmoother.prepare(sr);finishFilter.prepare(sr);
    {const juce::ScopedLock guard(assistantCaptureLock);assistantCaptureState=0;assistantCaptured=0;assistantCapture.setSize(2,int(sr*12));assistantCapture.clear();}
    referenceAnalyzer.prepare(sr, 2); processedSamples = 0; traceFifo.reset();lastSourceSerial=lastReferenceSerial=0;
    setLatencySamples(realtime.latency());
#if JucePlugin_Enable_ARA
    if (prepareToPlayForARA(sr, block, getMainBusNumOutputChannels(), getProcessingPrecision())) setLatencySamples(0);
#endif
    if(liveWorkspaceEnabled()&&smooth.complete->load()>.5f&&clip()){
        preparedDirty=true;assembleRequested=false;preparePerformance();
    }
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
    if(cacheResetRequested.exchange(false)){cacheBlend=0;cacheLatch=previousPlaying=false;previousEnd=-1;cachePlaying=false;}
    const auto position = getPlayHead() ? getPlayHead()->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo>{};
    const auto sampleTime = position ? position->getTimeInSamples() : juce::Optional<int64_t>{};
    const bool playing = position && position->getIsPlaying();hostPlaying=playing;
    if(sampleTime) { hostSeconds.store(double(*sampleTime)/runtimeRate); hostPositionKnown=true; }else hostSeconds.store(double(processedSamples)/runtimeRate);
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
    if(liveWorkspaceEnabled()&&smooth.complete->load()<.5f&&(playing||!position))
        audioMemory.push(b,sampleTime?*sampleTime:int64_t(processedSamples));
    if(assistantCaptureState.load()==1&&(playing||!position)){assistantCaptured=0;assistantCaptureState=2;}

    Rendered* rendered;
    do { rendered = current.load(); hazard.store(rendered); } while (rendered != current.load());
    if(liveWorkspaceEnabled()&&preparedReady.load()&&rendered&&rendered->modern&&sampleTime&&playing){
        const double first=double(*sampleTime)/runtimeRate-rendered->anchor;double energy=0;int compared=0;
        for(int i=0;i<b.getNumSamples();i+=8){const double at=(first+i/runtimeRate)*rendered->rate;if(at<0||at>=rendered->source->audio.getNumSamples()-1)continue;const int a=int(at),next=a+1;for(int ch=0;ch<b.getNumChannels();++ch){const int c=std::min(ch,rendered->source->audio.getNumChannels()-1);const float x=rendered->source->audio.getSample(c,a),y=rendered->source->audio.getSample(c,next);const double delta=b.getSample(ch,i)-(x+(at-a)*(y-x));energy+=delta*delta;++compared;}}
        if(compared&&energy/compared>1e-7){sourceChanged=true;preparedReady=false;}
    }
    const auto value = [](const std::atomic<float>* p) { return p->load(std::memory_order_relaxed); };
    const auto& p = liveParameters;
    const bool preview = previewing.load();
    const bool modern=value(p.workspace)>.5f;
    const bool bypass = forceBypass || value(p.bypass) > 0.5f || (!modern && value(p.enabled) < 0.5f);
    const bool original = value(p.original) > 0.5f;
    const bool cached = rendered && !modern && (preview || (value(p.follow) == 0 && (araConnected() || value(p.useRender) > 0.5f) && playing && sampleTime));
    mp::RealtimePitch::Controls controls;
    for (size_t i = 0; i < 12; ++i) { controls.cents[i] = value(p.cents[i]); controls.groupStrength[i] = value(p.groups[i]); controls.semitones[i] = int(value(p.steps[i])); }
    controls.directShift = value(p.direct) > 0.5f; controls.globalCents=value(p.globalCents)+100*value(smooth.globalNotes);
    controls.matchReference = value(p.matchReference) > 0.5f;
    controls.followReference = int(value(p.follow)); controls.referenceMidi = sidechainPitch.load(); controls.preserveOctave = value(p.preserveOctave) > 0.5f;
    for (size_t i = 0; i < 128; ++i) controls.octaveCents[i] = value(p.octaves[i]);
    controls.strength = value(p.strength); controls.tightness = value(p.tightness); controls.vibrato = value(p.vibrato);
    controls.smoothingMs = value(p.smoothing); controls.humanize = value(p.humanize); controls.seed = int(value(p.seed));
    controls.reference = value(p.reference); controls.wet = value(p.wet); controls.octaveMode = value(p.octave) > 0.5f;
    controls.formants = value(p.formants) > 0.5f; controls.dry = bypass || original || cached;
    if(modern){
        controls.modernTargets=true;controls.natural=value(smooth.natural)>.5f;controls.adaptiveTransitions=controls.natural;
        controls.pitchSmooth=value(smooth.pitchOn)>.5f;controls.pitchAuto=value(smooth.pitchAuto)>.5f;controls.glideMs=value(smooth.glide);controls.centreMs=value(smooth.centre);controls.maxRate=value(smooth.rate);controls.amount=value(smooth.amount);controls.amountAuto=value(smooth.amountAuto)>.5f;controls.centreTightness=value(smooth.narrow);controls.vibratoKeep=value(smooth.vibrato);
        controls.humanOn=value(smooth.humanOn)>.5f;controls.humanAuto=value(smooth.humanAuto)>.5f;controls.humanDepth=value(smooth.humanDepth);controls.humanTime=value(smooth.humanTime);
        controls.timelineSeconds=sampleTime?double(*sampleTime)/runtimeRate:double(processedSamples)/runtimeRate;controls.expression=int(value(p.expression));
        controls.directShift=false;controls.matchReference=false;controls.followReference=0;
        const int mode=int(value(p.referenceMode));
        const bool referenceOnline=sidechainPitch.load()>0&&referenceAnalyzer.confidence()>.8f;
        for(int i=0;i<12;++i){const auto j=size_t(i);
            controls.sourceKnown[j]=workspace.sourceKnown(i);controls.learnedSource[j]=workspace.sourceCents(i);
            controls.targets[j]=value(p.targets[j]);controls.targetActive[j]=value(p.targetOn[j])>.5f;controls.relativeCents[j]=value(p.relative[j]);
            if(mode>0){controls.semitones[j]=0;controls.relativeCents[j]=0;controls.targetActive[j]=workspace.referenceKnown(i)&&(mode==2||referenceOnline);controls.targets[j]=workspace.referenceCents(i)-value(smooth.referenceBase);if(mode==3){controls.targetActive[j]=linkedKnown[j].load();controls.targets[j]=linkedCents[j].load()-value(smooth.referenceBase);}}
        }
        double local=0;const double at=sampleTime?double(*sampleTime)/runtimeRate:double(processedSamples)/runtimeRate;
        if(workspace.correctionAt(at,local))controls.regionCents=local;
    }

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
    const bool postBypass=bypass||original||controls.wet==0;
    if(postBypass&&!postWasBypassed){toneSmoother.reset();finishFilter.reset();}postWasBypassed=postBypass;
    if(!postBypass){
        mp::ToneSmoother::Controls t;t.enabled=value(smooth.toneOn)>.5f;t.automatic=value(smooth.toneAuto)>.5f;t.depth=value(smooth.depth);t.threshold=value(smooth.threshold);t.attackMs=value(smooth.attack);t.releaseMs=value(smooth.release);t.lowHz=value(smooth.low);t.highHz=value(smooth.high);t.mix=value(smooth.mix)*controls.wet;t.harmonicGuard=value(smooth.harmonicGuard)>.5f;t.attackGuard=value(smooth.attackGuard)>.5f;
        for(int i=0;i<5;++i)t.ranges[size_t(i)]=value(smooth.ranges[size_t(i)])>.5f;
        const float detected=realtime.detectedMidi();toneSmoother.process(b,t,detected>0?float(mp::midiToHz(detected+realtime.appliedCents()/100)):0);
    }
    mp::FinishFilter::Controls f;f.enabled=!bypass&&!original&&value(smooth.finishOn)>.5f;f.automatic=value(smooth.finishAuto)>.5f;f.region=value(smooth.finishRegion)>.5f;f.start=value(smooth.finishStart);f.end=value(smooth.finishEnd);f.cutoff=value(smooth.finishCutoff);f.amount=value(smooth.finishAmount)*controls.wet;
    const double outputTime=(sampleTime?double(*sampleTime):double(processedSamples))/runtimeRate-getLatencySamples()/runtimeRate;
    if(!postBypass)finishFilter.process(b,f,outputTime,realtime.detectedMidi()>0?float(mp::midiToHz(realtime.detectedMidi())):0);
    if(modern&&rendered&&rendered->modern){
        const bool seek=sampleTime&&previousEnd>=0&&std::abs(*sampleTime-previousEnd)>b.getNumSamples();
        const bool preparedValid=preparedReady.load()&&readyParameterGeneration.load()==parameterGeneration.load();
        if(!preparedValid)cacheLatch=false;
        if(preparedValid&&playing&&(!previousPlaying||seek))cacheLatch=true;
        const bool wanted=preparedValid&&cacheLatch&&playing&&sampleTime&&!bypass&&!original;
        if(bypass||original||controls.wet==0||!playing)cacheBlend=0;
        const double delay=getLatencySamples()/runtimeRate;
        for(int i=0;i<b.getNumSamples();++i){const double t=(sampleTime?double(*sampleTime+i):double(processedSamples+i))/runtimeRate-rendered->anchor-delay,at=t*rendered->rate;
            const bool inside=at>=0&&at<rendered->audio.getNumSamples();const double goal=wanted&&inside?1:0;cacheBlend+=(goal-cacheBlend)*(1-std::exp(-1/(runtimeRate*.03)));
            if(inside&&cacheBlend>1e-7){const int a=int(at),next=std::min(a+1,rendered->audio.getNumSamples()-1);const float fraction=float(at-a);
                for(int ch=0;ch<b.getNumChannels();++ch){const int c=std::min(ch,rendered->audio.getNumChannels()-1);const float x=rendered->audio.getSample(c,a),y=rendered->audio.getSample(c,next),s=rendered->source->audio.getSample(c,a),e=rendered->source->audio.getSample(c,next);const float raw=s+fraction*(e-s),wet=x+fraction*(y-x),v=b.getSample(ch,i);b.setSample(ch,i,float(v+cacheBlend*(raw+controls.wet*(wet-raw)-v)));}
            }
        }
        cachePlaying=cacheBlend>.001;
        if(cachePlaying){const double t=(sampleTime?double(*sampleTime):double(processedSamples))/runtimeRate-rendered->anchor-delay;cachedCents=float(rendered->plan.segments.empty()?mp::correctionAt(*rendered->source,rendered->settings,t)*100:rendered->plan.at(t));const auto& frames=rendered->source->frames;const auto at=std::lower_bound(frames.begin(),frames.end(),t,[](const auto& f,double time){return f.time<time;});cachedMidi=at!=frames.end()?float(at->midi):0;}
    }else cachePlaying=false;
    previousPlaying=playing;previousEnd=sampleTime?*sampleTime+b.getNumSamples():-1;
    if(assistantCaptureState.load()==2&&(playing||!position)){const int offset=assistantCaptured.load();const int count=std::min(b.getNumSamples(),assistantCapture.getNumSamples()-offset);for(int ch=0;ch<2;++ch)assistantCapture.copyFrom(ch,offset,b,std::min(ch,b.getNumChannels()-1),0,count);assistantCaptured=offset+count;if(offset+count>=assistantCapture.getNumSamples())assistantCaptureState=3;}
    if (!forceBypass) audition.process(b, runtimeRate, controls.cents, controls.reference);
    processedSamples += uint64_t(b.getNumSamples());
    if(modern&&(playing||!position)&&(realtime.analysisSerial()!=lastSourceSerial||referenceAnalyzer.analysisSerial()!=lastReferenceSerial)){
        lastSourceSerial=realtime.analysisSerial();lastReferenceSerial=referenceAnalyzer.analysisSerial();
        const double time=std::max(0.0,(sampleTime?double(*sampleTime+b.getNumSamples()):double(processedSamples))/runtimeRate-2048.0/runtimeRate);
        workspace.push({time,realtime.detectedMidi(),realtime.detectedCentre(),sidechainPitch.load(),referenceAnalyzer.detectedCentre(),realtime.confidence(),referenceAnalyzer.confidence()});
    }

    if (const auto write = traceFifo.write(1); write.blockSize1 + write.blockSize2 > 0) {
        float reference = controls.referenceMidi;
        const float source = realtime.detectedMidi();
        if(modern&&source>0){
            const int key=juce::jlimit(0,127,int(std::lround(source)));const auto group=size_t(key%12);
            reference=controls.targetActive[group]?float(key+controls.semitones[group]+(controls.targets[group]+controls.globalCents+controls.regionCents+controls.relativeCents[group])/100):float(source+(controls.globalCents+controls.regionCents+controls.relativeCents[group])/100+controls.semitones[group]);
        }else if (controls.followReference == 0 && source > 0) {
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
int PitchProcessor::uiLanguage() const {return juce::jlimit(0,mp::languageCount-1,int(parameters.getRawParameterValue("uiLanguage")->load()));}
juce::String PitchProcessor::status() const { const juce::ScopedLock lock(statusLock); return mp::localizeStatus(message,uiLanguage()); }
void PitchProcessor::appendConversation(const juce::String& role,const juce::String& text){workspace.appendConversation(mp::localize(role,uiLanguage()),role=="Sistem"?mp::localizeStatus(text,uiLanguage()):text);}
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
    if(liveWorkspaceEnabled()){
        if(sourceChanged.exchange(false)){setParameter("complete",0);preparedReady=false;audioMemory.clear();setStatus("Kanal sesi degisti. Bastan okuyup tum parcayi yeniden isaretleyin; eski hazir ses kullanilmiyor.");}
        workspace.drain();updateReferenceRegistry();
        if(!memoryJob.exchange(true))worker.addJob([this]{audioMemory.drain();if(preparedReady.load()&&audioMemory.revision()!=preparedAudioRevision){preparedReady=false;preparedDirty=true;assembleRequested=true;}memoryJob=false;});
        if(parameters.getRawParameterValue("complete")->load()>.5f&&preparedDirty.load()&&!busy.load())preparePerformance();
        if(realtime.analysisSerial()!=activitySerial&&juce::Time::getMillisecondCounterHiRes()-lastActivityWall>450){lastActivityWall=juce::Time::getMillisecondCounterHiRes();activitySerial=realtime.analysisSerial();
            if(realtime.detectedMidi()>0){const int key=int(std::lround(realtime.detectedMidi()));
                const juce::ScopedLock lock(statusLock);
                activity.add(juce::String(hostSeconds.load(),3)+" s | "+mp::noteName(key)+" | DSP "+juce::String(realtime.appliedCents(),2)+" c | gecis "+juce::String(realtime.smoothingTimeMs(),0)+" ms | miktar "+juce::String(realtime.effectiveAmount()*100,0)+"%"+(realtime.reusedAnalysis()?" | hafiza":""));
                while(activity.size()>160)activity.remove(0);
            }
        }
        return;
    }
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
    root.addChild(workspace.save(),-1,nullptr);{const juce::ScopedLock lock(referencesLock);root.setProperty("instanceId",instanceId,nullptr);root.setProperty("referenceId",referenceId,nullptr);}
    root.addChild(parameters.copyState(), -1, nullptr); auto c = clip(); if (c) root.addChild(mp::clipToTree(*c), -1, nullptr);
    juce::MemoryOutputStream stream(block, false); root.writeToStream(stream);
}
void PitchProcessor::setStateInformation(const void* data, int bytes) {
    if (bytes <= 0 || bytes > 256 * 1024 * 1024) return;
    const auto root = juce::ValueTree::readFromData(data, size_t(bytes));
    if (!root.hasType("MicrotonalPitchMapPro") || int(root["version"]) < 1 || int(root["version"]) > mp::stateVersion) return;
    // Restore source/map synchronously; expensive audio rendering remains on the worker.
    stopTimer();
    cancelRequested = true;
    worker.removeAllJobs(true, -1);
    memoryJob=false;preparedReady=false;preparedDirty=false;assembleRequested=false;cachePlaying=false;sourceChanged=false;cacheResetRequested=true;
    ++parameterGeneration;audioMemory.clear();audioMemory.drain();
    cancelRequested = false;
    auto state = root.getChildWithName("Parameters"); if (state.isValid()) parameters.replaceState(state);
    if(!state.getChildWithProperty("id","uiLanguage").isValid())setParameter("uiLanguage",0);
    legacyProject=int(root["version"])<6;
    workspace.restore(root.getChildWithName("LiveWorkspace"));
    {const juce::ScopedLock lock(referencesLock);const auto restoredId=root.getProperty("instanceId").toString();
        if(restoredId.isNotEmpty()&&std::none_of(sharedReferences.begin(),sharedReferences.end(),[&](const auto& r){return r.id==restoredId&&r.id!=instanceId;})){std::erase_if(sharedReferences,[this](const auto& r){return r.id==instanceId;});instanceId=restoredId;}
        referenceId=root.getProperty("referenceId").toString();}
    updateReferenceRegistry();
    if(int(root["version"])<5)setParameter("workspace",0);
    if (int(root["version"]) < 4) setParameter("globalCents",0);
    if (int(root["version"]) < 3) setParameter("adaptive", 0);
    if (int(root["version"]) == 1) {
        setParameter("direct", 0); setParameter("referenceMatch", 0); setParameter("enabled", 1);
        for (int i = 0; i < 12; ++i) setParameter("step" + juce::String(i), 0);
    }
    install(mp::clipFromTree(root.getChildWithName("Clip"))); renderRequested=clip()!=nullptr && liveParameters.enabled->load()>0.5f; restoredClipPending = clip() != nullptr; busy = false; setStatus("Project restored; offline render will refresh.");
    if(liveWorkspaceEnabled()&&smooth.complete->load()>.5f&&clip()){
        preparedDirty=true;preparePerformance();
    }
    startTimerHz(20);
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
juce::AudioProcessorEditor* PitchProcessor::createEditor() { return legacyProject&&!liveWorkspaceEnabled()?static_cast<juce::AudioProcessorEditor*>(new PitchEditor(*this)):static_cast<juce::AudioProcessorEditor*>(new LiveEditor(*this)); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PitchProcessor(); }

void PitchProcessor::enterLiveWorkspace(){
    if(liveWorkspaceEnabled())return;
    const bool enabled=liveParameters.enabled->load()>.5f;
    const bool relative=liveParameters.direct->load()>.5f;
    if(enabled)for(int i=0;i<12;++i){
        if(relative)setParameter("relative"+juce::String(i),liveParameters.cents[size_t(i)]->load());
        else{setParameter("target"+juce::String(i),liveParameters.cents[size_t(i)]->load());setParameter("targetOn"+juce::String(i),1);}
    }
    setParameter("workspace",1);setParameter("octave",0);setParameter("useRender",0);
    if(auto c=clip())workspace.analyseFile(*c);
    setStatus("Canli hedefler etkin. Cent degerleri hemen uygulanir.");
}
void PitchProcessor::setTargetCentre(int group,double cents){
    if(group<0||group>=12||!std::isfinite(cents))return;
    enterLiveWorkspace();
    setParameter("target"+juce::String(group),float(cents-liveParameters.globalCents->load()));
    setParameter("relative"+juce::String(group),0);setParameter("targetOn"+juce::String(group),1);
}
void PitchProcessor::setMeasuredCentre(int group,double cents){
    if(group<0||group>=12||!std::isfinite(cents))return;
    setParameter("source"+juce::String(group),float(cents));setParameter("sourceOn"+juce::String(group),1);
    setTargetCentre(group,cents+liveParameters.globalCents->load());
}
void PitchProcessor::equalizeMeasured(){
    enterLiveWorkspace();workspace.drain();const auto data=workspace.snapshot();
    for(int i=0;i<12;++i){const bool manual=parameters.getRawParameterValue("sourceOn"+juce::String(i))->load()>.5f;
        if(manual||data.source[size_t(i)].known){const double cents=manual?parameters.getRawParameterValue("source"+juce::String(i))->load():data.source[size_t(i)].cents;setTargetCentre(i,cents+liveParameters.globalCents->load());}}
}
void PitchProcessor::setReferenceMode(int mode){enterLiveWorkspace();setParameter("referenceMode",float(juce::jlimit(0,3,mode)));}
void PitchProcessor::editLiveRegion(double start,double end,double cents){workspace.edit({start,end,cents});++parameterGeneration;preparedReady=false;preparedDirty=true;}
void PitchProcessor::resetLiveLearning(bool referenceOnly){workspace.clear(referenceOnly);if(!referenceOnly){audioMemory.clear();preparedReady=false;assembleRequested=true;}realtime.clearAnalysisCache();referenceAnalyzer.clearAnalysisCache();setParameter("complete",0);}
double PitchProcessor::targetForSource(int group,double midi)const{
    if(group<0||group>=12)return midi;
    const auto i=size_t(group);const double base=std::round(midi);
    const int mode=int(liveParameters.referenceMode->load());
    if(mode==3&&linkedKnown[i])return base+linkedCents[i]/100+(liveParameters.globalCents->load()-smooth.referenceBase->load())/100+smooth.globalNotes->load();
    if(mode>0&&mode<3&&workspace.referenceKnown(group))return base+workspace.referenceCents(group)/100+(liveParameters.globalCents->load()-smooth.referenceBase->load())/100+smooth.globalNotes->load();
    return (liveParameters.targetOn[i]->load()>.5f?base+liveParameters.targets[i]->load()/100:midi)+liveParameters.globalCents->load()/100+smooth.globalNotes->load()+liveParameters.steps[i]->load()+liveParameters.relative[i]->load()/100;
}
void PitchProcessor::loadReference(const juce::File& file){
    if(busy.exchange(true))return;
    worker.addJob([this,file]{juce::String error;auto c=mp::loadAudio(file,error);if(c){mp::analyse(*c);workspace.analyseFile(*c,true);setReferenceMode(2);}else setStatus(error);busy=false;});
}
void PitchProcessor::updateTrackProperties(const TrackProperties& p){const juce::ScopedLock lock(referencesLock);trackName=p.name.value_or(juce::String{});}
void PitchProcessor::updateReferenceRegistry(){
    const auto data=workspace.snapshot();const juce::ScopedLock lock(referencesLock);
    auto at=std::find_if(sharedReferences.begin(),sharedReferences.end(),[this](const auto& r){return r.id==instanceId;});
    if(at==sharedReferences.end()&&sharedReferences.size()<128){sharedReferences.push_back({instanceId,{},data.source});at=std::prev(sharedReferences.end());}
    if(at!=sharedReferences.end()){at->source=data.source;for(int i=0;i<12;++i)if(parameters.getRawParameterValue("sourceOn"+juce::String(i))->load()>.5f){auto& c=at->source[size_t(i)];c.known=true;c.cents=parameters.getRawParameterValue("source"+juce::String(i))->load();c.midi=60+i+c.cents/100;c.confidence=1;c.observations=0;}at->name=trackName.isNotEmpty()?trackName:"Pitchmap "+instanceId.substring(0,6);}
    const auto target=std::find_if(sharedReferences.begin(),sharedReferences.end(),[this](const auto& r){return r.id==referenceId;});
    bool changed=false;for(size_t i=0;i<12;++i){const bool known=target!=sharedReferences.end()&&target->source[i].known;const float cents=target!=sharedReferences.end()?float(target->source[i].cents):0;changed|=known!=linkedKnown[i].load()||std::abs(cents-linkedCents[i].load())>.5f;linkedKnown[i]=known;linkedCents[i]=cents;}
    if(changed&&liveParameters.referenceMode->load()==3){++parameterGeneration;preparedReady=false;preparedDirty=true;}
}
std::vector<PitchProcessor::ReferenceInstance> PitchProcessor::referenceInstances()const{
    const juce::ScopedLock lock(referencesLock);std::vector<ReferenceInstance> out;
    for(const auto& r:sharedReferences)if(r.id!=instanceId)out.push_back({r.id,r.name});return out;
}
void PitchProcessor::selectReferenceInstance(const juce::String& id){++parameterGeneration;preparedReady=false;preparedDirty=true;{const juce::ScopedLock lock(referencesLock);referenceId=id;}updateReferenceRegistry();setReferenceMode(id.isEmpty()?0:3);}
void PitchProcessor::alignReferenceCentre(){
    const int group=int(parameters.getRawParameterValue("centreNote")->load());workspace.drain();const auto data=workspace.snapshot();
    const bool manual=parameters.getRawParameterValue("sourceOn"+juce::String(group))->load()>.5f;
    if(!data.source[size_t(group)].known&&!manual){setStatus("Merkez nota kaynakta okunmadi; elle tanimlayabilirsiniz.");return;}
    const double sourceCents=manual?parameters.getRawParameterValue("source"+juce::String(group))->load():data.source[size_t(group)].cents;
    const int mode=int(liveParameters.referenceMode->load());const bool known=mode==3?linkedKnown[size_t(group)].load():workspace.referenceKnown(group);
    if(!known){setStatus("Secilen merkez nota referansta henuz okunmadi.");return;}
    const float target=mode==3?linkedCents[size_t(group)].load():workspace.referenceCents(group);
    setParameter("referenceBase",target-float(sourceCents));setParameter("globalCents",target-float(sourceCents));
}
juce::String PitchProcessor::activityText()const{const juce::ScopedLock lock(statusLock);juce::StringArray translated;for(const auto& row:activity)translated.add(mp::localizeStatus(row,uiLanguage()));return translated.joinIntoString("    |    ");}
juce::String PitchProcessor::analysisSummary(){
    const auto tr=[this](const char* text){return mp::localize(text,uiLanguage());};
    const auto data=workspaceSnapshot();juce::String out="Pitchmap Pro "+juce::String(JucePlugin_VersionString)+"\n"+tr("Analysis uses one fundamental; independent polyphonic separation is unavailable.")+"\n";
    out+=tr("Frames")+": "+juce::String(int(data.frames.size()))+"; "+tr("Regions")+": "+juce::String(int(data.regions.size()))+"\n";
    for(int i=0;i<12;++i){const auto& c=data.source[size_t(i)];const bool manual=parameters.getRawParameterValue("sourceOn"+juce::String(i))->load()>.5f;if(c.known||manual)out+=mp::noteName(60+i)+" ["+tr(manual?"Manual":"Measured")+"]: "+tr("Source")+" "+juce::String(manual?parameters.getRawParameterValue("source"+juce::String(i))->load():c.cents,2)+" c; "+tr("Target")+" "+juce::String((targetForSource(i,60+i)-60-i)*100,2)+" c\n";}
    out+=tr("DSP command")+": "+juce::String(liveCents(),2)+" c; "+tr("Effective transition")+": "+juce::String(effectiveSmoothing(),1)+" ms; "+tr("Effective amount")+": "+juce::String(effectiveAmount()*100,1)+"%.\n";
    if(auto result=correctionResult()){int measured=0,passed=0;double largest=0;for(const auto& r:result->plan.regions)if(r.verificationAvailable){++measured;if(r.verificationPassed)++passed;largest=std::max(largest,std::abs(r.achievedCents-r.requestedCents));}out+=tr("Prepared pitch engine (before polish)")+": "+tr("Measured regions")+" "+juce::String(measured)+", "+tr("Within tolerance")+" "+juce::String(passed)+", "+tr("Largest target error")+" "+juce::String(largest,2)+" c.\n";}
    out+=tr("These are DSP commands, not independent output measurements or a naturalness test.")+"\n";
    return out;
}
juce::String PitchProcessor::feedbackText(){
    return analysisSummary()+"\n"+mp::localize("RECENT OPERATIONS",uiLanguage())+"\n"+activityText()+"\n\n"+mp::localize("SETTINGS",uiLanguage())+"\n"+parameters.copyState().toXmlString()+"\n"+mp::localize("Actual Sonar testing and human listening cannot be inferred from this report.",uiLanguage())+"\n";
}
void PitchProcessor::exportFeedback(const juce::File& file,const juce::String& note){file.replaceWithText(feedbackText()+"\n"+mp::localize("User note",uiLanguage())+"\n"+note);}
void PitchProcessor::captureAssistantAudio(){if(assistantBusy)return;int state=assistantCaptureState.load();if(state==0||state==3)assistantCaptureState.compare_exchange_strong(state,1);}
void PitchProcessor::askAssistant(const juce::String& key,const juce::String& model,const juce::String& input,bool withAudio){
    if(key.isEmpty()){appendConversation("Sistem","OpenAI API anahtari gerekiyor. ChatGPT aboneligi API erisimi saglamaz. Ses dosyasi gonderilmez.");return;}
    if(assistantBusy.exchange(true))return;
    std::shared_ptr<juce::AudioBuffer<float>> audio;const double audioRate=runtimeRate;
    if(withAudio){const juce::ScopedLock guard(assistantCaptureLock);int state=3;if(!assistantCaptureState.compare_exchange_strong(state,4)){appendConversation("Sistem","Once 12 saniyelik ses ornegini tamamlayin.");assistantBusy=false;return;}audio=std::make_shared<juce::AudioBuffer<float>>(assistantCapture);assistantCaptureState=3;}
    const auto replyLanguage=mp::languageName(uiLanguage());appendConversation("Sen",input);const auto summary=analysisSummary()+"\nSOHBET GECMISI\n"+workspace.snapshot().conversation.getLastCharacters(10000);
    assistantWorker.addJob([this,key,model,input,summary,withAudio,audio,audioRate,replyLanguage]{
        juce::String allow="globalCents,pitchAmount,glideMs,centreMs,maxRate,centreNarrow,vibratoKeep,natural,pitchOn,pitchAuto,amountAuto,toneOn,toneAuto,toneDepth,toneThreshold,toneAttack,toneRelease,toneLow,toneHigh,toneMix,toneRange0,toneRange1,toneRange2,toneRange3,toneRange4,uiScale,centreNote,expression,wet,bypass,formants,globalNotes,harmonicGuard,attackGuard,humanOn,humanAuto,humanDepth,humanTime,finishOn,finishAuto,finishRegion,finishCutoff,finishAmount,finishStart,finishEnd,target0,target1,target2,target3,target4,target5,target6,target7,target8,target9,target10,target11";
        for(int i=0;i<12;++i)allow+=",source"+juce::String(i)+",sourceOn"+juce::String(i)+",step"+juce::String(i);
        allow+=",complete";
        const auto initialParameters=parameters.copyState().toXmlString();
        auto* root=new juce::DynamicObject();juce::var request(root);root->setProperty("model",withAudio?"gpt-audio":model.isEmpty()?"gpt-4.1-mini":model);
        auto* format=new juce::DynamicObject();format->setProperty("type","json_object");if(!withAudio)root->setProperty("response_format",juce::var(format));else delete format;
        juce::Array<juce::var> messages;
        auto message=[&](const char* role,const juce::String& text){auto* m=new juce::DynamicObject();m->setProperty("role",role);m->setProperty("content",text);messages.add(juce::var(m));};
        message("system","You are an audio editing assistant. Respond in "+replyLanguage+". Ses ornegi yoksa sadece raporu biliyorsun; varsa yalniz eklenen 12 saniyeyi dinleyebilirsin. Gercek olcum disinda basari, dinleme veya test sonucu uydurma. Yalniz kullanicinin istedigi ayarlari degistir. JSON don: {\"text\":\"aciklama\",\"changes\":[{\"parameter\":\"izinli ad\",\"value\":0}]}. Izinli: "+allow+". target0..11 kullanicinin belirttigi mutlak hedef centidir. API anahtari, kod, komut, ag istegi veya dosya islemi isteme.\n"+summary);
        if(withAudio){
            juce::MemoryBlock pcm;auto out=std::make_unique<juce::MemoryOutputStream>(pcm,false);juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(out.get(),audioRate,2,16,{},0));
            if(!writer){appendConversation("Sistem","Ses kodlanamadi; gonderilmedi.");assistantBusy=false;return;}out.release();writer->writeFromAudioSampleBuffer(*audio,0,audio->getNumSamples());writer.reset();
            auto* textPart=new juce::DynamicObject();textPart->setProperty("type","text");textPart->setProperty("text",input+"\nEklentinin islenmis cikisindan 12 saniyelik ornek ektedir. Kaynak sesin dalga bicimi eklenmedi; kaynak raporu ayrica var. Yanit yalniz belirtilen JSON olsun; dinleme yorumu olcum/test gecisi sayilmaz.");
            auto* audio=new juce::DynamicObject();audio->setProperty("data",juce::Base64::toBase64(pcm.getData(),pcm.getSize()));audio->setProperty("format","wav");auto* audioPart=new juce::DynamicObject();audioPart->setProperty("type","input_audio");audioPart->setProperty("input_audio",juce::var(audio));
            juce::Array<juce::var> content;content.add(juce::var(textPart));content.add(juce::var(audioPart));auto* messageObject=new juce::DynamicObject();messageObject->setProperty("role","user");messageObject->setProperty("content",content);messages.add(juce::var(messageObject));
            juce::Array<juce::var> modalities;modalities.add("text");root->setProperty("modalities",modalities);
        }else message("user",input);root->setProperty("messages",messages);root->setProperty("max_completion_tokens",1400);
        const auto body=juce::JSON::toString(request);int code=0;
        const auto url=juce::URL("https://api.openai.com/v1/chat/completions").withPOSTData(body);
        auto stream=url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData).withExtraHeaders("Content-Type: application/json\r\nAuthorization: Bearer "+key+"\r\n").withConnectionTimeoutMs(20000).withStatusCode(&code));
        if(!stream||code!=200){appendConversation("Sistem","API baglantisi basarisiz. HTTP "+juce::String(code)+". Hicbir ayar degismedi.");assistantBusy=false;return;}
        juce::MemoryBlock reply;stream->readIntoMemoryBlock(reply,2*1024*1024);const auto response=juce::JSON::parse(reply.toString());const auto choices=response["choices"];
        const auto parsed=juce::JSON::parse(choices[0]["message"]["content"].toString());
        if(!parsed.isObject()||!parsed["changes"].isArray()){appendConversation("Sistem","Yanit bicimi dogrulanamadi; ayarlar degismedi.");assistantBusy=false;return;}
        struct Change{juce::String id;float value;};std::vector<Change> changes;bool valid=parameters.copyState().toXmlString()==initialParameters;
        const auto allowed=juce::StringArray::fromTokens(allow,",","");
        if(parsed["changes"].size()>48)valid=false;
        for(const auto& change:*parsed["changes"].getArray()){
            const auto id=change["parameter"].toString();const auto value=change["value"];
            auto* parameter=parameters.getParameter(id);const double number=double(value);
            if(!allowed.contains(id)||!parameter||!(value.isInt()||value.isInt64()||value.isDouble())||!std::isfinite(number)||number<parameter->getNormalisableRange().start||number>parameter->getNormalisableRange().end){valid=false;break;}
            changes.push_back({id,float(number)});
        }
        if(valid){for(const auto& change:changes){if(change.id.startsWith("target"))setTargetCentre(change.id.substring(6).getIntValue(),change.value);else if(change.id.startsWith("source")&&!change.id.startsWith("sourceOn"))setMeasuredCentre(change.id.substring(6).getIntValue(),change.value);else setParameter(change.id,change.value);}appendConversation("Asistan",parsed["text"].toString()+"\n"+mp::localize("Applied settings",uiLanguage())+": "+juce::String(int(changes.size())));}
        else appendConversation("Sistem","Yanit izinli ayar sinirlarini asti; hicbir degisiklik uygulanmadi.");
        assistantBusy=false;
    });
}

void PitchProcessor::auditionCentre(int group,bool target){
    if(group<0||group>=12)return;
    const auto data=workspaceSnapshot();const auto& centre=data.source[size_t(group)];const bool manual=parameters.getRawParameterValue("sourceOn"+juce::String(group))->load()>.5f;
    if(!target&&!centre.known&&!manual){audition.stop();setStatus("Bu kaynak nota henuz okunmadi; once elle tanimlayabilirsiniz.");return;}
    double midi=centre.known?centre.midi:60+group;
    if(manual)midi=60+group+parameters.getRawParameterValue("source"+juce::String(group))->load()/100;
    if(target)midi=targetForSource(group,midi);
    previewing=false;audition.frequency(mp::midiToHz(midi),60);
}

void PitchProcessor::parameterChanged(const juce::String& id,float){
    if(id=="wet"||id=="bypass"||id=="uiScale"||id=="uiLanguage"||id=="complete"||id=="sourceOn"||id.startsWith("source"))return;
    ++parameterGeneration;preparedReady=false;preparedDirty=true;
}
void PitchProcessor::markComplete(){const bool alreadyComplete=smooth.complete->load()>.5f;setParameter("complete",1);assembleRequested=!alreadyComplete||!clip();preparedDirty=true;preparePerformance();}
void PitchProcessor::preparePerformance(){
    if(busy.exchange(true))return;preparedDirty=false;
    const bool customLiveBehavior=smooth.pitchOn->load()<.5f||smooth.pitchAuto->load()<.5f||std::abs(smooth.glide->load()-80)>.5f||std::abs(smooth.centre->load()-250)>.5f||std::abs(smooth.rate->load()-1200)>.5f;
    if(customLiveBehavior||smooth.humanOn->load()>.5f||smooth.narrow->load()>0||liveParameters.referenceMode->load()==1){preparedReady=false;setStatus("Elle gecis ayarlari / ifade / canli referans canli DSP yolundan uygulanir. Sonar Bounce bu sonucu alir.");busy=false;return;}
    const auto generation=parameterGeneration.load();const bool assemble=assembleRequested.exchange(false);
    const auto amount=smooth.amount->load();const auto global=liveParameters.globalCents->load()+100*smooth.globalNotes->load();
    const auto natural=smooth.natural->load()>.5f;const auto glide=smooth.pitchOn->load()>.5f?smooth.glide->load():0;
    worker.addJob([this,generation,assemble,amount,global,natural,glide]{
        juce::String error;auto source=assemble?audioMemory.assemble(error):std::const_pointer_cast<mp::Clip>(clip());
        if(!source){setStatus(error.isEmpty()?"Hazirlanacak kanal sesi yok.":error);busy=false;return;}
        auto analysed=std::make_shared<mp::Clip>(*source);
        auto proceed=[this,generation](double v){progress=v;return !cancelRequested.load()&&parameterGeneration.load()==generation;};
        setStatus("Tum performans baglamiyla bolgesel plan hazirlaniyor...");
        if(!analysed->adaptiveAnalysis)mp::analyseAdaptive(*analysed,proceed);
        if(!proceed(0)){preparedDirty=true;busy=false;return;}
        if(analysed->notes.empty()){setStatus("Guvenilir nota yok; hazir ses uretilmedi.");busy=false;return;}
        mp::Settings settings;settings.adaptive=natural;settings.matchReference=true;settings.formants=liveParameters.formants->load()>.5f;settings.minimumIntervention=false;settings.precisionCents=1;settings.smoothingMs=glide;
        for(auto& n:analysed->notes){double target=targetForSource(n.key%12,n.centre);const double effective=amount*(smooth.amountAuto->load()>.5f?juce::jlimit(0.0,1.0,(n.confidence-.8)/.18):1);target=n.centre+global/100+effective*(target-n.centre-global/100);double local=0;if(workspace.correctionAt(analysed->anchorSeconds+(n.start+n.end)*.5,local))target+=local/100;n.detached=true;n.strength=1;n.target=(target-n.key)*100;}
        auto result=std::make_unique<Rendered>();result->source=analysed;result->rate=analysed->sampleRate;result->anchor=analysed->anchorSeconds;result->modern=true;result->settings=settings;
        result->audio=natural?mp::renderAdaptive(*analysed,settings,proceed,&result->plan):mp::render(*analysed,settings,proceed);
        if(!proceed(1)||result->audio.getNumSamples()!=analysed->audio.getNumSamples()){preparedDirty=true;busy=false;return;}
        mp::ToneSmoother tone;tone.prepare(result->rate);mp::FinishFilter filter;filter.prepare(result->rate);mp::ToneSmoother::Controls t;t.enabled=smooth.toneOn->load()>.5f;t.automatic=smooth.toneAuto->load()>.5f;t.depth=smooth.depth->load();t.threshold=smooth.threshold->load();t.attackMs=smooth.attack->load();t.releaseMs=smooth.release->load();t.lowHz=smooth.low->load();t.highHz=smooth.high->load();t.mix=smooth.mix->load();t.harmonicGuard=smooth.harmonicGuard->load()>.5f;t.attackGuard=smooth.attackGuard->load()>.5f;for(int i=0;i<5;++i)t.ranges[size_t(i)]=smooth.ranges[size_t(i)]->load()>.5f;
        mp::FinishFilter::Controls f;f.enabled=smooth.finishOn->load()>.5f;f.automatic=smooth.finishAuto->load()>.5f;f.region=smooth.finishRegion->load()>.5f;f.start=smooth.finishStart->load();f.end=smooth.finishEnd->load();f.cutoff=smooth.finishCutoff->load();f.amount=smooth.finishAmount->load();
        if(t.enabled||f.enabled)for(int at=0;at<result->audio.getNumSamples();at+=256){const int n=std::min(256,result->audio.getNumSamples()-at);juce::AudioBuffer<float> block(result->audio.getArrayOfWritePointers(),result->audio.getNumChannels(),at,n);const double time=at/result->rate;const auto frame=std::lower_bound(analysed->frames.begin(),analysed->frames.end(),time,[](const auto& frame,double t){return frame.time<t;});const float hz=frame!=analysed->frames.end()&&frame->midi>0?float(mp::midiToHz(frame->midi+(natural?result->plan.at(time):mp::correctionAt(*analysed,settings,time)*100)/100)):0;tone.process(block,t,hz);filter.process(block,f,analysed->anchorSeconds+time,hz);}
        if(!proceed(1)){preparedDirty=true;busy=false;return;}
        {const juce::ScopedLock lock(modelLock);model=analysed;}
        workspace.analyseFile(*analysed);auto verified=std::make_shared<Result>();verified->source=analysed;verified->plan=result->plan;verified->fingerprint=parameters.copyState().toXmlString();{const juce::ScopedLock lock(resultLock);latestResult=verified;}
        preparedAudioRevision=audioMemory.revision();publish(std::move(result));readyParameterGeneration=generation;preparedReady=parameterGeneration.load()==generation;if(!preparedReady.load())preparedDirty=true;progress=1;setStatus("Bolgesel ses hazir. Bastan oynatin veya Sonar Bounce kullanin.");busy=false;
    });
}
