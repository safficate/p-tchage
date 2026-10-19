#pragma once
#include "PitchEngine.h"

namespace mp {
class Audition {
public:
    void tone(int group, int octave);
    int source(std::shared_ptr<const Clip>, int group);
    void stop() { mode.store(0); }
    bool process(juce::AudioBuffer<float>&, double rate, const std::array<float, 12>& cents, float reference);
    bool active() const { return mode.load() != 0; }
private:
    struct Selection { std::shared_ptr<const Clip> clip; double begin = 0, end = 0; };
    std::vector<std::unique_ptr<Selection>> retained;
    std::atomic<Selection*> selection{nullptr}, hazard{nullptr};
    std::atomic<int> mode{0}, group{0}, octave{4};
    std::atomic<uint64_t> request{0};
    uint64_t seen = 0;
    double phase = 0, position = 0;
    int lastGroup = -1, lastNote = -1;
};
}
