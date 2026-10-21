#include "RealtimePitch.h"
#include "signalsmith-stretch.h"
#include <algorithm>
#include <cmath>

namespace mp {
struct RealtimePitch::Impl {
    static constexpr int window = 4096, fftSize = window * 2, hop = 1024, chunkSize = 128;
    signalsmith::stretch::SignalsmithStretch<float> stretch{1};
    signalsmith::linear::Pow2FFT<float> fft{fftSize};
    std::array<std::complex<float>, fftSize> time{}, spectrum{};
    std::array<double, window + 1> energy{};
    std::array<double, window / 2 + 2> cmnd{};
    std::array<std::array<float, window>, 2> history{};
    std::array<std::array<float, window>, 2> renderedHistory{};
    struct Expected { int64_t end = -1; double midi = 0; int key = -1; };
    std::array<Expected, 64> expected{};
    std::array<std::array<float, chunkSize>, 2> input{}, output{}, dry{};
    juce::AudioBuffer<float> delay;
    double rate = 48000, centre = 0, slowPitch = 0, pitch = 0, shift = 0, trim = 0;
    int channels = 2, latency = 0, delayPosition = 0, writePosition = 0, filled = 0, hopPosition = 0, key = -1;
    int unvoiced = 0;
    uint32_t noteCounter = 0;
    int outputWrite = 0, expectedWrite = 0;
    int64_t received = 0;
    bool suspended = false;

    Impl(double sr, int ch) : rate(sr), channels(ch) {
        stretch.presetDefault(channels, float(rate), true);
        latency = stretch.inputLatency() + stretch.outputLatency();
        delay.setSize(channels, latency); delay.clear();
    }
    void resetStreaming() {
        stretch.reset(); suspended = false;
        for (auto& h : history) h.fill(0);
        for (auto& h : renderedHistory) h.fill(0);
        expected.fill({}); outputWrite = expectedWrite = 0; received = 0; trim = 0;
        writePosition = filled = hopPosition = unvoiced = 0;
        key = -1; centre = slowPitch = pitch = shift = 0; noteCounter = 0;
    }
    void reset() { resetStreaming(); delay.clear(); delayPosition = 0; }
    double detect(const std::array<std::array<float, window>, 2>& samples, int cursor) {
        double e[2]{};
        for (int ch = 0; ch < channels; ++ch)
            for (float v : samples[size_t(ch)]) e[ch] += double(v) * v;
        const int channel = channels == 2 && e[1] > e[0] ? 1 : 0;
        if (e[channel] / window < 1e-6) return 0;
        double mean = 0;
        for (float v : samples[size_t(channel)]) mean += v;
        mean /= window;
        time.fill({}); energy[0] = 0;
        for (int i = 0; i < window; ++i) {
            const float v = float(samples[size_t(channel)][size_t((cursor + i) % window)] - mean);
            time[size_t(i)] = {v, 0}; energy[size_t(i + 1)] = energy[size_t(i)] + double(v) * v;
        }
        fft.fft(time.data(), spectrum.data());
        for (auto& v : spectrum) v = {std::norm(v), 0};
        fft.ifft(spectrum.data(), time.data());
        const int lo = std::max(2, int(rate / 1600)), hi = std::min(window / 2, int(rate / 30));
        double sum = 0; cmnd.fill(1);
        for (int t = 1; t <= hi + 1; ++t) {
            const double difference = std::max(0.0, energy[size_t(window - t)] + energy.back() - energy[size_t(t)] - 2.0 * time[size_t(t)].real() / fftSize);
            sum += difference; cmnd[size_t(t)] = sum > 0 ? difference * t / sum : 1;
        }
        int best = lo;
        for (int t = lo; t <= hi; ++t) {
            if (cmnd[size_t(t)] < cmnd[size_t(best)]) best = t;
            if (cmnd[size_t(t)] < 0.12) {
                while (t < hi && cmnd[size_t(t + 1)] < cmnd[size_t(t)]) ++t;
                best = t; break;
            }
        }
        if (cmnd[size_t(best)] > 0.15) return 0;
        const double a = cmnd[size_t(best - 1)], b = cmnd[size_t(best)], c = cmnd[size_t(best + 1)], denominator = a - 2 * b + c;
        const double delta = std::abs(denominator) > 1e-12 ? juce::jlimit(-0.5, 0.5, 0.5 * (a - c) / denominator) : 0;
        return 69 + 12 * std::log2(rate / (best + delta) / 440);
    }
    void analyse() {
        pitch = detect(history, writePosition);
        if (pitch == 0) { losePitch(); return; }
        // Hysteresis avoids switching pitch classes at a vibrato extremum.
        if (key < 0 || std::abs(pitch - key) > 0.7) {
            key = juce::jlimit(0, 127, int(std::lround(pitch)));
            centre = slowPitch = pitch; trim = 0; ++noteCounter;
        } else {
            centre += (1 - std::exp(-hop / (rate * 0.25))) * (pitch - centre);
            slowPitch += (1 - std::exp(-hop / (rate * 0.06))) * (pitch - slowPitch);
        }
        unvoiced = 0;
    }
    void calibrate(double requestedShift) {
        expected[size_t(expectedWrite)] = {received, pitch > 0 ? pitch + requestedShift : 0, key};
        expectedWrite = (expectedWrite + 1) % int(expected.size());
        if (std::abs(requestedShift) < 1e-6) { trim = 0; return; }
        const int64_t sourceEnd = received - latency;
        const Expected* nearest = nullptr;
        for (const auto& point : expected) if (point.end >= 0 && point.end <= sourceEnd && (!nearest || point.end > nearest->end)) nearest = &point;
        if (!nearest || nearest->midi == 0 || nearest->key != key) return;
        int stable = 0;
        for (const auto& point : expected) if (point.end >= nearest->end - 3 * hop && point.end <= nearest->end) {
            if (point.key != key || point.midi == 0 || std::abs(point.midi - nearest->midi) > 0.015) return;
            ++stable;
        }
        if (stable < 4) return;
        const double outputMidi = detect(renderedHistory, outputWrite);
        if (outputMidi == 0) return;
        const double error = nearest->midi - outputMidi;
        // Bounded centre refinement only on stable voiced passages. Transitions,
        // vibrato and ambiguous/noisy audio do not drive the feedback controller.
        if (std::abs(error) < 0.25) trim = juce::jlimit(-0.15, 0.15, trim + error * 0.08);
    }
    void losePitch() {
        // Do not transpose breath/noise/silence using an old sustained note.
        pitch = 0;
        if (++unvoiced >= 2) key = -1;
    }
    double correction(const Controls& c) const {
        if (pitch == 0 || key < 0) return 0;
        const int group = key % 12;
        if (c.followReference > 0) {
            if (c.referenceMidi <= 0) return 0;
            double target = c.referenceMidi;
            if (c.followReference == 1) target = key + (target - std::round(target));
            else if (c.preserveOctave) target += 12 * std::round((pitch - target) / 12);
            return juce::jlimit(-24.0, 24.0, target - (c.followReference == 1 ? centre : pitch));
        }
        if (c.matchReference) {
            const int targetKey = key + c.semitones[size_t(group)];
            return juce::jlimit(-24.0, 24.0, targetKey + c.cents[size_t((targetKey % 12 + 12) % 12)] / 100 + 12 * std::log2(c.reference / 440.0) - centre);
        }
        if (c.directShift) return c.semitones[size_t(group)] + (c.octaveMode ? c.octaveCents[size_t(key)] : c.cents[size_t(group)]) / 100.0 + c.globalCents / 100.0;
        const double target = key + (c.octaveMode ? c.octaveCents[size_t(key)] : c.cents[size_t(group)]) / 100.0 + 12 * std::log2(c.reference / 440.0);
        const uint32_t hash = uint32_t(key) * 1664525u + uint32_t(c.seed) * 1013904223u + noteCounter * 2246822519u;
        const double human = c.humanize * (double(hash % 20001) / 10000 - 1) / 100;
        return juce::jlimit(-12.0, 12.0,
            c.strength * c.groupStrength[size_t(group)] * (target - centre) + human
            - c.tightness * ((slowPitch - centre) + (1 - c.vibrato) * (pitch - slowPitch)));
    }
};
RealtimePitch::RealtimePitch() = default;
RealtimePitch::~RealtimePitch() = default;
void RealtimePitch::prepare(double sr, int channels) {
    impl = std::make_unique<Impl>(sr, juce::jlimit(1, 2, channels));
    midi = cents = level = 0;
}
void RealtimePitch::reset() { if (impl) impl->reset(); midi = cents = level = 0; }
int RealtimePitch::latency() const { return impl ? impl->latency : 0; }
void RealtimePitch::processDelay(juce::AudioBuffer<float>& b) {
    if (!impl || b.getNumChannels() == 0) return;
    auto& p = *impl; double energy = 0;
    for (int i = 0; i < b.getNumSamples(); ++i) {
        for (int ch = 0; ch < std::min(p.channels, b.getNumChannels()); ++ch) {
            float input = b.getSample(ch, i); if (!std::isfinite(input)) input = 0;
            const float output = p.delay.getSample(ch, p.delayPosition); p.delay.setSample(ch, p.delayPosition, input); b.setSample(ch, i, output);
            if (ch == 0) energy += double(input) * input;
        }
        p.delayPosition = (p.delayPosition + 1) % p.latency;
    }
    p.suspended = true; midi = cents = 0;
    level = b.getNumSamples() > 0 ? float(std::sqrt(energy / b.getNumSamples())) : 0;
}
void RealtimePitch::process(juce::AudioBuffer<float>& b, const Controls& controls) {
    if (!impl || b.getNumChannels() == 0) return;
    auto& p = *impl;
    if (p.suspended) p.resetStreaming();
    const int channels = std::min(p.channels, b.getNumChannels());
    double blockEnergy = 0;
    for (int offset = 0; offset < b.getNumSamples();) {
        const int count = std::min({Impl::chunkSize, b.getNumSamples() - offset, Impl::hop - p.hopPosition});
        for (int i = 0; i < count; ++i) {
            for (int ch = 0; ch < p.channels; ++ch) {
                float v = b.getSample(std::min(ch, channels - 1), offset + i);
                if (!std::isfinite(v)) v = 0;
                p.input[size_t(ch)][size_t(i)] = v;
                p.history[size_t(ch)][size_t(p.writePosition)] = v;
                p.dry[size_t(ch)][size_t(i)] = p.delay.getSample(ch, p.delayPosition);
                p.delay.setSample(ch, p.delayPosition, v);
                if (ch == 0) blockEnergy += double(v) * v;
            }
            p.writePosition = (p.writePosition + 1) % Impl::window;
            p.delayPosition = (p.delayPosition + 1) % p.latency;
        }
        p.filled = std::min(Impl::window, p.filled + count); p.hopPosition += count;
        p.received += count;
        if (p.hopPosition == Impl::hop) {
            p.hopPosition = 0;
            if (p.filled == Impl::window) p.analyse();
        }
        const double target = p.correction(controls);
        const double smoothing = std::max(5.0, double(controls.smoothingMs)) / 1000;
        p.shift += (1 - std::exp(-count / (p.rate * smoothing))) * (target + p.trim - p.shift);
        p.stretch.setTransposeSemitones(float(p.shift));
        p.stretch.setFormantFactor(1, controls.formants);
        p.stretch.setFormantBase(p.pitch > 0 ? float(440 * std::exp2((p.pitch - 69) / 12) / p.rate) : 0);
        const float* input[2] = {p.input[0].data(), p.input[1].data()};
        float* output[2] = {p.output[0].data(), p.output[1].data()};
        if (!controls.analysisOnly) p.stretch.process(input, count, output, count);
        if (!controls.analysisOnly && channels == 2) {
            bool identical = true, inverted = true;
            for (int i = 0; i < count; ++i) {
                identical &= p.input[0][size_t(i)] == p.input[1][size_t(i)] && p.dry[0][size_t(i)] == p.dry[1][size_t(i)];
                inverted &= p.input[0][size_t(i)] == -p.input[1][size_t(i)] && p.dry[0][size_t(i)] == -p.dry[1][size_t(i)];
            }
            // A mono source duplicated by a stereo host must stay mono. The
            // spectral processor's independent channel phases must not create
            // artificial width (or cancel an inverted duplicate).
            if (identical || inverted) for (int i = 0; i < count; ++i)
                p.output[1][size_t(i)] = identical ? p.output[0][size_t(i)] : -p.output[0][size_t(i)];
        }
        const bool unchanged = controls.dry || (controls.directShift || controls.matchReference || controls.followReference > 0 ? std::abs(p.shift) < 1e-7 : controls.strength == 0 && controls.tightness == 0 && controls.humanize == 0);
        if (controls.analysisOnly) { offset += count; continue; }
        for (int i = 0; i < count; ++i) {
            for (int ch = 0; ch < p.channels; ++ch) p.renderedHistory[size_t(ch)][size_t(p.outputWrite)] = p.output[size_t(ch)][size_t(i)];
            p.outputWrite = (p.outputWrite + 1) % Impl::window;
        }
        if (p.hopPosition == 0 && p.filled == Impl::window) p.calibrate(target);
        for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < count; ++i) {
            const float dry = p.dry[size_t(ch)][size_t(i)];
            float wet = p.output[size_t(ch)][size_t(i)];
            wet = std::isfinite(wet) ? juce::jlimit(-0.999f, 0.999f, wet) : 0;
            b.setSample(ch, offset + i, unchanged ? dry : dry + controls.wet * (wet - dry));
        }
        offset += count;
    }
    midi.store(float(p.pitch), std::memory_order_relaxed);
    cents.store(controls.dry ? 0 : float(p.shift * 100), std::memory_order_relaxed);
    level.store(b.getNumSamples() > 0 ? float(std::sqrt(blockEnergy / b.getNumSamples())) : 0, std::memory_order_relaxed);
}
}
