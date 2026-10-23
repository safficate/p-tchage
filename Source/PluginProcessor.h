#pragma once
#include "PitchEngine.h"
#include "AdaptivePitch.h"
#include "RealtimePitch.h"
#include "Audition.h"
#include "LiveWorkspace.h"
#include "ToneSmoother.h"
#include "FinishFilter.h"
#include "AudioMemory.h"
#include <juce_audio_utils/juce_audio_utils.h>

class PitchProcessor final : public juce::AudioProcessor,
#if JucePlugin_Enable_ARA
                             public juce::AudioProcessorARAExtension,
#endif
                             private juce::Timer, private juce::AudioProcessorValueTreeState::Listener {
public:
    explicit PitchProcessor(bool legacyDefaults=false);
    ~PitchProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override { realtime.reset();toneSmoother.reset();finishFilter.reset(); }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Microtonal Pitch Map Pro"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return getLatencySamples() / runtimeRate; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState parameters;
    mp::Settings settings() const;
    std::shared_ptr<const mp::Clip> clip() const;
    void load(const juce::File&);
    void analyse();
    void render();
    void capture(bool start);
    void preview(bool start);
    void editNotes(std::vector<mp::Note>);
    void undo(bool redo);
    void importScale(const juce::File&);
    void exportAudio(const juce::File&);
    void exportMidi(const juce::File&);
    void preset(const juce::File&, bool save);
    void setParameter(const juce::String&, float);
    void applyCorrection();
    struct Result { mp::CorrectionPlan plan; juce::String fingerprint; std::shared_ptr<const mp::Clip> source; };
    std::shared_ptr<const Result> correctionResult() const;
    void auditionTarget(int group, int octave, int sourceNote);
    void auditionCentre(int group,bool target);
    void releaseAuditionTone(){audition.releaseTone();}
    void cancelWork() { cancelRequested = true; needsRender = false; renderRequested=false; }
    void refreshHostClips();
    void auditionTone(int group, int octave) { previewing = false; audition.tone(group, octave); }
    int auditionSource(int group) { previewing = false; const auto n = audition.source(clip(), group); if (n < 0) { audition.stop(); setStatus("Bu nota kaynak klipte bulunmadi; sahte kaynak tonu calinmadi."); } return n; }
    bool hasRender() const { return current.load() != nullptr; }
    void stopAudition() { audition.stop(); previewing = false; }
    bool araConnected() const {
#if JucePlugin_Enable_ARA
        return isBoundToARA();
#else
        return false;
#endif
    }
    float referencePitch() const { return sidechainPitch.load(); }
    struct TracePoint { double time = 0; float source = 0, reference = 0; };
    std::vector<TracePoint> takeTrace();
    juce::String status() const;
    float liveMidi() const { return cachePlaying.load()?cachedMidi.load():realtime.detectedMidi(); }
    float liveCents() const { return cachePlaying.load()?cachedCents.load():realtime.appliedCents(); }
    float liveInputLevel() const { return realtime.inputLevel(); }
    void enterLiveWorkspace();
    void equalizeMeasured();
    void setMeasuredCentre(int group,double cents);
    void markComplete();
    bool preparedPlayback()const{return cachePlaying.load();}
    struct ReferenceInstance {juce::String id,name;};
    std::vector<ReferenceInstance> referenceInstances()const;
    void selectReferenceInstance(const juce::String& id);
    void alignReferenceCentre();
    juce::String activityText()const;
    juce::String feedbackText();
    void askAssistant(const juce::String& key,const juce::String& model,const juce::String& text,bool withAudio=false);
    void captureAssistantAudio();
    bool assistantAudioReady()const{return assistantCaptureState.load()==3;}
    float assistantCaptureProgress()const{return float(assistantCaptured.load())/float(std::max(1,assistantCapture.getNumSamples()));}
    std::atomic<bool> assistantBusy{false};
    float finishAmount()const{return finishFilter.effective();}
    float toneReduction(int band)const{return toneSmoother.reduction(band);}
    float toneFrequency(int band)const{return toneSmoother.frequency(band);}
    float effectiveAmount()const{return realtime.effectiveAmount();}
    float effectiveSmoothing()const{return realtime.smoothingTimeMs();}
    bool liveWorkspaceEnabled() const {return liveParameters.workspace->load()>.5f;}
    mp::WorkspaceSnapshot workspaceSnapshot() {workspace.drain();return workspace.snapshot();}
    void setTargetCentre(int sourceClass,double cents);
    void setReferenceMode(int mode);
    void editLiveRegion(double start,double end,double additionalCents);
    void resetLiveLearning(bool referenceOnly=false);
    double targetForSource(int sourceClass,double sourceMidi) const;
    void loadReference(const juce::File&);
    void exportFeedback(const juce::File&,const juce::String& userNote);
    void appendConversation(const juce::String& role,const juce::String& text){workspace.appendConversation(role,text);}
    juce::String analysisSummary();
    double transportSeconds()const{return hostSeconds.load();}
    bool transportIsPlaying()const{return hostPlaying.load();}
    std::atomic<bool> busy{false}, capturing{false}, previewing{false};
    std::atomic<double> progress{0};
    std::atomic<int> revision{0};

private:
    mp::LiveWorkspace workspace;
    mp::AudioMemory audioMemory;
    std::atomic<bool> sourceChanged{false},cacheResetRequested{false};
    std::atomic<bool> memoryJob{false},preparedReady{false},preparedDirty{false},assembleRequested{false},cachePlaying{false};
    std::atomic<uint64_t> parameterGeneration{0},readyParameterGeneration{0};
    std::atomic<float> cachedMidi{0},cachedCents{0};
    uint64_t preparedAudioRevision=0;double cacheBlend=0;bool previousPlaying=false,cacheLatch=false;int64_t previousEnd=-1;
    void parameterChanged(const juce::String&,float)override;
    void preparePerformance();
    mp::ToneSmoother toneSmoother;
    mp::FinishFilter finishFilter;
    juce::AudioBuffer<float> assistantCapture;
    juce::CriticalSection assistantCaptureLock;
    std::atomic<int> assistantCaptureState{0},assistantCaptured{0};
    double lastActivityWall=0;
    bool postWasBypassed=true;
    juce::ThreadPool assistantWorker{1};
    juce::String instanceId,referenceId,trackName;
    std::array<std::atomic<float>,12> linkedCents{};
    std::array<std::atomic<bool>,12> linkedKnown{};
    juce::StringArray activity;
    uint64_t activitySerial=0;
    void updateReferenceRegistry();
    struct SmoothParameters {
        std::atomic<float>* natural=nullptr,*pitchOn=nullptr,*pitchAuto=nullptr,*glide=nullptr,*centre=nullptr,*rate=nullptr,*amount=nullptr,*amountAuto=nullptr,*narrow=nullptr,*vibrato=nullptr;
        std::atomic<float>* toneOn=nullptr,*toneAuto=nullptr,*depth=nullptr,*threshold=nullptr,*attack=nullptr,*release=nullptr,*low=nullptr,*high=nullptr,*mix=nullptr,*harmonicGuard=nullptr,*attackGuard=nullptr;
        std::atomic<float>* humanOn=nullptr,*humanAuto=nullptr,*humanDepth=nullptr,*humanTime=nullptr;
        std::atomic<float>* finishOn=nullptr,*finishAuto=nullptr,*finishRegion=nullptr,*finishCutoff=nullptr,*finishAmount=nullptr,*finishStart=nullptr,*finishEnd=nullptr,*globalNotes=nullptr,*referenceBase=nullptr,*complete=nullptr;
        std::array<std::atomic<float>*,5> ranges{};
    } smooth;
    void updateTrackProperties(const TrackProperties& properties)override;
    uint64_t lastSourceSerial=0,lastReferenceSerial=0;
    std::atomic<bool> hostPlaying{false};
    mp::RealtimePitch realtime;
    mp::RealtimePitch referenceAnalyzer;
    mp::Audition audition;
    std::atomic<float> sidechainPitch{0};
    juce::AbstractFifo traceFifo{1024};
    std::array<TracePoint, 1024> trace{};
    uint64_t processedSamples = 0;
    std::atomic<bool> renderedNotify{false};
    std::atomic<bool> cancelRequested{false};
    struct LiveParameters {
        std::array<std::atomic<float>*, 12> cents{}, groups{}, steps{}, targets{}, targetOn{}, relative{};
        std::atomic<float>* workspace=nullptr,*referenceMode=nullptr,*expression=nullptr;
        std::array<std::atomic<float>*, 128> octaves{};
        std::atomic<float>* strength = nullptr, *tightness = nullptr, *vibrato = nullptr, *smoothing = nullptr;
        std::atomic<float>* humanize = nullptr, *seed = nullptr, *reference = nullptr, *wet = nullptr;
        std::atomic<float>* octave = nullptr, *formants = nullptr, *bypass = nullptr, *original = nullptr;
        std::atomic<float>* useRender = nullptr, *offset = nullptr;
        std::atomic<float>* direct = nullptr;
        std::atomic<float>* globalCents = nullptr;
        std::atomic<float>* enabled = nullptr, *matchReference = nullptr, *follow = nullptr, *preserveOctave = nullptr;
    } liveParameters;
    void processAudio(juce::AudioBuffer<float>&, juce::MidiBuffer&, bool forceBypass);
    struct Rendered { juce::AudioBuffer<float> audio; std::shared_ptr<const mp::Clip> source; double rate = 48000, anchor = 0; bool modern=false;mp::Settings settings;mp::CorrectionPlan plan; };
    std::atomic<Rendered*> current{nullptr}, hazard{nullptr};
    std::vector<std::unique_ptr<Rendered>> retained;
    std::atomic<bool> inCallback{false};
    juce::AudioBuffer<float> captureBuffer;
    std::atomic<int> captured{0};
    std::atomic<bool> captureFull{false};
    std::atomic<double> captureAnchor{0};
    double runtimeRate = 48000;
    std::atomic<int64_t> previewPosition{0};
    mutable juce::CriticalSection modelLock, statusLock, resultLock;
    std::shared_ptr<const Result> latestResult;
    std::atomic<bool> renderRequested{false};
    std::atomic<double> hostSeconds{0};
    std::atomic<bool> hostPositionKnown{false};
    std::shared_ptr<mp::Clip> model;
    juce::String message = "Kanali dinletin. ESITLE ile olcumleri hedeflere alin; degisiklikler anlik duyulur.";
    std::vector<std::vector<mp::Note>> history, future;
    juce::ThreadPool worker{1};
    mp::Settings lastRenderedSettings;
    juce::String settingsFingerprint;
    std::atomic<bool> needsRender{false};
    bool restoredClipPending = false,legacyProject=false;
    void setStatus(juce::String);
    void install(std::shared_ptr<mp::Clip>);
    void publish(std::unique_ptr<Rendered>);
    void timerCallback() override;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchProcessor)
};
