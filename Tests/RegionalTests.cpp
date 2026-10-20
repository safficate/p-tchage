#include "AdaptivePitch.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <cstring>
#include <numeric>

static int assertions=0;
static void check(bool value,const char* message){++assertions;if(!value)throw std::runtime_error(message);}
static double midiAt(int kind,double time){
    if(kind==1||kind==3){const double x=juce::jlimit(0.0,1.0,(time-1.0)/0.4);return 69.2+(70.85-69.2)*x;}
    if(kind==10)return time<1.25?69.05:68.75;
    if(kind==8)return 57.1;
    return 69.1+((kind==0)?0.22*std::sin(2*juce::MathConstants<double>::pi*5*time):0)+(kind==2?0.03*time:0);
}
static mp::Clip fixture(int kind,double rate){
    mp::Clip c;c.sampleRate=rate;c.audio.setSize(2,int(rate*3));c.audio.clear();c.name="synthetic-region-fixture-"+juce::String(kind);
    uint32_t rng=47;double phases[3]{};
    for(int i=0;i<c.audio.getNumSamples();++i){const double t=i/rate;double left=0,right=0;
        if(kind==4||kind==5||kind==6||kind==7){
            for(int event=0;event<3;++event){const double start=event*0.8,age=t-start;if(age<0)continue;
                const double keys[]={64.15,67.75,71.10};const double pitch=kind==5?57.15:keys[event];
                const double f=mp::midiToHz(pitch),decay=std::exp(-age/(kind==6?0.8:kind==7?1.2:0.25));
                for(int h=1;h<=6;++h){const double stiff=kind==7?std::sqrt(1+0.0005*h*h):1;const double amp=(h==1&&kind==5?0.12:1.0/h)*decay*std::min(1.0,age/0.001);
                    left+=amp*std::sin(2*juce::MathConstants<double>::pi*f*h*stiff*age);right+=amp*0.9*std::sin(2*juce::MathConstants<double>::pi*f*h*stiff*age+0.22);}
                if(kind==5){left+=0.28*std::exp(-age/0.12)*std::sin(2*juce::MathConstants<double>::pi*180*age);right+=0.2*std::exp(-age/0.12)*std::sin(2*juce::MathConstants<double>::pi*180*age);}
            }
        }else if(kind==2){
            for(int voice=0;voice<3;++voice){const double m=midiAt(kind,t)+(voice-1)*0.04+0.04*std::sin(2*juce::MathConstants<double>::pi*(4.5+voice*0.2)*t);phases[voice]+=2*juce::MathConstants<double>::pi*mp::midiToHz(m)/rate;left+=std::sin(phases[voice])/3;right+=std::sin(phases[voice]+voice*0.4)/3;}
        }else{
            const double m=kind==11?69:midiAt(kind,t);phases[0]+=2*juce::MathConstants<double>::pi*mp::midiToHz(m)/rate;
            const double p=phases[0];
            if(kind==8)left=0.7*std::sin(2*p)+0.3*std::sin(3*p)+0.15*std::sin(4*p);
            else if(kind==9){const double x=juce::jlimit(0.0,1.0,(t-0.6)/0.8);left=(1-x*0.98)*std::sin(p)+(0.15+0.85*x)*std::sin(2*p)+0.25*std::sin(3*p)+0.06*std::sin(5*p);}
            else left=(kind==3?0.2:1)*std::sin(p)+0.3*std::sin(2*p)+0.15*std::sin(3*p);
            rng=rng*1664525u+1013904223u;if(kind==3)left+=0.025*(double(rng)/4294967295.0*2-1);
            right=left;
            if(kind==10&&t>1.15&&t<1.35)left=right=0;
        }
        const double fade=std::min({1.0,t/0.015,(3-t)/0.02});c.audio.setSample(0,i,float(left*0.15*fade));c.audio.setSample(1,i,float(right*0.15*fade));
    }
    return c;
}
static double measuredShift(const mp::Clip& c,const juce::AudioBuffer<float>& output,double time){
    const int at=int(time*c.sampleRate)-2048;
    const auto [a,ac]=mp::detectPitch(c.audio.getReadPointer(0,at),4096,c.sampleRate);const auto [b,bc]=mp::detectPitch(output.getReadPointer(0,at),4096,c.sampleRate);
    check(a>0&&b>0&&ac>0.85&&bc>0.85,"Measured render must contain reliable source and output pitches");return 100*(mp::hzToMidi(b)-mp::hzToMidi(a));
}
static double independentFrequency(const juce::AudioBuffer<float>& audio,double rate) {
    double first=0,last=0;int crossings=0;bool armed=false; const double threshold=audio.getMagnitude(0,audio.getNumSamples())*0.15;
    for(int i=int(rate*0.8)+1;i<int(rate*2.3);++i) { if(audio.getSample(0,i)<-threshold) armed=true; if(armed && audio.getSample(0,i-1)<0 && audio.getSample(0,i)>=0) {
        const double a=audio.getSample(0,i-1),b=audio.getSample(0,i),position=i-1-a/(b-a);
        if(crossings++==0)first=position;last=position;armed=false;
    }}
    check(crossings>100,"Independent crossing measurement must observe enough cycles");return (crossings-1)*rate/(last-first);
}
static void exportCase(const juce::File& folder,const mp::Clip& c,const juce::AudioBuffer<float>& output,const mp::CorrectionPlan& plan){
    if(folder==juce::File{})return;folder.createDirectory();
    const auto base=juce::String(int(c.sampleRate))+"-"+c.name;
    auto save=[&](juce::String suffix,const juce::AudioBuffer<float>& audio){auto f=folder.getChildFile(base+suffix+".wav");f.deleteFile();check(mp::writeWav(f,audio,c.sampleRate),"Write reproducible audio evidence");};
    save("-original",c.audio);save("-corrected",output);
    juce::AudioBuffer<float> ab(2,c.audio.getNumSamples()*2);for(int ch=0;ch<2;++ch){ab.copyFrom(ch,0,c.audio,ch,0,c.audio.getNumSamples());ab.copyFrom(ch,c.audio.getNumSamples(),output,ch,0,output.getNumSamples());}save("-AB",ab);
    folder.getChildFile(base+"-plan.json").replaceWithText(juce::JSON::toString(plan.toJson(),true));
    juce::String csv="time,source_midi,confidence,uncertainty_cents,onset,agreement,planned_cents,output_midi,output_confidence\n";
    for(const auto& f:c.frames){const int at=int(f.time*c.sampleRate)-2048;std::pair<double,double> detected{};if(at>=0&&at+4096<=output.getNumSamples())detected=mp::detectPitch(output.getReadPointer(0,at),4096,c.sampleRate);
        csv+=juce::String(f.time,6)+","+juce::String(f.midi,6)+","+juce::String(f.confidence,5)+","+juce::String(f.uncertainty,5)+","+juce::String(f.onset,5)+","+juce::String(f.agreement,3)+","+juce::String(plan.at(f.time),6)+","+juce::String(detected.first>0?mp::hzToMidi(detected.first):0,6)+","+juce::String(detected.second,5)+"\n";
    }
    folder.getChildFile(base+"-analysis.csv").replaceWithText(csv);
}
static void plannerSafety(){
    auto c=fixture(0,48000);mp::Note a;a.start=0;a.end=1.4;a.stableStart=0.1;a.stableEnd=1.3;a.key=69;a.centre=69.2;a.confidence=.99;
    auto b=a;b.start=1.4;b.end=3;b.stableStart=1.5;b.stableEnd=2.8;b.key=71;b.centre=70.85;c.notes={a,b};c.adaptiveAnalysis=true;
    mp::Settings s;s.adaptive=true;s.matchReference=true;auto p=mp::planCorrection(c,s);
    check(std::abs(p.at(.8)+20)<.01&&std::abs(p.at(2.0)-15)<.01,"Regional centres require independent -20 and +15 cent displacement");
    double step=0;for(double t=0;t<3;t+=.001)step=std::max(step,std::abs(p.at(t+.001)-p.at(t)));check(step<=1,"Connected correction trajectory must not introduce a cent step at 1 ms");
    c.notes[0].stableEnd=1.399;c.notes[1].stableStart=1.401;p=mp::planCorrection(c,s);check(p.regions[0].decision==mp::RegionDecision::Reduced,"An impossible short transition must report reduced accuracy");
    c.notes[0].overlap=c.notes[1].overlap=true;c.notes[0].discrete=c.notes[1].discrete=true;p=mp::planCorrection(c,s);
    check(!p.changesAudio()&&p.regions[0].decision==mp::RegionDecision::Review,"Conflicting overlapping tails must be declined without pretending separation");
    {
        auto chain=c; auto x=a,y=b,z=b;
        x.start=0;x.end=1;x.stableStart=.1;x.stableEnd=.9;x.key=69;x.centre=69.2;x.overlap=false;
        y.start=1;y.end=2;y.stableStart=1.1;y.stableEnd=1.9;y.key=70;y.centre=70.2;y.overlap=true;
        z.start=2;z.end=3;z.stableStart=2.1;z.stableEnd=2.8;z.key=72;z.centre=72.25;z.overlap=false;
        chain.notes={x,y,z}; const auto group=mp::planCorrection(chain,s);
        check(!group.changesAudio() && group.regions.front().decision==mp::RegionDecision::Review,"A later overlap conflict must also decline the first event of the connected group");
        chain.notes[2].centre=72.2; const auto shared=mp::planCorrection(chain,s);
        check(shared.regions.front().decision==mp::RegionDecision::Shared && std::abs(shared.regions.front().appliedCents-shared.regions.back().appliedCents)<1e-9,"Compatible overlap must have one shared group displacement");
    }
    const auto original=mp::renderAdaptive(c,s);check(std::memcmp(original.getReadPointer(0),c.audio.getReadPointer(0),size_t(original.getNumSamples())*sizeof(float))==0,"Declined overlap must preserve original PCM bit-for-bit");
    c.notes[0].centre=69;c.notes[1].centre=70.8;c.notes[0].overlap=c.notes[1].overlap=false;p=mp::planCorrection(c,s);
    check(p.regions[0].appliedCents==0,"A correctly tuned neighbour must not be shifted to solve another region's transition");
    mp::CorrectionPlan cancelled;auto stopped=mp::renderAdaptive(c,s,[](double){return false;},&cancelled);check(stopped.getNumSamples()==0&&cancelled.cancelled,"Cancelled processing must not return publishable partial audio");
}
int main(int argc,char** argv){
    std::cout<<std::unitbuf;
    try{
        plannerSafety();const juce::File folder=argc>1?juce::File(argv[1]):juce::File{};
        const char* names[]={"violin-vibrato","violin-legato","ensemble-common-drift","ney-slide-noise","baglama-plucks","oud-body-resonance","guitar-overlap","piano-inharmonic-decay","missing-fundamental","changing-harmonics","microtonal-variants","already-tuned"};
        for(double rate:{44100.0,48000.0})for(int kind=0;kind<12;++kind){
            auto c=fixture(kind,rate);c.name=names[kind];mp::analyseAdaptive(c);
            mp::Settings s;s.adaptive=true;s.directShift=true;s.formants=false;for(auto& cents:s.cents)cents=10;
            if(kind==0)for(auto& cents:s.cents)cents=-10;
            if(kind==1||kind==3||kind==11){s.directShift=false;s.matchReference=true;for(auto& cents:s.cents)cents=0;}
            mp::CorrectionPlan plan;auto output=mp::renderAdaptive(c,s,{},&plan);
            check(output.getNumSamples()==c.audio.getNumSamples(),"Adaptive render must preserve exact duration");
            bool finite=true;double stereoDifference=0,energy=0;for(int ch=0;ch<2;++ch)for(int i=0;i<output.getNumSamples();++i)finite&=std::isfinite(output.getSample(ch,i));
            for(int i=0;i<output.getNumSamples();++i){const double l=output.getSample(0,i),r=output.getSample(1,i);energy+=l*l+r*r;stereoDifference+=(l-r)*(l-r);}
            check(finite&&output.getMagnitude(0,output.getNumSamples())<1,"No NaN/Inf or clipping in the rendered corpus");
            double maxStep=0;for(double t=0;t<3;t+=.001)maxStep=std::max(maxStep,std::abs(plan.at(t+.001)-plan.at(t)));
            check(maxStep<=1.01,"Planned corrections must remain continuous at 1 ms sampling");
            if(kind==11)check(std::memcmp(output.getReadPointer(0),c.audio.getReadPointer(0),size_t(output.getNumSamples())*sizeof(float))==0,"Already-tuned material must remain bit-exact under minimum intervention");
            if(kind==0){
                check(plan.changesAudio(),"Reliable vibrato event must receive its requested regional translation");
                std::vector<double> a,b;for(double t=.6;t<2.4;t+=.02){const int at=int(t*rate)-2048;auto x=mp::detectPitch(c.audio.getReadPointer(0,at),4096,rate);auto y=mp::detectPitch(output.getReadPointer(0,at),4096,rate);if(x.first>0&&y.first>0){a.push_back(100*mp::hzToMidi(x.first));b.push_back(100*mp::hzToMidi(y.first));}}
                const double ma=std::accumulate(a.begin(),a.end(),0.0)/a.size(),mb=std::accumulate(b.begin(),b.end(),0.0)/b.size();double xx=0,yy=0,xy=0,error=0;for(size_t i=0;i<a.size();++i){const double x=a[i]-ma,y=b[i]-mb;xx+=x*x;yy+=y*y;xy+=x*y;error+=(x-y)*(x-y);}const double correlation=xy/std::sqrt(xx*yy),rms=std::sqrt(error/a.size());
                std::cout<<"VIBRATO rate="<<rate<<" shift="<<mb-ma<<" correlation="<<correlation<<" contour_rms_cents="<<rms<<'\n';
                check(std::abs(mb-ma+10)<1,"Vibrato centre translation must meet requested -10 cents");check(correlation>=.97&&rms<=3,"Constant-offset render must preserve natural vibrato contour");
            }
            if(kind==1){check(c.notes.size()>=2,"Connected glide must retain independent stable plateau anchors");check(std::abs(measuredShift(c,output,.6)+20)<1&&std::abs(measuredShift(c,output,2.1)-15)<1,"Connected phrase plateau centres must reach requested targets");}
            if(kind==2)check(energy>1&&stereoDifference/energy>.001,"Ensemble texture must not collapse to mono");
            if(kind==4){bool discrete=false;for(const auto& r:plan.regions)if(r.discrete){discrete=true;const int start=int(std::ceil(r.start*rate)),end=std::min(output.getNumSamples(),int((r.start+.015)*rate));for(int i=start;i<end;++i)check(output.getSample(0,i)==c.audio.getSample(0,i),"Protected transient samples must be identical to original");}check(discrete,"Plucked decay must be recognized as a discrete event");
                // Independent of detected regions: attacks were synthesized at
                // these known sample positions, not at the analysis boundaries.
                for(double attack:{0.0,0.8,1.6})for(int ch=0;ch<2;++ch)
                    for(int i=int(std::ceil(attack*rate));i<int((attack+.015)*rate);++i)
                        check(output.getSample(ch,i)==c.audio.getSample(ch,i),"Actual known pick attack must remain sample-identical for its first 15 ms");}
            if(kind==8||kind==9){int wrong=0,count=0;for(const auto& f:c.frames)if(f.midi>0&&f.time>.3&&f.time<2.7){++count;wrong+=std::abs(f.midi-midiAt(kind,f.time))>0.5;}std::cout<<"FUNDAMENTAL kind="<<kind<<" voiced="<<count<<" wrong="<<wrong<<'\n';check(count>50&&double(wrong)/count<.05,"Harmonic dominance must not cause sustained octave relabelling");}
            if(kind==10)check(c.notes.size()>=2,"Distinct microtonal variants separated by silence must not be pooled into one centre");
            std::cout<<names[kind]<<" rate="<<rate<<" notes="<<c.notes.size()<<" correction_step_1ms="<<maxStep<<" "<<plan.summary()<<'\n';
            exportCase(folder,c,output,plan);
        }
        for(double rate:{44100.0,48000.0})for(double amount:{-20.0,-5.0,-3.0,3.0,5.0,20.0}){auto c=fixture(11,rate);mp::analyseAdaptive(c);mp::Settings s;s.adaptive=true;s.directShift=true;s.cents[9]=amount;mp::CorrectionPlan finePlan;auto output=mp::renderAdaptive(c,s,{},&finePlan);c.name="fine-"+juce::String(amount,0)+"c";exportCase(folder,c,output,finePlan);const double actual=measuredShift(c,output,1.5);double total=0;int valid=0;
            for(double t=.8;t<2.3;t+=.04){total+=measuredShift(c,output,t);++valid;}
            std::cout<<"FINE rate="<<rate<<" requested="<<amount<<" actual="<<actual<<" regional_mean="<<total/valid<<'\n';
            if(std::abs(actual-amount)>=1){auto f=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("pitchmap3-fine-diagnostic.wav");f.deleteFile();mp::writeWav(f,output,rate);}
check(std::abs(actual-amount)<1,"Stable regional fine-cent translation must be within one cent");const double independent=1200*std::log2(independentFrequency(output,rate)/440);std::cout<<"INDEPENDENT requested="<<amount<<" actual="<<independent<<'\n'; if(std::abs(independent-amount)>=1){auto f=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("pitchmap3-fine-diagnostic.wav");f.deleteFile();mp::writeWav(f,output,rate);std::cout<<"DIAGNOSTIC "<<f.getFullPathName()<<'\n';}check(std::abs(independent-amount)<1,"Independent zero-crossing frequency must confirm the actual requested cents");}
        std::cout<<assertions<<" regional rendered-audio assertions passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL after "<<assertions<<" checks: "<<e.what()<<'\n';return 1;}
}
