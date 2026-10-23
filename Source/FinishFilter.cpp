#include "FinishFilter.h"
#include <cmath>
namespace mp {
void FinishFilter::process(juce::AudioBuffer<float>& b,const Controls& c,double time,float f0){
    if(!c.enabled&&blend<1e-6){blend=0;activity=0;return;}
    const double hz=juce::jlimit(300.0,rate*.45,std::max(double(c.cutoff),double(f0)*3));
    const double w=2*juce::MathConstants<double>::pi*hz/rate,cs=std::cos(w),a=std::sin(w)/(2*.70710678),d=1+a;
    const double b0=(1-cs)*.5/d,b1=(1-cs)/d,b2=b0,a1=-2*cs/d,a2=(1-a)/d;
    const double hp=std::exp(-2*juce::MathConstants<double>::pi*4000/rate),slew=1-std::exp(-1/(rate*.02));
    for(int i=0;i<b.getNumSamples();++i){double inputEnergy=0,highEnergy=0;
        for(int ch=0;ch<std::min(2,b.getNumChannels());++ch){const double x=b.getSample(ch,i);hf[size_t(ch)]=hp*(hf[size_t(ch)]+x-previous[size_t(ch)]);previous[size_t(ch)]=x;inputEnergy=std::max(inputEnergy,x*x);highEnergy=std::max(highEnergy,hf[size_t(ch)]*hf[size_t(ch)]);}
        const double envelope=1-std::exp(-1/(rate*.01));inputEnv+=envelope*(inputEnergy-inputEnv);highEnv+=envelope*(highEnergy-highEnv);
        const bool within=!c.region||(time+i/rate>=c.start&&time+i/rate<c.end);
        const double automatic=c.automatic?juce::jlimit(0.0,1.0,(highEnv/std::max(1e-8,inputEnv)-.1)*2):1;
        const double target=c.enabled&&within?c.amount*automatic:0;blend+=(target-blend)*slew;
        for(int ch=0;ch<std::min(2,b.getNumChannels());++ch){const double x=b.getSample(ch,i),y=b0*x+z1[size_t(ch)];z1[size_t(ch)]=b1*x-a1*y+z2[size_t(ch)];z2[size_t(ch)]=b2*x-a2*y;b.setSample(ch,i,float(x+blend*(y-x)));}
    }
    activity=float(blend);
}
}
