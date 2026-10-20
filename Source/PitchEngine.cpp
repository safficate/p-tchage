#include "PitchEngine.h"
#include "AdaptivePitch.h"
#include "signalsmith-stretch.h"
#include <numeric>
#include <stdexcept>

namespace mp {
double hzToMidi(double hz) { return 69 + 12 * std::log2(hz / 440.0); }
double midiToHz(double midi) { return 440 * std::exp2((midi - 69) / 12); }
juce::String noteName(int key) {
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return juce::String(names[(key % 12 + 12) % 12]) + juce::String(key / 12 - 1);
}
std::pair<double, double> detectPitch(const float* data, int size, double sr) {
    int order = 1; while ((1 << order) < size * 2) ++order;
    juce::dsp::FFT fft(order);
    const auto n = 1 << order;
    std::vector<juce::dsp::Complex<float>> x(static_cast<size_t>(n)), f(static_cast<size_t>(n)), r(static_cast<size_t>(n));
    double mean = std::accumulate(data, data + size, 0.0) / size;
    std::vector<double> energy(size_t(size + 1));
    for (int i = 0; i < size; ++i) {
        const float v = float(data[i] - mean);
        x[size_t(i)] = {v, 0}; energy[size_t(i + 1)] = energy[size_t(i)] + v * v;
    }
    if (energy.back() / size < 0.000001) return {0, 0};
    fft.perform(x.data(), f.data(), false);
    for (auto& v : f) v = {std::norm(v), 0};
    fft.perform(f.data(), r.data(), true);
    const int lo = std::max(2, int(sr / 1600)), hi = std::min(size / 2, int(sr / 30));
    std::vector<double> cmnd(size_t(hi + 2), 1);
    double sum = 0;
    for (int t = 1; t <= hi + 1; ++t) {
        const double d = std::max(0.0, energy[size_t(size - t)] + energy.back() - energy[size_t(t)] - 2 * r[size_t(t)].real());
        sum += d; cmnd[size_t(t)] = sum > 0 ? d * t / sum : 1;
    }
    int best = lo;
    for (int t = lo; t <= hi; ++t) {
        if (cmnd[size_t(t)] < cmnd[size_t(best)]) best = t;
        if (cmnd[size_t(t)] < 0.12) {
            while (t < hi && cmnd[size_t(t + 1)] < cmnd[size_t(t)]) ++t;
            best = t; break;
        }
    }
    const double confidence = 1 - cmnd[size_t(best)];
    if (confidence < 0.8) return {0, confidence};
    const double a = cmnd[size_t(best - 1)], b = cmnd[size_t(best)], c = cmnd[size_t(best + 1)];
    const double den = a - 2 * b + c;
    const double delta = std::abs(den) > 1e-12 ? juce::jlimit(-0.5, 0.5, 0.5 * (a - c) / den) : 0;
    return {sr / (best + delta), confidence};
}
static double median(std::vector<double> values) {
    if (values.empty()) return 0;
    auto m = values.begin() + std::ptrdiff_t(values.size() / 2);
    std::nth_element(values.begin(), m, values.end()); return *m;
}
void analyse(Clip& clip, const Progress& progress) {
    clip.frames.clear(); clip.notes.clear();
    constexpr int window = 4096, hop = 512;
    std::vector<float> mono(window);
    const auto samples = clip.audio.getNumSamples();
    for (int pos = 0; pos + window <= samples; pos += hop) {
        if (progress && !progress(double(pos) / std::max(1, samples))) return;
        double energy = 0;
        const int ch = clip.audio.getNumChannels() == 1 || clip.audio.getRMSLevel(0, pos, window) >= clip.audio.getRMSLevel(1, pos, window) ? 0 : 1;
        for (int i = 0; i < window; ++i) {
            // Select the stronger channel for pitch; anti-phase stereo must not cancel.
            mono[size_t(i)] = clip.audio.getSample(ch, pos + i); energy += mono[size_t(i)] * mono[size_t(i)];
        }
        auto [hz, confidence] = detectPitch(mono.data(), window, clip.sampleRate);
        clip.frames.push_back({(pos + window / 2.0) / clip.sampleRate, hz > 0 ? hzToMidi(hz) : 0, confidence, std::sqrt(energy / window)});
    }
    int begin = -1; double last = 0;
    auto finish = [&](int end) {
        if (begin < 0 || end - begin < 4) { begin = -1; return; }
        std::vector<double> pitches, confidences;
        for (int i = begin + 1; i < end - 1; ++i) if (clip.frames[size_t(i)].confidence >= 0.85) {
            pitches.push_back(clip.frames[size_t(i)].midi); confidences.push_back(clip.frames[size_t(i)].confidence);
        }
        if (pitches.empty()) { begin = -1; return; }
        const auto centre = median(pitches);
        const auto start = std::max(0.0, clip.frames[size_t(begin)].time - window / (2 * clip.sampleRate));
        const auto stop = std::min(samples / clip.sampleRate, clip.frames[size_t(end - 1)].time + hop / clip.sampleRate);
        clip.notes.push_back({start, stop, centre, median(confidences), int(std::lround(centre)), false, 0, 1});
        begin = -1;
    };
    for (int i = 0; i < int(clip.frames.size()); ++i) {
        const auto& f = clip.frames[size_t(i)];
        if (f.midi == 0 || f.confidence < 0.8) { finish(i); continue; }
        if (begin >= 0 && std::abs(f.midi - last) > 0.65) finish(i);
        if (begin < 0) begin = i;
        last = f.midi;
    }
    finish(int(clip.frames.size()));
    // Notes may not overlap: edits and automation use a single monophonic timeline.
    for (size_t i = 1; i < clip.notes.size(); ++i)
        clip.notes[i - 1].end = std::min(clip.notes[i - 1].end, clip.notes[i].start);
    if (progress) progress(1);
}
static double framePitch(const Clip& c, double time, double fallback) {
    auto it = std::lower_bound(c.frames.begin(), c.frames.end(), time, [](const Frame& f, double t) { return f.time < t; });
    if (it == c.frames.end() || it->midi == 0 || it->confidence < 0.8) return fallback;
    return it->midi;
}
double correctionAt(const Clip& c, const Settings& s, double time) {
    auto it = std::upper_bound(c.notes.begin(), c.notes.end(), time, [](double t, const Note& n) { return t < n.start; });
    if (it == c.notes.begin()) return 0;
    const auto& n = *--it;
    if (time > n.end) return 0;
    if (s.matchReference) {
        const int targetKey = n.key + s.semitones[size_t(n.key % 12)];
        double target = targetKey + s.cents[size_t((targetKey % 12 + 12) % 12)] / 100 + 12 * std::log2(s.reference / 440.0);
        if (n.detached) target = n.key + n.target / 100;
        double shift = target - n.centre;
        if (s.smoothingMs > 0) shift *= juce::jlimit(0.0, 1.0, std::min(time - n.start, n.end - time) / (s.smoothingMs / 1000));
        return juce::jlimit(-24.0, 24.0, shift);
    }
    if (s.directShift) {
        // A direct amount means exactly that amount added to the performed note,
        // preserving its original tuning/vibrato. No percentage or centre snap.
        const auto group = size_t((n.key % 12 + 12) % 12);
        double shift = s.semitones[group] + s.cents[group] / 100;
        if (s.octaveMode) if (auto o = s.octaveCents.find(n.key); o != s.octaveCents.end()) shift = s.semitones[group] + o->second / 100;
        if (n.detached) shift = n.target / 100;
        if (s.smoothingMs > 0) shift *= juce::jlimit(0.0, 1.0, std::min(time - n.start, n.end - time) / (s.smoothingMs / 1000));
        return juce::jlimit(-24.0, 24.0, shift);
    }
    double cents = s.cents[size_t((n.key % 12 + 12) % 12)];
    if (s.octaveMode) { const auto o = s.octaveCents.find(n.key); if (o != s.octaveCents.end()) cents = o->second; }
    const double strength = n.detached ? n.strength : s.strength * s.groupStrength[size_t((n.key % 12 + 12) % 12)];
    if (n.detached) cents = n.target;
    const double target = n.key + cents / 100 + 12 * std::log2(s.reference / 440.0);
    const uint32_t hash = uint32_t(n.key * 1664525u + uint32_t(s.seed) * 1013904223u + uint32_t(n.start * 48000));
    const double human = s.humanize * (double(hash % 20001) / 10000 - 1) / 100;
    double correction = strength * (target - n.centre) + human;
    if (s.tightness > 0) {
        // Remove slow deviations, preserving motion in the vibrato band (roughly 4-9Hz).
        double mean = 0; int count = 0;
        auto frame = std::lower_bound(c.frames.begin(), c.frames.end(), time - 0.06, [](const Frame& f, double t) { return f.time < t; });
        for (; frame != c.frames.end() && frame->time <= time + 0.06; ++frame) if (frame->midi != 0) { mean += frame->midi; ++count; }
        mean = count ? mean / count : n.centre;
        const double pitch = framePitch(c, time, n.centre);
        correction -= s.tightness * ((mean - n.centre) + (1 - s.vibrato) * (pitch - mean));
    }
    const double fade = s.smoothingMs / 1000;
    if (fade > 0) correction *= juce::jlimit(0.0, 1.0, std::min(time - n.start, n.end - time) / fade);
    return juce::jlimit(-12.0, 12.0, correction);
}
static juce::AudioBuffer<float> renderPass(const Clip& c, const Settings& s, const std::vector<double>& trims, const Progress& progress) {
    const auto channels = c.audio.getNumChannels(), length = c.audio.getNumSamples();
    juce::AudioBuffer<float> output(channels, length); output.clear();
    if (channels < 1 || length < 1) return output;
    bool unchanged = true;
    for (const auto& n : c.notes) if (std::abs(correctionAt(c, s, (n.start + n.end) * 0.5)) > 1e-9 || (!s.directShift && !s.matchReference && s.tightness > 0)) { unchanged = false; break; }
    if (unchanged) { output.makeCopyOf(c.audio); if (progress) progress(1); return output; }
    // A mono-equivalent stereo recording must remain exactly phase linked, including
    // the spectral engine's startup/tail. Process it once rather than creating two
    // independent spectral noise floors. General stereo still uses linked STFT.
    int processingChannels = channels; double stereoFactor = 1;
    if (channels == 2) {
        double energy = 0, cross = 0;
        for (int i = 0; i < length; ++i) { const double left = c.audio.getSample(0, i); energy += left * left; cross += left * c.audio.getSample(1, i); }
        if (energy > 1e-10) {
            stereoFactor = cross / energy; double residual = 0;
            for (int i = 0; i < length; ++i) residual = std::max(residual, std::abs(c.audio.getSample(1, i) - stereoFactor * c.audio.getSample(0, i)));
            if (residual < 1e-7 && std::abs(stereoFactor) <= 16) processingChannels = 1;
        }
    }
    signalsmith::stretch::SignalsmithStretch<float> stretch(1);
    stretch.presetDefault(processingChannels, float(c.sampleRate));
    const int delay = stretch.inputLatency() + stretch.outputLatency();
    constexpr int block = 128;
    juce::AudioBuffer<float> in(processingChannels, block), out(processingChannels, block);
    std::vector<const float*> ip(size_t(processingChannels), nullptr);
    std::vector<float*> op(size_t(processingChannels), nullptr);
    for (int ch = 0; ch < processingChannels; ++ch) { ip[size_t(ch)] = in.getReadPointer(ch); op[size_t(ch)] = out.getWritePointer(ch); }
    for (int pos = 0; pos < length + delay; pos += block) {
        if (progress && !progress(double(pos) / (length + delay))) return {};
        in.clear();
        for (int ch = 0; ch < processingChannels; ++ch) if (pos < length) in.copyFrom(ch, 0, c.audio, ch, pos, std::min(block, length - pos));
        const auto time = (pos - stretch.inputLatency() + block * 0.5) / c.sampleRate;
        auto semitones = correctionAt(c, s, time);
        auto note = std::upper_bound(c.notes.begin(), c.notes.end(), time, [](double t, const Note& n) { return t < n.start; });
        if (note != c.notes.begin()) {
            --note;
            if (time <= note->end) {
                const double fade = s.smoothingMs / 1000;
                const double envelope = fade > 0 ? juce::jlimit(0.0, 1.0, std::min(time - note->start, note->end - time) / fade) : 1;
                semitones += trims[size_t(note - c.notes.begin())] * envelope;
            }
        }
        stretch.setTransposeSemitones(float(semitones));
        stretch.setFormantFactor(1.0f, s.formants);
        stretch.setFormantBase(float(midiToHz(framePitch(c, time, 57)) / c.sampleRate));
        stretch.process(ip.data(), block, op.data(), block);
        for (int j = 0; j < block; ++j) {
            const int dest = pos + j - delay;
            if (dest >= 0 && dest < length) for (int ch = 0; ch < processingChannels; ++ch) {
                const float v = out.getSample(ch, j);
                output.setSample(ch, dest, std::isfinite(v) ? v : 0);
            }
        }
    }
    if (channels == 2 && processingChannels == 1) output.copyFrom(1, 0, output.getReadPointer(0), length, float(stereoFactor));
    // Linked normalisation prevents clipping without changing stereo relationships.
    float peak = 0; for (int ch = 0; ch < channels; ++ch) peak = std::max(peak, output.getMagnitude(ch, 0, length));
    if (peak > 0.999f) output.applyGain(0.999f / peak);
    if (progress) progress(1);
    return output;
}
juce::AudioBuffer<float> render(const Clip& c, const Settings& s, const Progress& progress) {
    if (s.adaptive) return renderAdaptive(c, s, progress);
    bool changes = !s.directShift && !s.matchReference && s.tightness > 0;
    for (const auto& n : c.notes) changes = changes || std::abs(correctionAt(c, s, (n.start + n.end) * 0.5)) > 1e-9;
    if (!changes) { if (progress) progress(1); return c.audio; }
    std::vector<double> trims(c.notes.size(), 0);
    const int analysisChannel = c.audio.getNumChannels() == 1 || c.audio.getRMSLevel(0, 0, c.audio.getNumSamples()) >= c.audio.getRMSLevel(1, 0, c.audio.getNumSamples()) ? 0 : 1;
    juce::AudioBuffer<float> output;
    // Small spectral shifts can differ by a few cents from the requested ratio.
    // Measure the rendered result against the source trajectory and refine offline.
    // This targets centre accuracy without flattening the original vibrato/glides.
    for (int pass = 0; pass < 4; ++pass) {
        output = renderPass(c, s, trims, [&](double p) { return !progress || progress((pass + p) / 4); });
        if (output.getNumSamples() != c.audio.getNumSamples()) return {};
        if (pass == 3) break;
        bool refine = false;
        for (size_t index = 0; index < c.notes.size(); ++index) {
            const auto& n = c.notes[index]; std::vector<double> errors;
            const double margin = std::max(0.07, s.smoothingMs / 1000 + 0.04);
            if (n.end - n.start < margin * 2 + 0.06) continue;
            for (int point = 0; point < 16; ++point) {
                const double time = n.start + margin + (n.end - n.start - 2 * margin) * (point + 0.5) / 16;
                const auto f = std::lower_bound(c.frames.begin(), c.frames.end(), time, [](const Frame& frame, double t) { return frame.time < t; });
                if (f == c.frames.end() || f->midi == 0 || f->confidence < 0.9 || f->time >= n.end - margin) continue;
                const int start = int(std::lround(f->time * c.sampleRate)) - 2048;
                if (start < 0 || start + 4096 > output.getNumSamples()) continue;
                const auto [frequency, confidence] = detectPitch(output.getReadPointer(analysisChannel, start), 4096, c.sampleRate);
                if (frequency <= 0 || confidence < 0.9) continue;
                const double desired = f->midi + correctionAt(c, s, f->time);
                const double error = desired - hzToMidi(frequency);
                if (std::abs(error) < 0.35) errors.push_back(error);
            }
            if (errors.size() >= 4) {
                const double error = median(errors);
                if (std::abs(error) > 0.003) { trims[index] += juce::jlimit(-0.15, 0.15, error); refine = true; }
            }
        }
        if (!refine) break;
    }
    if (progress) progress(1);
    return output;
}
std::shared_ptr<Clip> loadAudio(const juce::File& file, juce::String& error) {
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
    if (!reader) { error = "Cannot read audio file"; return {}; }
    if (reader->numChannels < 1 || reader->numChannels > 2 || reader->sampleRate < 8000 || reader->sampleRate > 192000 || reader->lengthInSamples > maxSeconds * reader->sampleRate || reader->lengthInSamples * reader->numChannels * 4 > maxAudioBytes) {
        error = "Use mono/stereo audio, up to 600 seconds / 240 MiB PCM, 8-192 kHz"; return {};
    }
    auto c = std::make_shared<Clip>(); c->sampleRate = reader->sampleRate; c->name = file.getFileName();
    c->audio.setSize(int(reader->numChannels), int(reader->lengthInSamples));
    if (!reader->read(&c->audio, 0, c->audio.getNumSamples(), 0, true, true)) { error = "Audio read failed"; return {}; }
    return c;
}
bool writeWav(const juce::File& f, const juce::AudioBuffer<float>& audio, double sr) {
    if (audio.getNumSamples() < 1 || f.existsAsFile()) return false;
    std::unique_ptr<juce::OutputStream> stream = f.createOutputStream(); if (!stream) return false;
    juce::WavAudioFormat format;
    auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions{}.withSampleRate(sr).withNumChannels(audio.getNumChannels()).withBitsPerSample(24));
    if (!writer) return false;
    return writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples());
}
bool writeMidi(const juce::File& f, const Clip& c, const Settings& s) {
    if (f.existsAsFile()) return false;
    juce::MidiMessageSequence sequence;
    sequence.addEvent(juce::MidiMessage::tempoMetaEvent(500000), 0);
    // Explicit +/-2 semitone pitch-bend sensitivity (RPN 0).
    sequence.addEvent(juce::MidiMessage::controllerEvent(1, 101, 0), 0);
    sequence.addEvent(juce::MidiMessage::controllerEvent(1, 100, 0), 0);
    sequence.addEvent(juce::MidiMessage::controllerEvent(1, 6, 2), 0);
    sequence.addEvent(juce::MidiMessage::controllerEvent(1, 38, 0), 0);
    sequence.addEvent(juce::MidiMessage::controllerEvent(1, 101, 127), 0);
    sequence.addEvent(juce::MidiMessage::controllerEvent(1, 100, 127), 0);
    for (const auto& n : c.notes) {
        const auto t = (n.start + n.end) / 2;
        const auto p = n.centre + correctionAt(c, s, t);
        const auto key = juce::jlimit(0, 127, int(std::lround(p)));
        const auto bend = juce::jlimit(0, 16383, int(std::lround(8192 + (p - key) * 4096)));
        const double ticks = 1920; // 960 PPQ, fixed tempo 120 BPM.
        sequence.addEvent(juce::MidiMessage::pitchWheel(1, bend), n.start * ticks);
        sequence.addEvent(juce::MidiMessage::noteOn(1, key, juce::uint8(96)), n.start * ticks);
        for (const auto& frame : c.frames) if (frame.time > n.start && frame.time < n.end && frame.midi > 0) {
            const double pitch = frame.midi + correctionAt(c, s, frame.time);
            const int v = juce::jlimit(0, 16383, int(std::lround(8192 + (pitch - key) * 4096)));
            sequence.addEvent(juce::MidiMessage::pitchWheel(1, v), frame.time * ticks);
        }
        sequence.addEvent(juce::MidiMessage::noteOff(1, key), n.end * ticks);
    }
    sequence.updateMatchedPairs(); juce::MidiFile midi; midi.setTicksPerQuarterNote(960); midi.addTrack(sequence);
    auto stream = f.createOutputStream(); return stream && midi.writeTo(*stream);
}
bool importScala(const juce::File& file, Settings& s, juce::String& error) {
    juce::StringArray lines; lines.addLines(file.loadFileAsString()); juce::StringArray clean;
    for (auto line : lines) { line = line.upToFirstOccurrenceOf("!", false, false).trim(); if (line.isNotEmpty()) clean.add(line); }
    if (clean.size() < 14 || clean[1].getIntValue() != 12) { error = "Only 12-step octave Scala scales are supported"; return false; }
    std::array<double,12> offsets{};
    double previous = 0;
    for (int i = 1; i <= 12; ++i) {
        const auto token = clean[i + 1].upToFirstOccurrenceOf(" ", false, false);
        if (!token.containsOnly("0123456789.+-/")) { error = "Invalid scale interval"; return false; }
        double cents = 0;
        if (token.containsChar('/')) { const auto a = token.upToFirstOccurrenceOf("/", false, false).getDoubleValue(), b = token.fromFirstOccurrenceOf("/", false, false).getDoubleValue(); if (a <= 0 || b <= 0) { error = "Invalid ratio"; return false; } cents = 1200 * std::log2(a / b); }
        else if (token.containsChar('.')) cents = token.getDoubleValue();
        else { const double ratio = token.getDoubleValue(); if (ratio <= 0) { error = "Invalid interval"; return false; } cents = 1200 * std::log2(ratio); }
        if (!std::isfinite(cents) || cents <= previous || std::abs(cents - i * 100) > 200 || (i == 12 && std::abs(cents - 1200) > 0.01)) { error = "Use ascending 12-step octave scales with offsets within +/-200 cents"; return false; }
        previous = cents;
        if (i < 12) offsets[size_t(i)] = cents - i * 100;
    }
    s.cents = offsets; return true;
}
juce::ValueTree settingsToTree(const Settings& s) {
    juce::ValueTree t("Settings");
    t.setProperty("adaptive", s.adaptive, nullptr); t.setProperty("minimumIntervention", s.minimumIntervention, nullptr); t.setProperty("precisionCents", s.precisionCents, nullptr);
    for (int i = 0; i < 12; ++i) t.setProperty("c" + juce::String(i), s.cents[size_t(i)], nullptr);
    for (int i = 0; i < 12; ++i) t.setProperty("step" + juce::String(i), s.semitones[size_t(i)], nullptr);
    t.setProperty("direct", s.directShift, nullptr);
    t.setProperty("matchReference", s.matchReference, nullptr);
    for (int i = 0; i < 12; ++i) t.setProperty("g" + juce::String(i), s.groupStrength[size_t(i)], nullptr);
    t.setProperty("octave", s.octaveMode, nullptr); t.setProperty("formants", s.formants, nullptr);
    t.setProperty("strength", s.strength, nullptr); t.setProperty("tightness", s.tightness, nullptr);
    t.setProperty("vibrato", s.vibrato, nullptr); t.setProperty("smoothing", s.smoothingMs, nullptr);
    t.setProperty("humanize", s.humanize, nullptr); t.setProperty("reference", s.reference, nullptr); t.setProperty("seed", s.seed, nullptr);
    for (auto [key, cents] : s.octaveCents) { juce::ValueTree o("Octave"); o.setProperty("key", key, nullptr); o.setProperty("cents", cents, nullptr); t.addChild(o, -1, nullptr); }
    return t;
}
Settings settingsFromTree(const juce::ValueTree& t) {
    Settings s;
    auto value = [&](const juce::String& name, double fallback, double lo, double hi) { const double v = t.getProperty(name, fallback); return std::isfinite(v) ? juce::jlimit(lo, hi, v) : fallback; };
    for (int i = 0; i < 12; ++i) s.cents[size_t(i)] = value("c" + juce::String(i), 0, -200, 200);
    for (int i = 0; i < 12; ++i) s.semitones[size_t(i)] = int(value("step" + juce::String(i), 0, -12, 12));
    s.adaptive = t.getProperty("adaptive", false); s.minimumIntervention = t.getProperty("minimumIntervention", true); s.precisionCents = value("precisionCents", 1, 0.1, 25);
    s.directShift = t.getProperty("direct", false);
    s.matchReference = t.getProperty("matchReference", false);
    for (int i = 0; i < 12; ++i) s.groupStrength[size_t(i)] = value("g" + juce::String(i), 1, 0, 1);
    s.octaveMode = t.getProperty("octave", false); s.formants = t.getProperty("formants", true);
    s.strength = value("strength", 0, 0, 1); s.tightness = value("tightness", 0, 0, 1); s.vibrato = value("vibrato", 1, 0, 1);
    s.smoothingMs = value("smoothing", 30, 0, 250); s.humanize = value("humanize", 0, 0, 50); s.reference = value("reference", 440, 400, 480);
    s.seed = int(t.getProperty("seed", 1));
    for (auto o : t) if (o.hasType("Octave")) s.octaveCents[juce::jlimit(0, 127, int(o["key"]))] = juce::jlimit(-200.0, 200.0, double(o["cents"]));
    return s;
}
juce::ValueTree clipToTree(const Clip& c) {
    juce::ValueTree t("Clip"); t.setProperty("adaptiveAnalysis", c.adaptiveAnalysis, nullptr); t.setProperty("rate", c.sampleRate, nullptr); t.setProperty("anchor", c.anchorSeconds, nullptr); t.setProperty("name", c.name, nullptr);
    t.setProperty("channels", c.audio.getNumChannels(), nullptr); t.setProperty("samples", c.audio.getNumSamples(), nullptr);
    juce::MemoryOutputStream raw;
    for (int ch = 0; ch < c.audio.getNumChannels(); ++ch) for (int i = 0; i < c.audio.getNumSamples(); ++i) raw.writeFloat(c.audio.getSample(ch, i));
    t.setProperty("audio", juce::var(raw.getMemoryBlock()), nullptr);
    for (const auto& f : c.frames) { juce::ValueTree v("Frame"); v.setProperty("t", f.time, nullptr); v.setProperty("m", f.midi, nullptr); v.setProperty("q", f.confidence, nullptr); v.setProperty("r", f.rms, nullptr); v.setProperty("u", f.uncertainty, nullptr); v.setProperty("onset", f.onset, nullptr); v.setProperty("agreement", f.agreement, nullptr); t.addChild(v, -1, nullptr); }
    for (const auto& n : c.notes) { juce::ValueTree v("Note"); v.setProperty("start", n.start, nullptr); v.setProperty("end", n.end, nullptr); v.setProperty("centre", n.centre, nullptr); v.setProperty("confidence", n.confidence, nullptr); v.setProperty("key", n.key, nullptr); v.setProperty("detached", n.detached, nullptr); v.setProperty("target", n.target, nullptr); v.setProperty("strength", n.strength, nullptr); v.setProperty("stableStart", n.stableStart, nullptr); v.setProperty("stableEnd", n.stableEnd, nullptr); v.setProperty("uncertainty", n.uncertainty, nullptr); v.setProperty("discrete", n.discrete, nullptr); v.setProperty("overlap", n.overlap, nullptr); t.addChild(v, -1, nullptr); }
    return t;
}
std::shared_ptr<Clip> clipFromTree(const juce::ValueTree& t) {
    if (!t.hasType("Clip")) return {};
    auto c = std::make_shared<Clip>(); c->adaptiveAnalysis = t.getProperty("adaptiveAnalysis", false); c->sampleRate = t["rate"]; const int channels = t["channels"], samples = t["samples"];
    if (!std::isfinite(c->sampleRate) || c->sampleRate < 8000 || c->sampleRate > 192000 || channels < 1 || channels > 2 || samples < 1 || samples > maxSeconds * c->sampleRate) return {};
    const auto* raw = t["audio"].getBinaryData();
    if (!raw || raw->getSize() > size_t(maxAudioBytes) || raw->getSize() != size_t(channels) * size_t(samples) * 4) return {};
    juce::MemoryInputStream stream(*raw, false); c->audio.setSize(channels, samples);
    for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < samples; ++i) { const auto v = stream.readFloat(); c->audio.setSample(ch, i, std::isfinite(v) ? v : 0); }
    c->anchorSeconds = t["anchor"]; if (!std::isfinite(c->anchorSeconds)) c->anchorSeconds = 0; c->name = t["name"].toString();
    for (auto v : t) {
        if (v.hasType("Frame")) { Frame f{double(v["t"]), double(v["m"]), double(v["q"]), double(v["r"]), double(v["u"]), double(v["onset"]), double(v.getProperty("agreement", 1.0))}; if (std::isfinite(f.time) && std::isfinite(f.midi) && std::isfinite(f.confidence)) c->frames.push_back(f); }
        if (v.hasType("Note")) { Note n{double(v["start"]), double(v["end"]), double(v["centre"]), double(v["confidence"]), int(v["key"]), bool(v["detached"]), double(v["target"]), double(v["strength"]), double(v["stableStart"]), double(v["stableEnd"]), double(v["uncertainty"]), bool(v["discrete"]), bool(v["overlap"])}; if (std::isfinite(n.start) && std::isfinite(n.end) && std::isfinite(n.centre) && std::isfinite(n.target) && std::isfinite(n.strength) && n.start >= 0 && n.end > n.start && n.end <= samples / c->sampleRate && n.key >= 0 && n.key <= 127) c->notes.push_back(n); }
    }
    std::sort(c->frames.begin(), c->frames.end(), [](auto& a, auto& b){ return a.time < b.time; });
    std::sort(c->notes.begin(), c->notes.end(), [](auto& a, auto& b){ return a.start < b.start; });
    return c;
}
}
