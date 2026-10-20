#pragma once
#include "PitchEngine.h"

namespace mp {
enum class RegionDecision { None, Translate, Reduced, Shared, Review };
struct CorrectionRegion {
    double start = 0, end = 0, stableStart = 0, stableEnd = 0;
    double centre = 0, uncertaintyCents = 0, confidence = 0;
    double requestedCents = 0, appliedCents = 0, calibrationCents = 0;
    double achievedCents = 0, verificationConfidence = 0;
    double periodicAchievedCents = 0, spectralAchievedCents = 0;
    bool spectralVerification = false, verificationAvailable = false, verificationPassed = false;
    bool discrete = false, overlap = false;
    RegionDecision decision = RegionDecision::None;
    juce::String reason;
};
struct CorrectionSegment {
    double start = 0, end = 0, fromCents = 0, toCents = 0;
    double at(double time) const;
};
struct CorrectionPlan {
    std::vector<CorrectionRegion> regions;
    std::vector<CorrectionSegment> segments;
    bool cancelled = false;
    double at(double seconds) const; // cents: source trajectory remains separate
    double wetAt(double seconds) const;
    bool changesAudio() const;
    juce::String summary() const;
    juce::var toJson() const;
};
// Offline only. No new dependency, neural model, or realtime allocation.
void analyseAdaptive(Clip&, const Progress& = {});
CorrectionPlan planCorrection(const Clip&, const Settings&);
juce::AudioBuffer<float> renderAdaptive(const Clip&, const Settings&, const Progress& = {}, CorrectionPlan* = nullptr);
}
