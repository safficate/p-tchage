#include "AdaptivePitch.h"
#include "signalsmith-stretch.h"
#include <numeric>

namespace mp {
namespace {
double median(std::vector<double> values) {
    if (values.empty()) return 0;
    const auto middle = values.begin() + std::ptrdiff_t(values.size() / 2);
    std::nth_element(values.begin(), middle, values.end()); return *middle;
}
double weightedCentre(const std::vector<std::pair<double, double>>& values) {
    if (values.empty()) return 0;
    auto sorted = values; std::sort(sorted.begin(), sorted.end());
    double weight = 0; for (auto p : sorted) weight += p.second;
    double sum = 0, centre = sorted.back().first;
    for (auto p : sorted) if ((sum += p.second) >= weight / 2) { centre = p.first; break; }
    std::vector<double> errors; for (auto p : values) errors.push_back(std::abs(p.first - centre));
    const double scale = std::max(0.025, 1.4826 * median(errors));
    for (int iteration = 0; iteration < 4; ++iteration) {
        double total = 0, weights = 0;
        for (auto [value, w] : values) { const double robust = w * std::min(1.0, 1.5 * scale / std::max(1e-9, std::abs(value - centre))); total += robust * value; weights += robust; }
        if (weights > 0) centre = total / weights;
    }
    return centre;
}
double amplitude(const float* samples, int count, double sr, double hz) {
    std::complex<double> total{}; const double step = -2 * juce::MathConstants<double>::pi * hz / sr;
    for (int i = 0; i < count; ++i) total += double(samples[i]) * (0.5 - 0.5 * std::cos(2 * juce::MathConstants<double>::pi * i / std::max(1, count - 1))) * std::polar(1.0, step * i);
    return std::abs(total) * 4 / count;
}
// Only lower an apparent second harmonic when independent odd harmonics
// provide actual evidence. A pure tone must not be relabelled one octave down.
double fundamentalCandidate(const float* samples, int count, double sr, double hz) {
    if (hz < 65) return hz;
    const double peak = amplitude(samples, count, sr, hz);
    if (peak < 1e-5) return hz;
    const double first = amplitude(samples, count, sr, hz / 2);
    const double third = amplitude(samples, count, sr, hz * 1.5);
    const double fifth = amplitude(samples, count, sr, hz * 2.5);
    if ((first > peak * 0.025 && third > peak * 0.04) || (third > peak * 0.10 && fifth > peak * 0.025)) return hz / 2;
    return hz;
}
// Verification only: the played note is already established from periodicity.
// Fit its known fundamental in a narrow band on a stable, long observation.
// This is not a strongest-FFT-bin note detector and is skipped for missing F0.
double stableSpectralCentre(const float* data, int count, double sr, double expectedHz) {
    double energy=0; for(int i=0;i<count;++i)energy+=double(data[i])*data[i];
    if(count<8192 || amplitude(data,count,sr,expectedHz)<std::sqrt(energy/count)*0.08)return 0;
    int order=1;while((1<<order)<count*8)++order;const int size=1<<order;
    juce::dsp::FFT fft(order);std::vector<juce::dsp::Complex<float>> input(size_t(size),{0,0}),spectrum(size_t(size),{0,0});
    for(int i=0;i<count;++i)input[size_t(i)]={float(data[i]*(0.5-0.5*std::cos(2*juce::MathConstants<double>::pi*i/(count-1)))),0};
    fft.perform(input.data(),spectrum.data(),false);
    const int lo=std::max(2,int(expectedHz*std::exp2(-0.3/12)*size/sr)),hi=std::min(size/2-2,int(expectedHz*std::exp2(0.3/12)*size/sr));
    int best=lo;for(int i=lo+1;i<=hi;++i)if(std::norm(spectrum[size_t(i)])>std::norm(spectrum[size_t(best)]))best=i;
    if(best==lo||best==hi)return 0;
    const double a=std::log(std::max(1e-30f,std::norm(spectrum[size_t(best-1)]))),b=std::log(std::max(1e-30f,std::norm(spectrum[size_t(best)]))),c=std::log(std::max(1e-30f,std::norm(spectrum[size_t(best+1)])));
    const double denominator=a-2*b+c,delta=std::abs(denominator)>1e-12?0.5*(a-c)/denominator:0;
    return (best+juce::jlimit(-0.5,0.5,delta))*sr/size;
}
void curve(CorrectionPlan& plan, double length) {
    plan.segments.clear();
    if (plan.regions.empty()) return;
    auto value = [](const CorrectionRegion& r) { return r.appliedCents + (std::abs(r.appliedCents) > 1e-9 ? r.calibrationCents : 0); };
    const auto& first = plan.regions.front();
    double end = 0, previous = value(first);
    if (first.start > 0.04) { plan.segments.push_back({0, first.start - 0.03, 0, 0}); plan.segments.push_back({first.start - 0.03, first.start, 0, previous}); end = first.start; }
    for (size_t i = 1; i < plan.regions.size(); ++i) {
        const auto& a = plan.regions[i - 1]; const auto& b = plan.regions[i];
        double beginTransition, endTransition;
        if (b.start - a.end > 0.04) { beginTransition = a.end; endTransition = b.start; }
        else { beginTransition = a.stableEnd; endTransition = b.stableStart; }
        beginTransition = std::max(end, beginTransition); endTransition = std::max(beginTransition + 0.001, endTransition);
        plan.segments.push_back({end, beginTransition, previous, previous});
        plan.segments.push_back({beginTransition, endTransition, previous, value(b)});
        previous = value(b); end = endTransition;
    }
    plan.segments.push_back({end, std::max(length, end + 0.001), previous, previous});
}
juce::AudioBuffer<float> process(const Clip& c, const Settings& s, const CorrectionPlan& plan, const Progress& progress) {
    if (!plan.changesAudio()) return c.audio;
    const int channels = c.audio.getNumChannels(), length = c.audio.getNumSamples();
    juce::AudioBuffer<float> output(channels, length); output.clear();
    int linkedChannels = channels; double factor = 1;
    if (channels == 2) {
        double energy = 0, cross = 0;
        for (int i = 0; i < length; ++i) { const double l = c.audio.getSample(0, i); energy += l * l; cross += l * c.audio.getSample(1, i); }
        if (energy > 1e-10) {
            factor = cross / energy; double residual = 0;
            for (int i = 0; i < length; ++i) { const double error = c.audio.getSample(1, i) - factor * c.audio.getSample(0, i); residual += error * error; }
            if (residual / energy < 1e-12) linkedChannels = 1;
        }
    }
    signalsmith::stretch::SignalsmithStretch<float> stretch(1); stretch.presetDefault(linkedChannels, float(c.sampleRate));
    const int delay = stretch.inputLatency() + stretch.outputLatency(); constexpr int block = 128;
    juce::AudioBuffer<float> input(linkedChannels, block), work(linkedChannels, block);
    const float* ip[2] = {input.getReadPointer(0), input.getReadPointer(linkedChannels - 1)};
    float* op[2] = {work.getWritePointer(0), work.getWritePointer(linkedChannels - 1)};
    for (int pos = 0; pos < length + delay; pos += block) {
        if (progress && !progress(double(pos) / (length + delay))) return {};
        input.clear();
        for (int ch = 0; ch < linkedChannels; ++ch) if (pos < length) input.copyFrom(ch, 0, c.audio, ch, pos, std::min(block, length - pos));
        const double time = (pos - stretch.inputLatency() + block * 0.5) / c.sampleRate;
        stretch.setTransposeSemitones(float(plan.at(time) / 100));
        stretch.setFormantFactor(1, s.formants);
        auto frame = std::lower_bound(c.frames.begin(), c.frames.end(), time, [](const Frame& f, double t) { return f.time < t; });
        stretch.setFormantBase(float(midiToHz(frame != c.frames.end() && frame->midi > 0 ? frame->midi : 57) / c.sampleRate));
        stretch.process(ip, block, op, block);
        for (int i = 0; i < block; ++i) {
            const int destination = pos + i - delay; if (destination < 0 || destination >= length) continue;
            for (int ch = 0; ch < linkedChannels; ++ch) output.setSample(ch, destination, std::isfinite(work.getSample(ch, i)) ? work.getSample(ch, i) : 0);
        }
    }
    if (channels == 2 && linkedChannels == 1) output.copyFrom(1, 0, output.getReadPointer(0), length, float(factor));
    for (int i = 0; i < length; ++i) {
        const double time = i / c.sampleRate; const float wet = float(plan.wetAt(time));
        for (int ch = 0; ch < channels; ++ch) {
            const float original = c.audio.getSample(ch, i);
            output.setSample(ch, i, wet == 0 ? original : original + wet * (output.getSample(ch, i) - original));
        }
    }
    return output;
}
}
void analyseAdaptive(Clip& clip, const Progress& progress) {
    clip.frames.clear(); clip.notes.clear(); clip.adaptiveAnalysis = false;
    constexpr int hop = 512, window = 4096, longWindow = 8192;
    const int samples = clip.audio.getNumSamples(); if (samples < window || clip.audio.getNumChannels() == 0) return;
    const double sr = clip.sampleRate, length = samples / sr;
    double previousShort = 0, previousMidi = 0;
    for (int pos = 0; pos + window <= samples; pos += hop) {
        if (progress && !progress(0.75 * pos / std::max(1, samples))) return;
        const int ch = clip.audio.getNumChannels() == 1 || clip.audio.getRMSLevel(0, pos, window) >= clip.audio.getRMSLevel(1, pos, window) ? 0 : 1;
        const auto* audio = clip.audio.getReadPointer(ch); const double time = (pos + window / 2.0) / sr;
        const double rms = clip.audio.getRMSLevel(ch, pos, window);
        const double shortRms = clip.audio.getRMSLevel(ch, pos + window / 2 - 512, 1024);
        const double onset = previousShort > 1e-5 ? std::max(0.0, (shortRms - previousShort) / std::max(shortRms * 0.2, previousShort)) : 0;
        previousShort = shortRms;
        auto [hz, confidence] = detectPitch(audio + pos, window, sr);
        double agreement = 1, uncertainty = 0.15;
        if (hz > 0 && (hz < 150 || (int(clip.frames.size()) % 4 == 0) || std::abs(hzToMidi(hz) - previousMidi) > 4)) {
            const int longStart = juce::jlimit(0, std::max(0, samples - longWindow), pos - window / 2);
            if (longStart + longWindow <= samples) {
                const auto [longHz, longConfidence] = detectPitch(audio + longStart, longWindow, sr);
                if (longHz > 0 && longConfidence >= 0.8) {
                    const double distance = std::abs(hzToMidi(hz) - hzToMidi(longHz));
                    agreement = distance < 0.12 ? 1 : distance > 10 ? 0.25 : 0.55;
                    uncertainty = std::max(0.15, std::min(35.0, distance * 50));
                    if (hz < 150 && agreement > 0.8) { hz = longHz; confidence = std::min(confidence, longConfidence); }
                }
            }
        }
        if (hz > 0 && (clip.frames.size() % 4 == 0 || std::abs(hzToMidi(hz) - previousMidi) > 6)) {
            const double candidate = fundamentalCandidate(audio + pos, window, sr, hz);
            if (candidate != hz) { hz = candidate; agreement = std::max(0.85, agreement); uncertainty = std::max(uncertainty, 1.0); }
        } else if (hz > 0 && previousMidi > 0 && std::abs(hzToMidi(hz) - previousMidi - 12) < 0.15 && onset < 0.2) {
            // Recheck changed harmonic dominance instead of blindly following it.
            hz = fundamentalCandidate(audio + pos, window, sr, hz);
        }
        const double midi = hz > 0 ? hzToMidi(hz) : 0;
        clip.frames.push_back({time, midi, confidence * (0.75 + 0.25 * agreement), rms, uncertainty, onset, agreement});
        if (midi > 0) previousMidi = midi;
    }
    // Stable plateaus use a local robust trend. Oscillation is not itself a note
    // boundary; connected glides are left between plateau anchors.
    struct Anchor { int begin = 0, end = 0; double centre = 0; };
    std::vector<Anchor> anchors; int begin = -1; double previousCentre = 0;
    const int radius = std::max(4, int(sr * 0.10 / hop));
    auto finish = [&](int end) { if (begin >= 0 && end - begin >= 4) anchors.push_back({begin, end, previousCentre}); begin = -1; };
    for (int i = 0; i < int(clip.frames.size()); ++i) {
        const auto& f = clip.frames[size_t(i)];
        std::vector<std::pair<double,double>> local; double sumT = 0, sumP = 0, sumTT = 0, sumTP = 0; int count = 0;
        for (int j = std::max(0, i - radius); j <= std::min(int(clip.frames.size()) - 1, i + radius); ++j) {
            const auto& v = clip.frames[size_t(j)]; if (v.midi <= 0 || v.confidence < 0.8) continue;
            const double t = v.time - f.time; local.push_back({v.midi, v.confidence * v.confidence}); sumT += t; sumP += v.midi; sumTT += t*t; sumTP += t*v.midi; ++count;
        }
        const double centre = weightedCentre(local);
        const double slope = count > 3 ? (sumTP - sumT * sumP / count) / std::max(1e-8, sumTT - sumT * sumT / count) : 100;
        const bool reliable = f.midi > 0 && f.confidence >= 0.8 && f.agreement >= 0.5 && count >= 4;
        const bool stable = reliable && std::abs(slope) < 1.2;
        const bool newPitch = begin >= 0 && std::abs(centre - previousCentre) > 0.55;
        const bool reattack = begin >= 0 && f.onset > 0.7 && f.time - clip.frames[size_t(begin)].time > 0.12;
        if (!stable || newPitch || reattack) finish(i);
        if (stable) { if (begin < 0) begin = i; previousCentre = centre; }
    }
    finish(int(clip.frames.size()));
    // Adjacent stable runs at the same pitch merge across short vibrato-related
    // trend excursions, but never across an actual re-attack or another plateau.
    std::vector<Anchor> merged;
    for (auto anchor : anchors) {
        bool continuousGap = true;
        if (!merged.empty()) for (int j = merged.back().end; j < anchor.begin; ++j) if (clip.frames[size_t(j)].midi <= 0 || clip.frames[size_t(j)].confidence < 0.8 || clip.frames[size_t(j)].onset > 0.5) continuousGap = false;
        if (continuousGap && !merged.empty() && clip.frames[size_t(anchor.begin)].time - clip.frames[size_t(merged.back().end-1)].time < 0.22
            && std::abs(anchor.centre - merged.back().centre) < 0.45 && clip.frames[size_t(anchor.begin)].onset < 0.5) merged.back().end = anchor.end;
        else merged.push_back(anchor);
    }
    for (const auto& a : merged) {
        std::vector<std::pair<double,double>> values; std::vector<double> uncertainties;
        double confidence = 0;
        for (int i = a.begin; i < a.end; ++i) { const auto& f = clip.frames[size_t(i)]; if (f.midi > 0 && f.confidence >= 0.8) { values.push_back({f.midi, f.confidence*f.confidence}); uncertainties.push_back(f.uncertainty); confidence += f.confidence; } }
        if (values.size() < 4) continue;
        const double centre = weightedCentre(values); std::vector<double> errors; for (auto p : values) errors.push_back(std::abs(p.first - centre));
        const double stableStart = clip.frames[size_t(a.begin)].time, stableEnd = clip.frames[size_t(a.end-1)].time;
        const double uncertainty = std::max(median(uncertainties), 100 * median(errors) * 0.20 / std::sqrt(std::max(1.0, (stableEnd-stableStart)*5)));
        Note n; n.start = std::max(0.0, stableStart - window / (2*sr)); n.end = std::min(length, stableEnd + window / (2*sr));
        n.centre = centre; n.confidence = confidence / values.size(); n.key = juce::jlimit(0,127,int(std::lround(centre)));
        n.stableStart = stableStart; n.stableEnd = stableEnd; n.uncertainty = uncertainty;
        const int at = int(n.start * sr), earlyCount = std::min(int(sr*0.06), samples-at), lateAt = std::min(samples-1,int((n.start + std::min(0.3,(n.end-n.start)*0.6))*sr));
        const double early = clip.audio.getRMSLevel(0,at,std::max(1,earlyCount)), late = clip.audio.getRMSLevel(0,lateAt,std::min(int(sr*0.08),samples-lateAt));
        n.discrete = early > 1e-5 && late / early < 0.83;
        if (n.discrete) {
            // Stable F0 can arrive well after a pick/hammer attack. Anchor the
            // protection to short-window energy evidence, not that late plateau.
            double strongest = 0, attackTime = -1;
            auto candidate = std::lower_bound(clip.frames.begin(), clip.frames.end(), stableStart-0.18,
                [](const Frame& f, double t) { return f.time<t; });
            for (; candidate!=clip.frames.end() && candidate->time<=stableStart; ++candidate) {
                const auto& f=*candidate;
                if (f.midi > 0 && f.confidence >= 0.8 && std::abs(f.midi-centre)<0.5 && f.onset>0.7 && f.onset>strongest) {
                    strongest=f.onset; attackTime=f.time;
                }
            }
            if (attackTime>=0) n.start=std::max(0.0,attackTime-0.012);
        }
        if (clip.notes.empty() && !clip.frames.empty() && clip.frames.front().midi > 0 && clip.frames.front().confidence >= 0.8 && std::abs(clip.frames.front().midi-centre)<0.55) n.start=0;
        clip.notes.push_back(n);
    }
    for (size_t i = 1; i < clip.notes.size(); ++i) {
        auto& a = clip.notes[i-1]; auto& b = clip.notes[i];
        const double boundary = (a.stableEnd + b.stableStart) / 2;
        if (a.end > b.start) { a.end = boundary; b.start = boundary; }
        const int start = int(b.stableStart*sr), count = std::min(4096,samples-start);
        if ((a.discrete || b.discrete) && b.start-a.end < 0.08 && std::abs(a.centre-b.centre)>0.5 && count > 1024) {
            const auto* audio = clip.audio.getReadPointer(0,start);
            const double old = amplitude(audio,count,sr,midiToHz(a.centre)), now = amplitude(audio,count,sr,midiToHz(b.centre));
            if (old > std::max(0.003, now*0.18)) { a.overlap = b.overlap = true; }
        }
    }
    clip.adaptiveAnalysis = true; if (progress) progress(1);
}
double CorrectionSegment::at(double time) const {
    if (end <= start) return toCents;
    const double x = juce::jlimit(0.0,1.0,(time-start)/(end-start));
    const double smooth = x*x*x*(10+x*(-15+6*x));
    return fromCents + smooth*(toCents-fromCents);
}
double CorrectionPlan::at(double time) const {
    if (segments.empty()) return 0;
    auto it = std::upper_bound(segments.begin(),segments.end(),time,[](double t,const CorrectionSegment& p){return t<p.start;});
    return it == segments.begin() ? segments.front().fromCents : (--it)->at(time);
}
double CorrectionPlan::wetAt(double time) const {
    double wet = 1;
    auto next = std::upper_bound(regions.begin(), regions.end(), time,
        [](double t, const CorrectionRegion& r) { return t<r.start; });
    while (next!=regions.begin()) {
        const auto& r=*--next;
        if (time-r.start>=0.080) break;
        if (r.discrete) {
            const double x = juce::jlimit(0.0,1.0,(time-r.start-0.040)/0.04); wet = std::min(wet,x*x*(3-2*x));
        }
    }
    // A completely inactive interval is returned verbatim, with a short bridge
    // derived from neighbouring planned correction rather than generic note fades.
    if (std::abs(at(time)) < 1e-9 && std::abs(at(time-0.025)) < 1e-9 && std::abs(at(time+0.025)) < 1e-9) return 0;
    return wet;
}
bool CorrectionPlan::changesAudio() const { for (const auto& r : regions) if (std::abs(r.appliedCents)>1e-9) return true; return false; }
CorrectionPlan planCorrection(const Clip& c,const Settings& s) {
    CorrectionPlan plan; Settings constant = s; constant.smoothingMs = constant.tightness = constant.humanize = 0;
    for (const auto& n : c.notes) {
        CorrectionRegion r; r.start=n.start; r.end=n.end; r.stableStart=n.stableEnd>n.stableStart?n.stableStart:n.start+std::min(0.08,(n.end-n.start)*0.2); r.stableEnd=n.stableEnd>n.stableStart?n.stableEnd:n.end-std::min(0.08,(n.end-n.start)*0.2);
        r.centre=n.centre; r.uncertaintyCents=n.uncertainty; r.confidence=n.confidence; r.discrete=n.discrete; r.overlap=n.overlap;
        r.requestedCents=100*correctionAt(c,constant,(n.start+n.end)/2); r.appliedCents=r.requestedCents;
        const double threshold = std::max(s.precisionCents, 1.5*r.uncertaintyCents);
        if (std::abs(r.requestedCents)<1e-8) { r.appliedCents=0; r.reason="No requested displacement"; }
        else if (r.confidence<0.8 || r.uncertaintyCents>30) { r.appliedCents=0; r.decision=RegionDecision::Review; r.reason="Uncertain centre: analysis only"; }
        else if (s.minimumIntervention && std::abs(r.requestedCents)<=threshold) { r.appliedCents=0; r.reason="Within precision/centre uncertainty: preserve original"; }
        else { r.decision=RegionDecision::Translate; r.reason=r.discrete?"Constant event translation; protect attack":"Translate centre; preserve pitch motion"; }
        plan.regions.push_back(r);
    }
    struct Group { size_t begin, end; double weight; };
    std::vector<Group> groups;
    for (size_t begin=0; begin<plan.regions.size();) {
        size_t end=begin+1;
        while (end<plan.regions.size()) {
            const auto& a=plan.regions[end-1]; const auto& b=plan.regions[end];
            if (!(a.overlap||b.overlap) || b.start-a.end>=0.08) break;
            ++end;
        }
        double low=1e9,high=-1e9,total=0,weight=0; bool conflict=false,preserve=false;
        for(size_t i=begin;i<end;++i) {
            const auto& r=plan.regions[i]; const double w=std::max(0.01,r.confidence)/(1+r.uncertaintyCents*r.uncertaintyCents);
            weight+=w; total+=w*r.appliedCents; low=std::min(low,r.requestedCents); high=std::max(high,r.requestedCents);
            conflict|=r.decision==RegionDecision::Review; preserve|=r.decision==RegionDecision::None;
        }
        if(end>begin+1) for(size_t i=begin;i<end;++i) {
            auto& r=plan.regions[i];
            if(conflict || high-low>s.precisionCents) {
                r.appliedCents=0; r.decision=RegionDecision::Review;
                r.reason="Conflicting overlapping tails: entire connected group declined; no separation";
            } else if(preserve) {
                r.appliedCents=0; r.decision=RegionDecision::None;
                r.reason="Compatible overlap includes an unchanged event: preserve original group";
            } else {
                r.appliedCents=total/weight; r.decision=RegionDecision::Shared;
                r.reason="One shared displacement for the entire overlapping texture";
            }
        }
        groups.push_back({begin,end,weight}); begin=end;
    }
    // An overlapping group is one variable. A later conflict must propagate
    // backward as well as forward, and pair projections may not split a group.
    auto setGroup=[&](size_t group,double value) {
        for(size_t i=groups[group].begin;i<groups[group].end;++i) {
            auto& r=plan.regions[i]; r.appliedCents=value; r.decision=RegionDecision::Reduced;
            r.reason="Insufficient transition time: reduced correction, preserve original articulation";
        }
    };
    for(int iteration=0;iteration<64;++iteration) {
        bool moved=false;
        for(size_t i=1;i<groups.size();++i) {
            auto& a=plan.regions[groups[i-1].end-1]; auto& b=plan.regions[groups[i].begin];
            if(b.start-a.end>=0.04) continue;
            const double allowed=400*std::max(0.001,b.stableStart-a.stableEnd)/1.875;
            const double difference=b.appliedCents-a.appliedCents;
            if(std::abs(difference)<=allowed+1e-7) continue;
            const bool protectA=a.decision==RegionDecision::None||a.decision==RegionDecision::Review;
            const bool protectB=b.decision==RegionDecision::None||b.decision==RegionDecision::Review;
            const double centre=(groups[i-1].weight*a.appliedCents+groups[i].weight*b.appliedCents)/(groups[i-1].weight+groups[i].weight);
            const double half=std::copysign(allowed/2,difference),av=a.appliedCents,bv=b.appliedCents;
            if(!protectA) setGroup(i-1,protectB?juce::jlimit(bv-allowed,bv+allowed,av):centre-half);
            if(!protectB) setGroup(i,protectA?juce::jlimit(av-allowed,av+allowed,bv):centre+half);
            moved|=!protectA||!protectB;
        }
        if(!moved) break;
    }
    bool feasible=true;
    for(size_t i=1;i<plan.regions.size();++i) {
        const auto& a=plan.regions[i-1];const auto& b=plan.regions[i];
        if(b.start-a.end<0.04 && std::abs(b.appliedCents-a.appliedCents)>400*std::max(0.001,b.stableStart-a.stableEnd)/1.875+1e-6) feasible=false;
    }
    if(!feasible) for(auto& r:plan.regions) {
        r.appliedCents=0;r.decision=RegionDecision::Review;r.reason="Transition constraints did not converge: preserve original clip";
    }
    curve(plan,c.audio.getNumSamples()/c.sampleRate); return plan;
}
juce::String CorrectionPlan::summary() const {
    int changed=0,review=0,unverified=0; for(const auto& r:regions){ const bool active=std::abs(r.appliedCents)>1e-9; changed+=active; review+=r.decision==RegionDecision::Review||r.decision==RegionDecision::Reduced; unverified+=active&&!r.verificationPassed; }
    return juce::String(changed)+" bolge islendi; "+juce::String(review)+" bolge temkinli; "+juce::String(unverified)+" bolge hassasiyet teyidi gerektiriyor.";
}
juce::var CorrectionPlan::toJson() const {
    auto root=std::make_unique<juce::DynamicObject>(); juce::Array<juce::var> rows,segmentsJson;
    for(const auto& r:regions){auto o=std::make_unique<juce::DynamicObject>(); o->setProperty("start",r.start);o->setProperty("end",r.end);o->setProperty("stableStart",r.stableStart);o->setProperty("stableEnd",r.stableEnd);o->setProperty("centreMidi",r.centre);o->setProperty("uncertaintyCents",r.uncertaintyCents);o->setProperty("confidence",r.confidence);o->setProperty("requestedCents",r.requestedCents);o->setProperty("appliedCents",r.appliedCents);o->setProperty("calibrationCents",r.calibrationCents);o->setProperty("achievedCents",r.achievedCents);o->setProperty("verificationConfidence",r.verificationConfidence);o->setProperty("periodicAchievedCents",r.periodicAchievedCents);o->setProperty("spectralAchievedCents",r.spectralAchievedCents);o->setProperty("spectralVerification",r.spectralVerification);o->setProperty("verificationAvailable",r.verificationAvailable);o->setProperty("verificationPassed",r.verificationPassed);o->setProperty("decision",int(r.decision));o->setProperty("reason",r.reason);o->setProperty("discrete",r.discrete);o->setProperty("overlap",r.overlap);rows.add(juce::var(o.release()));}
    for(const auto& s:segments){auto o=std::make_unique<juce::DynamicObject>();o->setProperty("start",s.start);o->setProperty("end",s.end);o->setProperty("fromCents",s.fromCents);o->setProperty("toCents",s.toCents);segmentsJson.add(juce::var(o.release()));}
    root->setProperty("regions",rows);root->setProperty("segments",segmentsJson);root->setProperty("cancelled",cancelled);return juce::var(root.release());
}
juce::AudioBuffer<float> renderAdaptive(const Clip& c,const Settings& s,const Progress& progress,CorrectionPlan* result) {
    auto plan=planCorrection(c,s); juce::AudioBuffer<float> output;
    int verificationChannel=0;
    for(int ch=1;ch<c.audio.getNumChannels();++ch)
        if(c.audio.getRMSLevel(ch,0,c.audio.getNumSamples())>c.audio.getRMSLevel(verificationChannel,0,c.audio.getNumSamples()))verificationChannel=ch;
    if (progress && !progress(0)) { plan.cancelled=true; if(result)*result=plan; return {}; }
    for(int pass=0;pass<4;++pass){
        bool cancelled=false;
        output=process(c,s,plan,[&](double p){const bool ok=!progress||progress((pass+p)/4);cancelled|=!ok;return ok;});
        if(cancelled){plan.cancelled=true;if(result)*result=plan;return {};}
        bool refine=false;
        for(size_t index=0;index<plan.regions.size();++index){auto& r=plan.regions[index];if(std::abs(r.appliedCents)<1e-9)continue;
            std::vector<double> shifts,errors; double confidence=0;
            const int verifyWindow = !r.discrete && r.uncertaintyCents < 0.4 && r.stableEnd-r.stableStart > 1.0 ? 16384 : 4096;
            const double halfWindow = verifyWindow/(2*c.sampleRate);
            const double begin=std::max(r.stableStart+halfWindow,r.start+0.12),end=std::min(r.stableEnd-halfWindow,r.end-0.08); if(end-begin<0.08)continue;
            for(int point=0;point<8;++point){const double time=begin+(end-begin)*(point+0.5)/8;const int at=int(time*c.sampleRate)-verifyWindow/2;if(at<0||at+verifyWindow>output.getNumSamples())continue;
                const auto [source,sc]=detectPitch(c.audio.getReadPointer(verificationChannel,at),verifyWindow,c.sampleRate);const auto [dest,dc]=detectPitch(output.getReadPointer(verificationChannel,at),verifyWindow,c.sampleRate);
                if(source>0&&dest>0&&sc>0.85&&dc>0.85){const double shift=100*(hzToMidi(dest)-hzToMidi(source));if(std::abs(shift-r.appliedCents)<20){shifts.push_back(shift);errors.push_back(r.appliedCents-shift);confidence+=dc;}}
            }
            if(shifts.size()>=4){r.achievedCents=median(shifts);r.periodicAchievedCents=r.achievedCents;r.verificationConfidence=confidence/shifts.size();double error=median(errors);
                if(verifyWindow>4096){const int count=std::min(int(c.sampleRate*1.2),int((r.stableEnd-r.stableStart)*c.sampleRate*0.8));const int at=int((r.stableStart+r.stableEnd)*0.5*c.sampleRate)-count/2;
                    if(at>=0&&at+count<=output.getNumSamples()){
                        const double source=stableSpectralCentre(c.audio.getReadPointer(verificationChannel,at),count,c.sampleRate,midiToHz(r.centre));
                        const double dest=source>0?stableSpectralCentre(output.getReadPointer(verificationChannel,at),count,c.sampleRate,source*std::exp2(r.appliedCents/1200)):0;
                        if(source>0&&dest>0){const double measured=1200*std::log2(dest/source);if(std::abs(measured-r.appliedCents)<20){r.spectralVerification=true;r.spectralAchievedCents=measured;r.achievedCents=(measured+r.periodicAchievedCents)/2;error=r.appliedCents-r.achievedCents;}}
                    }
                }
                if(pass<3&&std::abs(error)>0.2&&r.decision==RegionDecision::Translate){r.calibrationCents=juce::jlimit(-10.0,10.0,r.calibrationCents+error);refine=true;}
            }
        }
        if(!refine)break;
        // Scale calibration trims only; their zero vector is always feasible.
        // Stable-centre verification must not break the planned slope bounds.
        double scale=1;
        for(size_t i=1;i<plan.regions.size();++i) {
            const auto& a=plan.regions[i-1];const auto& b=plan.regions[i];
            if(b.start-a.end>=0.04)continue;
            const double allowed=400*std::max(0.001,b.stableStart-a.stableEnd)/1.875;
            const double d=b.appliedCents-a.appliedCents,e=b.calibrationCents-a.calibrationCents;
            if(e>1e-9)scale=std::min(scale,std::max(0.0,(allowed-d)/e));
            else if(e< -1e-9)scale=std::min(scale,std::max(0.0,(-allowed-d)/e));
        }
        if(scale<1)for(auto& r:plan.regions)r.calibrationCents*=scale;
        curve(plan,c.audio.getNumSamples()/c.sampleRate);
    }
    for (auto& r : plan.regions) {
        r.verificationAvailable = r.verificationConfidence > 0;
        r.verificationPassed = r.verificationAvailable && std::abs(r.periodicAchievedCents-r.appliedCents) <= s.precisionCents
            && (!r.spectralVerification || std::abs(r.spectralAchievedCents-r.appliedCents) <= s.precisionCents);
        if (std::abs(r.appliedCents)>1e-9 && !r.verificationPassed)
            r.reason += r.verificationAvailable ? "; Rendered precision not confirmed: audition/review required" : "; No reliable rendered centre verification: audition/review required";
    }
    if(result)*result=plan;if(progress)progress(1);return output;
}
}
