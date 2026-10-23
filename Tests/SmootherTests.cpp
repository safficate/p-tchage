#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>
#include <complex>
#include <cstdlib>
#include <new>

static thread_local bool callback=false;
static thread_local int allocations=0;
void* operator new(std::size_t n){if(callback)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
static int assertions=0;
static void check(bool ok,const char* what){++assertions;if(!ok)throw std::runtime_error(what);}
static double magnitude(const juce::AudioBuffer<float>& b,double rate,double hz,double start=1.4,double duration=.5){
    const int count=int(rate*duration),offset=int(rate*start);std::complex<double> sum{};
    for(int i=0;i<count;++i){const double window=.5-.5*std::cos(2*juce::MathConstants<double>::pi*i/(count-1));sum+=double(b.getSample(0,offset+i))*window*std::polar(1.0,-2*juce::MathConstants<double>::pi*hz*i/rate);}
    return std::abs(sum)*4/count;
}
// Independent spectral fitting around a known test tone, not the DSP detector.
static double measure(const juce::AudioBuffer<float>& b,double rate,double expected){
    double low=mp::midiToHz(expected-.25),high=mp::midiToHz(expected+.25);
    for(int i=0;i<32;++i){const double a=low+(high-low)*.38196601125,c=low+(high-low)*.61803398875;if(magnitude(b,rate,a)>magnitude(b,rate,c))high=c;else low=a;}
    const double hz=(low+high)*.5;check(magnitude(b,rate,hz)>.08,"Expected output fundamental must actually exist");return mp::hzToMidi(hz);
}
struct Head:juce::AudioPlayHead{int64_t sample=0;juce::Optional<PositionInfo> getPosition()const override{PositionInfo i;i.setIsPlaying(true);i.setTimeInSamples(sample);return i;}};
static juce::AudioBuffer<float> run(PitchProcessor& p,double rate,double source,Head* head=nullptr){
    const int length=int(rate*2.2);juce::AudioBuffer<float> output(2,length),block(2,257);juce::MidiBuffer midi;
    const int sizes[]={1,31,64,257,128,97};int n=0;
    for(int at=0;at<length;++n){const int count=std::min(sizes[n%6],length-at);block.setSize(2,count,false,false,true);
        for(int i=0;i<count;++i){const auto x=float(.2*std::sin(2*juce::MathConstants<double>::pi*mp::midiToHz(source)*(at+i)/rate));block.setSample(0,i,x);block.setSample(1,i,x);}
        if(head)head->sample=at;callback=true;p.processBlock(block,midi);callback=false;for(int ch=0;ch<2;++ch)output.copyFrom(ch,at,block,ch,0,count);at+=count;
    }return output;
}
static void memory(){
    mp::AudioMemory m;m.prepare(48000);juce::AudioBuffer<float> b(2,257);for(int ch=0;ch<2;++ch)for(int i=0;i<257;++i)b.setSample(ch,i,float(i*.001));
    callback=true;m.push(b,480000);callback=false;m.drain();juce::String error;const auto source=m.assemble(error);check(source&&source->audio.getNumSamples()==257,"DAW audio memory preserves arbitrary block sizes and absolute song time");check(source->anchorSeconds==10,"DAW memory preserves the clip timeline anchor");
    callback=true;m.push(b,480000);callback=false;m.drain();const auto again=m.assemble(error);check(again&&again->audio.getNumSamples()==257,"Repeated playback must not append a second PCM copy");
    callback=true;m.push(b,480600);callback=false;m.drain();check(!m.assemble(error),"Unheard gaps must not be silently rendered as source silence");
}
static void audition(double rate){
    mp::Audition tone;std::array<float,12> cents{};juce::AudioBuffer<float> block(2,128);
    tone.frequency(mp::midiToHz(60.15),60);tone.releaseTone();
    int elapsed=0;double energy=0;
    while(tone.active()&&elapsed<int(rate)){
        block.clear();callback=true;tone.process(block,rate,cents,440);callback=false;
        energy+=block.getRMSLevel(0,0,128);elapsed+=128;
    }
    check(energy>1,"Short tone button must emit actual audible audio");
    check(elapsed>=int(rate*.5)&&elapsed<int(rate*.5)+256,"Short click must sound for half a second");
    tone.frequency(mp::midiToHz(60.15),60);
    for(elapsed=0;elapsed<int(rate*.8);elapsed+=128){callback=true;tone.process(block,rate,cents,440);callback=false;}
    check(tone.active(),"Held tone button must remain audible beyond half a second");
    tone.releaseTone();
    for(int i=0;i<20;++i){callback=true;tone.process(block,rate,cents,440);callback=false;}
    check(!tone.active(),"Releasing a held tone must end it with a short fade");
    PitchProcessor p;p.prepareToPlay(rate,128);p.auditionCentre(1,false);block.clear();juce::MidiBuffer midi;
    callback=true;p.processBlock(block,midi);callback=false;
    check(block.getMagnitude(0,128)==0,"An unread source button must not fabricate a measured tone");
    p.setMeasuredCentre(0,10);p.auditionCentre(0,false);block.clear();p.processBlock(block,midi);
    check(block.getMagnitude(0,128)>.05,"Source tone button must play a manually defined source centre");
    auto frequency=[&](bool target){
        p.auditionCentre(0,target);double first=-1,last=0;int crossings=0;float previous=0;
        for(int at=0;at<int(rate*.65);at+=128){block.clear();callback=true;p.processBlock(block,midi);callback=false;
            for(int i=0;i<128;++i){const float x=block.getSample(0,i);if(at+i>rate*.05&&previous<=0&&x>0){const double position=at+i-1-previous/(x-previous);if(first<0)first=position;last=position;++crossings;}previous=x;}
        }
        check(crossings>100,"Audition must contain a measurable steady tone");return mp::hzToMidi((crossings-1)*rate/(last-first));
    };
    check(std::abs((frequency(false)-60.1)*100)<.1,"Source audition must sound its actual +10 cent centre");
    p.setTargetCentre(0,5);p.setParameter("globalCents",20);
    check(std::abs((frequency(true)-60.25)*100)<.1,"Target audition must include the target and global cents exactly once");
    p.auditionCentre(1,false);block.clear();p.processBlock(block,midi);
    check(block.getMagnitude(0,128)==0,"Clicking an unread note must stop the previous audition tone");
    p.stopAudition();
}
static void reference(){
    PitchProcessor p,q;q.setMeasuredCentre(0,5);q.selectReferenceInstance({});p.setMeasuredCentre(0,-15);const auto refs=p.referenceInstances();check(refs.size()==1,"Two plugin instances expose one selectable reference");p.selectReferenceInstance(refs.front().id);p.alignReferenceCentre();check(p.parameters.getRawParameterValue("globalCents")->load()==20,"Reference alignment uses the selected measured/manual centre");check(std::abs(p.targetForSource(0,59.85)-60.05)<1e-6,"Global reference alignment must not count the same 20 cents twice");p.setParameter("globalCents",30);check(std::abs(p.targetForSource(0,59.85)-60.15)<1e-6,"Manual global adjustment remains effective after reference alignment");
}
static void workspace(){
    mp::LiveWorkspace w;
    for(int i=0;i<100;++i)w.push({i*.025,60.1f,60.1f,0,0,1,0});w.drain();const auto first=w.snapshot();
    for(int i=0;i<100;++i)w.push({i*.025,60.1f,60.1f,0,0,1,0});w.drain();const auto replay=w.snapshot();
    check(first.frames.size()==replay.frames.size(),"Replay must not duplicate timeline frames");
    check(first.source[0].observations==replay.source[0].observations,"Replay must not increase note frequency counts");
    w.push({3,62.05f,62.05f,0,0,1,0});w.drain();check(w.snapshot().source[2].known,"One reliable rare observation must remain visible");
    w.edit({1,2,7.5});double value=0;check(w.correctionAt(1.5,value)&&value==7.5,"Local edit must reach realtime controls");
    mp::LiveWorkspace restored;restored.restore(w.save());check(restored.snapshot().frames.size()==101,"Measured history survives state round trip");check(restored.correctionAt(1.5,value)&&value==7.5,"Local edit survives state round trip");
    mp::LiveWorkspace reference;
    for(int i=0;i<100;++i)reference.push({i*.025,60.1f,60.1f,62.05f,62.05f,1,1});reference.drain();
    for(int i=0;i<100;++i)reference.push({i*.025,60.1f,60.1f,0,0,1,0});reference.drain();
    check(reference.snapshot().reference[2].observations==100&&reference.referenceCents(2)==5,"Source replay without reference must retain its learned map without duplicate counts");
    for(int i=0;i<100;++i)reference.push({i*.025,60.1f,60.1f,62.08f,62.08f,1,1});reference.drain();
    check(reference.snapshot().reference[2].observations==100&&reference.referenceCents(2)==8,"Rereading reference at the same time must replace its measurements");
    mp::Clip source;for(int i=0;i<100;++i)source.frames.push_back({i*.025,60.1,1,.1});reference.analyseFile(source);
    check(reference.snapshot().reference[2].observations==100&&reference.referenceCents(2)==8,"Source-only offline analysis must retain learned reference centres");
    check(reference.snapshot().frames[20].reference>0,"Source-only graph rebuild must retain the recorded reference curve");
    mp::LiveWorkspace savedReference;savedReference.restore(reference.save());savedReference.push({.5,60.1f,60.1f,0,0,1,0});savedReference.drain();
    check(savedReference.snapshot().reference[2].observations==100&&savedReference.referenceCents(2)==8,"Learned reference must survive state restore and later source playback");
    savedReference.clear(true);check(!savedReference.referenceKnown(2)&&savedReference.sourceKnown(0),"Clearing only reference must leave source learning intact");
}
static void tone(double rate){
    mp::ToneSmoother smoother;smoother.prepare(rate);mp::ToneSmoother::Controls c;c.enabled=true;c.automatic=false;c.threshold=2;c.depth=9;c.harmonicGuard=true;c.attackGuard=false;
    const double hz=smoother.frequency(13);juce::AudioBuffer<float> out(2,int(rate*2.2)),block(2,128);
    for(int at=0;at<out.getNumSamples();at+=128){const int n=std::min(128,out.getNumSamples()-at);block.setSize(2,n,false,false,true);for(int i=0;i<n;++i){const float x=float(.1*std::sin(2*juce::MathConstants<double>::pi*hz*(at+i)/rate)+.1*std::sin(2*juce::MathConstants<double>::pi*220*(at+i)/rate));block.setSample(0,i,x);block.setSample(1,i,-x);}
        callback=true;smoother.process(block,c,220);callback=false;for(int ch=0;ch<2;++ch)out.copyFrom(ch,at,block,ch,0,n);
    }
    const double amplitude=magnitude(out,rate,hz);std::cout<<"TONE "<<rate<<" resonance="<<hz<<" amplitude="<<amplitude<<" cut="<<smoother.reduction(13)<<'\n';
    check(amplitude<.085,"Enabled resonance smoother must reduce the real resonant sine amplitude");
    check(magnitude(out,rate,220)>.085,"Resonance processing must retain the separate low fundamental");
    bool finite=true;float error=0;for(int i=0;i<out.getNumSamples();++i){finite&=std::isfinite(out.getSample(0,i));error=std::max(error,std::abs(out.getSample(0,i)+out.getSample(1,i)));}
    check(finite,"Tone output finite");
    check(error<1e-6f,"Linked resonance smoother must retain inverted stereo relationship");
    smoother.reset();c.enabled=false;block.setSize(2,128);for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)block.setSample(ch,i,float(i*.001));auto dry=block;smoother.process(block,c);bool exact=true;for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)exact&=block.getSample(ch,i)==dry.getSample(ch,i);check(exact,"Disabled tone module must be bit exact");
}
static void finish(double rate){
    mp::FinishFilter f;f.prepare(rate);mp::FinishFilter::Controls c;c.enabled=true;c.automatic=false;c.region=true;c.start=.7;c.end=1.3;c.cutoff=2000;c.amount=1;
    juce::AudioBuffer<float> out(2,int(rate*2.2)),b(2,257);const double hz=6000;
    for(int at=0;at<out.getNumSamples();at+=257){const int n=std::min(257,out.getNumSamples()-at);b.setSize(2,n,false,false,true);for(int ch=0;ch<2;++ch)for(int i=0;i<n;++i)b.setSample(ch,i,float(.2*std::sin(2*juce::MathConstants<double>::pi*hz*(at+i)/rate)));callback=true;f.process(b,c,at/rate,220);callback=false;for(int ch=0;ch<2;++ch)out.copyFrom(ch,at,b,ch,0,n);}
    check(magnitude(out,rate,hz,.9,.2)<.04,"Repair filter must reduce high-frequency energy inside selected interval");
    check(std::abs(magnitude(out,rate,hz,1.8,.2)-.2)<.0001,"Repair interval must release to original signal outside selection");
}

static void realRecording(const juce::File& file,const juce::File& output){
    juce::String error;const auto source=mp::loadAudio(file,error);check(bool(source),"Real violin recording must load");
    output.createDirectory();const double rate=source->sampleRate;mp::Clip input=*source;mp::analyse(input);
    for(double shift:{-20.,5.,20.}){
        PitchProcessor p;p.enterLiveWorkspace();p.prepareToPlay(rate,257);p.setParameter("globalCents",float(shift));p.setParameter("formants",0);
        const int length=source->audio.getNumSamples(),latency=p.getLatencySamples();juce::AudioBuffer<float> all(2,length+latency),block(2,257);juce::MidiBuffer midi;
        for(int at=0;at<all.getNumSamples();at+=257){const int n=std::min(257,all.getNumSamples()-at);block.setSize(2,n,false,false,true);block.clear();for(int ch=0;ch<2;++ch)if(at<length)block.copyFrom(ch,0,source->audio,std::min(ch,source->audio.getNumChannels()-1),at,std::min(n,length-at));callback=true;p.processBlock(block,midi);callback=false;for(int ch=0;ch<2;++ch)all.copyFrom(ch,at,block,ch,0,n);}
        if(std::getenv("PITCHMAP_PREPARED_RECORDING")){
            p.markComplete();const auto deadline=juce::Time::getMillisecondCounterHiRes()+30000;while(p.busy&&juce::Time::getMillisecondCounterHiRes()<deadline)juce::Thread::sleep(5);
            check(!p.busy&&p.hasRender(),"Real recording must produce a complete prepared render");Head head;p.setPlayHead(&head);p.reset();all.clear();
            for(int at=0;at<all.getNumSamples();at+=257){const int n=std::min(257,all.getNumSamples()-at);block.setSize(2,n,false,false,true);block.clear();for(int ch=0;ch<2;++ch)if(at<length)block.copyFrom(ch,0,source->audio,std::min(ch,source->audio.getNumChannels()-1),at,std::min(n,length-at));head.sample=at;callback=true;p.processBlock(block,midi);callback=false;for(int ch=0;ch<2;++ch)all.copyFrom(ch,at,block,ch,0,n);}
            check(p.preparedPlayback(),"Real prepared audio must actually be heard through the host callback");p.setPlayHead(nullptr);
        }
        mp::Clip result;result.sampleRate=rate;result.audio.setSize(2,length);for(int ch=0;ch<2;++ch)result.audio.copyFrom(ch,0,all,ch,latency,length);mp::analyse(result);
        std::vector<double> deltas;double sumX=0,sumY=0,sumXX=0,sumYY=0,sumXY=0;int n=0;
        for(size_t i=0;i<std::min(input.frames.size(),result.frames.size());++i){const auto& a=input.frames[i];const auto& b=result.frames[i];if(a.time<.35||a.time>length/rate-.15||a.midi<=0||b.midi<=0||a.confidence<.95||b.confidence<.95)continue;const double x=a.midi*100,y=b.midi*100;deltas.push_back(y-x);sumX+=x;sumY+=y;sumXX+=x*x;sumYY+=y*y;sumXY+=x*y;++n;}
        std::cout<<"MEASURABLE "<<file.getFileName()<<" frames="<<n<<"\n";check(n>30,"Real recording must retain enough measurable voiced frames");std::sort(deltas.begin(),deltas.end());const double median=deltas[deltas.size()/2];const double denominator=std::sqrt(std::max(1e-12,(n*sumXX-sumX*sumX)*(n*sumYY-sumY*sumY)));const double correlation=(n*sumXY-sumX*sumY)/denominator;
        std::cout<<"REAL "<<file.getFileName()<<" prepared="<<(std::getenv("PITCHMAP_PREPARED_RECORDING")?1:0)<<" shift="<<shift<<" median="<<median<<" contour_correlation="<<correlation<<" voiced_frames="<<n<<'\n';
        check(std::abs(median-shift)<3,"Real violin median shift must follow entered cents");check(correlation>.8,"Real violin expressive contour must remain correlated");
        const auto wav=output.getChildFile("violin-shift-"+juce::String(shift,0)+".wav");wav.deleteFile();check(mp::writeWav(wav,result.audio,rate),"Export actual processed violin for user listening");
    }
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI gui;std::cout<<std::unitbuf;
    try{memory();workspace();reference();if(std::getenv("PITCHMAP_REAL_ONLY")&&argc==3){realRecording(juce::File(juce::String(argv[1])),juce::File(juce::String(argv[2])));std::cout<<assertions<<" real-recording assertions passed\n";return 0;}for(double rate:{44100.,48000.}){audition(rate);
        for(int key:{38,50,62,74,86})for(double detune:{.12,-.07}){
            PitchProcessor p;p.enterLiveWorkspace();p.prepareToPlay(rate,257);p.setParameter("formants",0);p.setTargetCentre(2,5);auto out=run(p,rate,key+detune);const double measured=(measure(out,rate,key+.05)-key)*100;
            std::cout<<"TARGET "<<rate<<" key="<<key<<" source="<<detune*100<<" target=5 measured="<<measured<<'\n';check(std::abs(measured-5)<1,"Same note target must reach all tested octaves within one cent");
        }
        PitchProcessor p;p.enterLiveWorkspace();p.prepareToPlay(rate,257);p.setParameter("formants",0);p.setTargetCentre(9,5);p.setParameter("globalCents",20);
        check(std::abs((measure(run(p,rate,69.1),rate,69.25)-69)*100-25)<1,"Global amount must be added once to the entered target");
        p.reset();p.setParameter("globalCents",0);p.setParameter("pitchAmount",.5f);
        check(std::abs((measure(run(p,rate,69.1),rate,69.075)-69)*100-7.5)<1,"50 percent correction must reach halfway from source to target");
        p.reset();p.setParameter("pitchAmount",1);p.setParameter("humanOn",1);p.setParameter("humanDepth",2);auto human=run(p,rate,69.1);check(std::abs((measure(human,rate,69.05)-69)*100-5)<3,"Optional human deviation must remain bounded in cents");
        p.setParameter("uiScale",175);p.setParameter("toneOn",1);p.setParameter("finishOn",1);juce::MemoryBlock state;p.getStateInformation(state);PitchProcessor restored;restored.setStateInformation(state.getData(),int(state.getSize()));check(restored.parameters.getRawParameterValue("uiScale")->load()==175,"UI scale survives host state");check(restored.parameters.getRawParameterValue("toneOn")->load()==1&&restored.parameters.getRawParameterValue("humanOn")->load()==1,"Module bypass settings survive host state");
        p.reset();p.setParameter("wet",0);auto dry=run(p,rate,69.1);bool exact=true;for(int i=p.getLatencySamples();i<dry.getNumSamples();++i){const auto expected=float(.2*std::sin(2*juce::MathConstants<double>::pi*mp::midiToHz(69.1)*(i-p.getLatencySamples())/rate));exact&=dry.getSample(0,i)==expected;}check(exact,"Global dry must bypass pitch, resonance and repair modules exactly");
        tone(rate);finish(rate);
        PitchProcessor prepared;prepared.prepareToPlay(rate,257);prepared.setParameter("globalCents",5);prepared.setParameter("formants",0);run(prepared,rate,69.1);prepared.markComplete();
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+30000;while(prepared.busy&&juce::Time::getMillisecondCounterHiRes()<deadline)juce::Thread::sleep(5);
        std::cout<<"PREPARED "<<rate<<" status="<<prepared.status()<<" busy="<<prepared.busy.load()<<" render="<<prepared.hasRender()<<'\n';
        check(!prepared.busy&&prepared.hasRender(),"Complete-performance action must prepare actual audio, not just a flag");
        Head head;prepared.setPlayHead(&head);prepared.reset();const auto rendered=run(prepared,rate,69.1,&head);check(prepared.preparedPlayback(),"Prepared region render must actually reach the DAW callback");
        check(std::abs((measure(rendered,rate,69.15)-69)*100-15)<1,"Prepared playback must retain the entered global cent target");prepared.setPlayHead(nullptr);
        juce::MemoryBlock completeState;prepared.getStateInformation(completeState);
        prepared.setStateInformation(completeState.getData(),int(completeState.getSize()));
        const auto restoreDeadline=juce::Time::getMillisecondCounterHiRes()+30000;while(prepared.busy&&juce::Time::getMillisecondCounterHiRes()<restoreDeadline)juce::Thread::sleep(5);
        check(!prepared.busy&&prepared.hasRender(),"Restoring identical complete state into an existing instance must rebuild real audio");
        prepared.prepareToPlay(rate,257);
        const auto prepareDeadline=juce::Time::getMillisecondCounterHiRes()+30000;while(prepared.busy&&juce::Time::getMillisecondCounterHiRes()<prepareDeadline)juce::Thread::sleep(5);
        check(!prepared.busy&&prepared.hasRender(),"Host prepare after state restore must not discard the prepared performance");
        prepared.setPlayHead(&head);prepared.reset();const auto recalled=run(prepared,rate,69.1,&head);
        check(prepared.preparedPlayback(),"Recalled complete state must actually play its rebuilt regional audio");
        check(std::abs((measure(recalled,rate,69.15)-69)*100-15)<1,"Recalled prepared audio must preserve the saved cent correction");prepared.setPlayHead(nullptr);
        prepared.setParameter("glideMs",120);prepared.markComplete();prepared.setPlayHead(&head);prepared.reset();run(prepared,rate,69.1,&head);
        check(!prepared.preparedPlayback(),"Manual live glide setting must not be hidden by a cache rendered with different behavior");prepared.setPlayHead(nullptr);
    }if(argc==3)realRecording(juce::File(juce::String(argv[1])),juce::File(juce::String(argv[2])));check(allocations==0,"New audio processing must not allocate C++ storage in callback");std::cout<<assertions<<" smoother/workspace assertions passed; callback allocations="<<allocations<<'\n';return 0;
    }catch(const std::exception& e){callback=false;std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
