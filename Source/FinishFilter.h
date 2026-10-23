#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
namespace mp {
// Optional, explicitly coloured finishing filter, separate from pitch and
// resonance correction. A brightness detector may reduce its wet proportion.
class FinishFilter {
public:
    struct Controls{bool enabled=false,automatic=true,region=false;double start=0,end=0;float cutoff=12000,amount=.5f;};
    void prepare(double sr){rate=sr;reset();}
    void reset(){z1={};z2={};hf={};previous={};blend=inputEnv=highEnv=0;activity=0;}
    void process(juce::AudioBuffer<float>&,const Controls&,double startTime,float f0);
    float effective()const{return activity.load();}
private:
    double rate=48000,blend=0,inputEnv=0,highEnv=0;
    std::array<double,2> z1{},z2{},hf{},previous{};
    std::atomic<float> activity{0};
};
}
