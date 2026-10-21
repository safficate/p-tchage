#include "Audition.h"

namespace mp {
void Audition::tone(int note, int oct) {
    fixedFrequency=0;
    group = juce::jlimit(0, 11, note); octave = juce::jlimit(-1, 9, oct);
    ++request; mode = 1;
}
void Audition::frequency(double hz) {
    fixedFrequency=juce::jlimit(20.0,16000.0,hz); ++request; mode=1;
}
int Audition::source(std::shared_ptr<const Clip> clip, int note) {
    if (!clip) return -1;
    int found = -1;
    for (int pass = 0; pass < 2 && found < 0; ++pass) for (int i = 0; i < int(clip->notes.size()); ++i) {
        if (pass == 0 && lastGroup == note && i <= lastNote) continue;
        if (clip->notes[size_t(i)].key % 12 == note) { found = i; break; }
    }
    if (found < 0) return -1;
    lastGroup = note; lastNote = found;
    const auto& n = clip->notes[size_t(found)];
    auto next = std::make_unique<Selection>(); next->clip = std::move(clip); next->begin = n.start; next->end = std::min(n.end, n.start + 3);
    auto* pointer = next.get(); retained.push_back(std::move(next)); selection.store(pointer);
    const auto* active = hazard.load(); std::erase_if(retained, [&](const auto& item) { return item.get() != pointer && item.get() != active; });
    ++request; mode = 2; return found;
}
bool Audition::process(juce::AudioBuffer<float>& b, double rate, const std::array<float, 12>& cents, float reference) {
    const int kind = mode.load(); if (kind == 0) return false;
    const auto version = request.load(); if (version != seen) { seen = version; position = phase = 0; }
    Selection* selected;
    do { selected = selection.load(); hazard.store(selected); } while (selected != selection.load());
    if (kind == 2 && !selected) { hazard = nullptr; return false; }
    const int key = group.load();
    const double generated = reference * std::exp2(((octave.load() + 1) * 12 + key - 69 + cents[size_t(key)] / 100.0) / 12);
    const double frequency = fixedFrequency.load()>0 ? fixedFrequency.load() : generated;
    const double duration = kind == 1 ? 3 : selected->end - selected->begin;
    for (int i = 0; i < b.getNumSamples(); ++i) {
        const double seconds = position / rate;
        const float fade = float(juce::jlimit(0.0, 1.0, std::min(seconds / 0.005, (duration - seconds) / 0.02)));
        const float tone = float(0.15 * std::sin(phase)); phase = std::fmod(phase + 2 * juce::MathConstants<double>::pi * frequency / rate, 2 * juce::MathConstants<double>::pi);
        for (int ch = 0; ch < b.getNumChannels(); ++ch) {
            float value = tone;
            if (kind == 2) {
                const double at = (selected->begin + seconds) * selected->clip->sampleRate;
                const int a = juce::jlimit(0, selected->clip->audio.getNumSamples() - 1, int(at));
                const int next = std::min(a + 1, selected->clip->audio.getNumSamples() - 1);
                const auto* source = selected->clip->audio.getReadPointer(std::min(ch, selected->clip->audio.getNumChannels() - 1));
                value = source[a] + float(at - a) * (source[next] - source[a]);
            }
            b.setSample(ch, i, fade * value);
        }
        ++position;
    }
    if (position / rate >= duration && request.load() == version) mode = 0;
    hazard = nullptr; return true;
}
}
