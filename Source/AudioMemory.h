#pragma once
#include "PitchEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <bitset>
#include <map>

namespace mp {
// The audio callback only copies into a fixed SPSC queue. Sparse DAW-time
// assembly and allocation happen on the worker; replay overwrites the same
// samples instead of growing another copy of the performance.
class AudioMemory {
public:
    void prepare(double sampleRate);
    void push(const juce::AudioBuffer<float>&,int64_t position)noexcept;
    void drain();
    void clear(){resetRequested=true;}
    std::shared_ptr<Clip> assemble(juce::String& error);
    uint64_t revision()const{return generation.load();}
    uint64_t dropped()const{return droppedBlocks.load();}
private:
    static constexpr int blockSize=128,capacity=16384;
    struct Block{int64_t start=0;int count=0;std::array<std::array<float,blockSize>,2> samples{};};
    struct Stored{std::array<std::array<float,blockSize>,2> samples{};std::bitset<blockSize> valid;};
    juce::AbstractFifo fifo{capacity};std::unique_ptr<Block[]> queue;
    std::map<int64_t,Stored> stored;
    juce::CriticalSection lock;
    std::atomic<uint64_t> generation{0};
    std::atomic<bool> resetRequested{false};std::atomic<uint64_t> droppedBlocks{0};
    double rate=48000;int64_t first=INT64_MAX,last=-1;
};
}
