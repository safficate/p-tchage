#pragma once
#include "PitchEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace mp {
struct WorkspaceFrame {
    double time=0;
    float source=0, sourceCentre=0, reference=0, referenceCentre=0;
    float confidence=0, referenceConfidence=0;
};
struct TuningCentre {
    bool known=false;
    double midi=0, cents=0, spread=0, confidence=0;
    int observations=0;
};
struct WorkspaceRegion {
    double start=0,end=0,midi=0,confidence=0;
    int key=0;
};
struct TimeCorrection { double start=0,end=0,cents=0; };
struct WorkspaceSnapshot {
    std::array<TuningCentre,12> source{},reference{};
    std::vector<WorkspaceFrame> frames;
    std::vector<WorkspaceRegion> regions;
    std::vector<TimeCorrection> edits;
    juce::String conversation;
    uint64_t revision=0;
};
// Audio thread only writes a bounded FIFO. Learning/history/state live outside
// the callback and are independent of whether an editor is open.
class LiveWorkspace {
public:
    void push(const WorkspaceFrame&) noexcept;
    void drain();
    void clear(bool referenceOnly=false);
    void analyseFile(const Clip&,bool reference=false);
    WorkspaceSnapshot snapshot() const;
    void edit(TimeCorrection);
    bool correctionAt(double seconds,double& cents) const noexcept;
    void appendConversation(const juce::String& role,const juce::String& text);
    juce::ValueTree save();
    void restore(const juce::ValueTree&);
    float sourceCents(int key) const noexcept {return sourceValues[size_t(key%12)].load();}
    float referenceCents(int key) const noexcept {return referenceValues[size_t(key%12)].load();}
    bool sourceKnown(int key) const noexcept {return sourceKnownValues[size_t(key%12)].load();}
    bool referenceKnown(int key) const noexcept {return referenceKnownValues[size_t(key%12)].load();}
private:
    static constexpr int capacity=8192,maxFrames=36000,maxEdits=128;
    juce::AbstractFifo fifo{capacity};
    // Construct outside playback and keep the large queue off the host stack.
    std::unique_ptr<WorkspaceFrame[]> pending=std::make_unique<WorkspaceFrame[]>(capacity);
    struct Distribution {
        std::array<double,201> bins{};
        double weight=0, midiSum=0,confidenceSum=0;
        int observations=0,noteClass=-1;
        void add(double midi,double confidence);
        TuningCentre centre() const;
    };
    std::array<Distribution,12> sourceStats{},referenceStats{};
    std::array<std::atomic<float>,12> sourceValues{},referenceValues{};
    std::array<std::atomic<bool>,12> sourceKnownValues{},referenceKnownValues{};
    struct AtomicEdit {std::atomic<double> start{0},end{0},cents{0};};
    std::array<AtomicEdit,maxEdits> realtimeEdits{};
    std::atomic<int> editCount{0};
    mutable juce::CriticalSection lock;
    WorkspaceSnapshot data;
    // Reference learning has its own time-indexed observations. Replaying the
    // source without a routed reference must not erase an already learned map.
    std::vector<WorkspaceFrame> referenceFrames;
    void rememberReference(const WorkspaceFrame&);
    void attachReference(WorkspaceFrame&) const;
    void rebuildReferenceStats();
    void updateCentres();
    void rebuildRegions();
    void publishEdits();
};
}
