#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace mp {
// Linked-stereo, cut-only dynamic EQ. All storage and filter centres are
// prepared outside the callback. It does not classify instruments or voices.
class ToneSmoother {
public:
    static constexpr int bands = 20;
    struct Controls {
        bool enabled=false, automatic=true, harmonicGuard=true, attackGuard=true;
        float depth=4, threshold=7, attackMs=25, releaseMs=180;
        float lowHz=180, highHz=9000, mix=1;
        std::array<bool,5> ranges{true,true,true,true,true};
    };
    void prepare(double sampleRate);
    void reset();
    void process(juce::AudioBuffer<float>&,const Controls&,float fundamentalHz=0);
    float reduction(int band) const {return reductions[size_t(band)].load(std::memory_order_relaxed);}
    float frequency(int band)const{return centres[size_t(band)];}
private:
    struct Filter {
        double b0=1,b1=0,b2=0,a1=0,a2=0;
        double db0=0,db1=0,db2=0,da1=0,da2=0;int remaining=0;
        std::array<double,2> z1{},z2{};
        float tick(float x,int ch) noexcept;
        void bandpass(double hz,double rate);
        void peak(double hz,double rate,double gainDb,bool ramp=false);
        void advance(){if(remaining>0){b0+=db0;b1+=db1;b2+=db2;a1+=da1;a2+=da2;--remaining;}}
    };
    std::array<Filter,bands> detectors{},cuts{};
    std::array<float,bands> centres{},gains{};
    std::array<double,bands> energy{};
    std::array<std::atomic<float>,bands> reductions{};
    double rate=48000,inputEnergy=0,previousRms=0,mix=0;
    int count=0;
    void update(const Controls&,float fundamentalHz);
};
}
