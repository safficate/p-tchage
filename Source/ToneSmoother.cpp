#include "ToneSmoother.h"
#include <algorithm>
#include <cmath>

namespace mp {
float ToneSmoother::Filter::tick(float x,int ch) noexcept {
    const double y=b0*x+z1[size_t(ch)];
    z1[size_t(ch)]=b1*x-a1*y+z2[size_t(ch)];
    z2[size_t(ch)]=b2*x-a2*y;
    return float(y);
}
void ToneSmoother::Filter::bandpass(double hz,double sr){
    const double w=2*juce::MathConstants<double>::pi*hz/sr, a=std::sin(w)/(2*4.0), d=1+a;
    b0=a/d;b1=0;b2=-a/d;a1=-2*std::cos(w)/d;a2=(1-a)/d;
}
void ToneSmoother::Filter::peak(double hz,double sr,double gain,bool ramp){
    const double w=2*juce::MathConstants<double>::pi*hz/sr, a=std::sin(w)/(2*4.0), A=std::pow(10.0,gain/40), d=1+a/A;
    const double nb0=(1+a*A)/d,nb1=-2*std::cos(w)/d,nb2=(1-a*A)/d,na1=nb1,na2=(1-a/A)/d;
    if(ramp){db0=(nb0-b0)/256;db1=(nb1-b1)/256;db2=(nb2-b2)/256;da1=(na1-a1)/256;da2=(na2-a2)/256;remaining=256;}
    else{b0=nb0;b1=nb1;b2=nb2;a1=na1;a2=na2;remaining=0;}
}
void ToneSmoother::prepare(double sr){
    rate=sr;
    const double upper=std::min(14000.0,sr*.42);
    for(int i=0;i<bands;++i){centres[size_t(i)]=float(100*std::pow(upper/100,double(i)/(bands-1)));detectors[size_t(i)].bandpass(centres[size_t(i)],sr);cuts[size_t(i)].peak(centres[size_t(i)],sr,0);}
    reset();
}
void ToneSmoother::reset(){
    for(auto& f:detectors){f.z1={};f.z2={};}
    for(auto& f:cuts){f.z1={};f.z2={};}
    gains.fill(0);energy.fill(0);for(auto& r:reductions)r=0;
    count=0;inputEnergy=previousRms=mix=0;
    for(int i=0;i<bands;++i)cuts[size_t(i)].peak(centres[size_t(i)],rate,0);
}
void ToneSmoother::update(const Controls& c,float f0){
    const double rms=std::sqrt(inputEnergy/std::max(1,count));
    const bool onset=previousRms>1e-5 && rms>previousRms*1.7;
    previousRms=rms;
    for(int b=0;b<bands;++b){
        const auto i=size_t(b);const double hz=centres[i];
        std::array<double,4> neighbours{};int n=0;
        for(int d:{-2,-1,1,2})if(b+d>=0&&b+d<bands)neighbours[size_t(n++)]=energy[size_t(b+d)];
        std::sort(neighbours.begin(),neighbours.begin()+n);
        const double floor=std::max(1e-12,neighbours[size_t(n/2)]);
        const double prominence=10*std::log10(std::max(1e-12,energy[i])/floor);
        bool harmonic=false;
        if(c.harmonicGuard && f0>20){const double k=std::round(hz/f0);harmonic=k>=1&&k<=12&&std::abs(hz/(k*f0)-1)<.09;}
        const bool active=c.enabled&&hz>=c.lowHz&&hz<=c.highHz&&c.ranges[size_t(b/4)]&&!harmonic&&rms>1e-5;
        // Automatic is an explicit spectral rule, not a learned quality model.
        const double threshold=c.automatic?std::max(5.0,double(c.threshold)):c.threshold;
        double target=active?std::min(double(c.depth),std::max(0.0,(prominence-threshold)*.65)):0;
        if(c.attackGuard&&onset)target*=.2;
        const double seconds=std::max(1.0,double(target>gains[i]?c.attackMs:c.releaseMs))*.001;
        gains[i]+=float((1-std::exp(-count/(rate*seconds)))*(target-gains[i]));
        if(gains[i]<.0001f)gains[i]=0;
        cuts[i].peak(hz,rate,-gains[i],true);reductions[i]=gains[i];
    }
    energy.fill(0);inputEnergy=0;count=0;
}
void ToneSmoother::process(juce::AudioBuffer<float>& buffer,const Controls& c,float f0){
    const int channels=std::min(2,buffer.getNumChannels());
    if(!c.enabled&&mix<1e-6){mix=0;return;}
    const double target=c.enabled?juce::jlimit(0.0,1.0,double(c.mix)):0;
    const double slew=1-std::exp(-1/(rate*.015));
    for(int s=0;s<buffer.getNumSamples();++s){
        mix+=(target-mix)*slew;
        double input=0;
        for(int ch=0;ch<channels;++ch){const float x=buffer.getSample(ch,s);input=std::max(input,double(x)*x);}
        inputEnergy+=input;
        for(int b=0;b<bands;++b){double e=0;for(int ch=0;ch<channels;++ch){const float v=detectors[size_t(b)].tick(buffer.getSample(ch,s),ch);e=std::max(e,double(v)*v);}energy[size_t(b)]+=e;}
        for(auto& f:cuts)f.advance();
        for(int ch=0;ch<channels;++ch){const float dry=buffer.getSample(ch,s);float wet=dry;for(auto& f:cuts)wet=f.tick(wet,ch);buffer.setSample(ch,s,std::isfinite(wet)?float(dry+mix*(wet-dry)):dry);}
        if(++count>=256)update(c,f0);
    }
}
}
