#include "LiveWorkspace.h"
#include <algorithm>
#include <cmath>

namespace mp {
void LiveWorkspace::Distribution::add(double midi,double confidence) {
    if(!std::isfinite(midi)||midi<12||midi>120||confidence<.8)return;
    if(noteClass<0)noteClass=(int(std::lround(midi))%12+12)%12;
    const double cents=(midi-std::round(midi))*100;
    const int bin=juce::jlimit(0,200,int(std::lround(cents+100)));
    const double w=juce::jlimit(.1,1.0,confidence);
    if(weight>4096){for(auto& b:bins)b*=.5;weight*=.5;midiSum*=.5;confidenceSum*=.5;}
    bins[size_t(bin)]+=w;weight+=w;midiSum+=midi*w;confidenceSum+=confidence*w;++observations;
}
TuningCentre LiveWorkspace::Distribution::centre() const {
    TuningCentre c;if(weight<.8)return c;
    const auto quantile=[&](double q){double sum=0;for(size_t i=0;i<bins.size();++i){sum+=bins[i];if(sum>=weight*q)return double(i)-100;}return 0.0;};
    c.known=true;c.cents=quantile(.5);c.spread=quantile(.9)-quantile(.1);
    c.midi=noteClass+12*std::round((midiSum/weight-noteClass)/12)+c.cents/100;c.confidence=confidenceSum/weight;c.observations=observations;return c;
}
void LiveWorkspace::push(const WorkspaceFrame& f) noexcept {
    if(!std::isfinite(f.time)||f.time<0)return;
    const auto write=fifo.write(1);
    if(write.blockSize1+write.blockSize2)pending[size_t(write.blockSize1?write.startIndex1:write.startIndex2)]=f;
}
void LiveWorkspace::updateCentres(){
    for(size_t i=0;i<12;++i){data.source[i]=sourceStats[i].centre();data.reference[i]=referenceStats[i].centre();
        sourceValues[i]=float(data.source[i].cents);sourceKnownValues[i]=data.source[i].known;
        referenceValues[i]=float(data.reference[i].cents);referenceKnownValues[i]=data.reference[i].known;}
}
void LiveWorkspace::rememberReference(const WorkspaceFrame& f){
    if(!std::isfinite(f.time)||f.time<0||!std::isfinite(f.reference)||f.reference<=0||!std::isfinite(f.referenceCentre)||!std::isfinite(f.referenceConfidence)||f.referenceConfidence<.8f)return;
    const double midi=f.referenceCentre>0?f.referenceCentre:f.reference;if(midi<12||midi>120)return;
    WorkspaceFrame value;value.time=f.time;value.reference=f.reference;value.referenceCentre=f.referenceCentre;value.referenceConfidence=f.referenceConfidence;
    auto at=std::lower_bound(referenceFrames.begin(),referenceFrames.end(),f.time,[](const auto& a,double t){return a.time<t;});
    if(at!=referenceFrames.end()&&std::abs(at->time-f.time)<.012)*at=value;
    else if(at!=referenceFrames.begin()&&std::abs(std::prev(at)->time-f.time)<.012)*std::prev(at)=value;
    else referenceFrames.insert(at,value);
    if(referenceFrames.size()>maxFrames)referenceFrames.erase(referenceFrames.begin(),referenceFrames.begin()+int(referenceFrames.size()-maxFrames));
}
void LiveWorkspace::attachReference(WorkspaceFrame& f)const{
    if(f.reference>0&&f.referenceConfidence>=.8f)return;
    auto at=std::lower_bound(referenceFrames.begin(),referenceFrames.end(),f.time,[](const auto& a,double t){return a.time<t;});
    if(at==referenceFrames.end()||std::abs(at->time-f.time)>=.012){
        if(at==referenceFrames.begin()||std::abs(std::prev(at)->time-f.time)>=.012)return;
        --at;
    }
    f.reference=at->reference;f.referenceCentre=at->referenceCentre;f.referenceConfidence=at->referenceConfidence;
}
void LiveWorkspace::rebuildReferenceStats(){
    referenceStats={};
    for(const auto& f:referenceFrames){const double m=f.referenceCentre>0?f.referenceCentre:f.reference;referenceStats[size_t((int(std::lround(m))%12+12)%12)].add(m,f.referenceConfidence);}
}
void LiveWorkspace::rebuildRegions(){
    data.regions.clear();
    for(const auto& f:data.frames){if(f.source<=0||f.confidence<.8)continue;
        const double centre=f.sourceCentre>0?f.sourceCentre:f.source;const int key=int(std::lround(centre));
        if(!data.regions.empty()){auto& r=data.regions.back();
            if(r.key==key&&f.time-r.end<.14&&f.time>=r.start&&std::abs(centre-r.midi)<.12){
                const double age=std::max(.02,r.end-r.start);r.midi=(r.midi*age+centre*.025)/(age+.025);r.end=f.time+.025;r.confidence=std::min(r.confidence,double(f.confidence));continue;}}
        data.regions.push_back({std::max(0.0,f.time-.025),f.time+.025,centre,f.confidence,key});
    }
}
void LiveWorkspace::drain(){
    const juce::ScopedLock guard(lock);bool changed=false;
    auto add=[&](WorkspaceFrame f){
        rememberReference(f);attachReference(f);
        auto at=std::lower_bound(data.frames.begin(),data.frames.end(),f.time,[](const auto& a,double t){return a.time<t;});
        if(at!=data.frames.end()&&std::abs(at->time-f.time)<.012)*at=f;
        else if(at!=data.frames.begin()&&std::abs(std::prev(at)->time-f.time)<.012)*std::prev(at)=f;
        else data.frames.insert(at,f);
        changed=true;
    };
    {const auto read=fifo.read(fifo.getNumReady());for(int i=0;i<read.blockSize1;++i)add(pending[size_t(read.startIndex1+i)]);for(int i=0;i<read.blockSize2;++i)add(pending[size_t(read.startIndex2+i)]);}
    if(changed){sourceStats={};
        if(data.frames.size()>maxFrames)data.frames.erase(data.frames.begin(),data.frames.begin()+int(data.frames.size()-maxFrames));
        for(const auto& f:data.frames){
            if(f.source>0&&f.confidence>=.8){const double m=f.sourceCentre>0?f.sourceCentre:f.source;sourceStats[size_t((int(std::lround(m))%12+12)%12)].add(m,f.confidence);}
        }
        rebuildReferenceStats();updateCentres();rebuildRegions();++data.revision;}
}
void LiveWorkspace::clear(bool referenceOnly){
    const juce::ScopedLock guard(lock);{const auto discarded=fifo.read(fifo.getNumReady());(void)discarded;}referenceStats={};referenceFrames.clear();
    if(!referenceOnly){sourceStats={};data.frames.clear();data.regions.clear();data.edits.clear();editCount=0;}
    else for(auto& f:data.frames){f.reference=f.referenceCentre=f.referenceConfidence=0;}
    updateCentres();++data.revision;
}
void LiveWorkspace::analyseFile(const Clip& clip,bool reference){
    const juce::ScopedLock guard(lock);
    if(reference){referenceStats={};referenceFrames.clear();for(auto& f:data.frames)f.reference=f.referenceCentre=f.referenceConfidence=0;}
    else{sourceStats={};data.frames.clear();data.regions.clear();}
    for(const auto& f:clip.frames){if(f.midi<=0||f.confidence<.8)continue;
        double centre=f.midi;for(const auto& n:clip.notes)if(f.time>=n.start&&f.time<=n.end){centre=n.centre;break;}
        if(reference)rememberReference({clip.anchorSeconds+f.time,0,0,float(f.midi),float(centre),0,float(f.confidence)});
        else{sourceStats[size_t((int(std::lround(centre))%12+12)%12)].add(centre,f.confidence);WorkspaceFrame frame{clip.anchorSeconds+f.time,float(f.midi),float(centre),0,0,float(f.confidence),0};attachReference(frame);data.frames.push_back(frame);}
    }
    if(reference)for(auto& f:data.frames)attachReference(f);
    rebuildReferenceStats();
    updateCentres();if(!reference)rebuildRegions();++data.revision;
}
WorkspaceSnapshot LiveWorkspace::snapshot() const {const juce::ScopedLock guard(lock);return data;}
void LiveWorkspace::publishEdits(){
    editCount=0;const int count=std::min(maxEdits,int(data.edits.size()));
    for(int i=0;i<count;++i){const auto& e=data.edits[size_t(i)];realtimeEdits[size_t(i)].start=e.start;realtimeEdits[size_t(i)].end=e.end;realtimeEdits[size_t(i)].cents=e.cents;}
    editCount=count;
}
void LiveWorkspace::edit(TimeCorrection e){
    if(!std::isfinite(e.start)||!std::isfinite(e.end)||!std::isfinite(e.cents)||e.start<0||e.end<=e.start)return;
    e.cents=juce::jlimit(-1200.0,1200.0,e.cents);const juce::ScopedLock guard(lock);
    auto at=std::find_if(data.edits.begin(),data.edits.end(),[&](const auto& x){return std::abs(e.start-x.start)<.02&&std::abs(e.end-x.end)<.02;});
    if(at!=data.edits.end())*at=e;else if(data.edits.size()<maxEdits)data.edits.push_back(e);
    publishEdits();++data.revision;
}
bool LiveWorkspace::correctionAt(double time,double& cents) const noexcept {
    double previous=-1,next=1e30,left=0,right=0;
    for(int i=editCount.load()-1;i>=0;--i){const auto& e=realtimeEdits[size_t(i)];const double start=e.start.load(),end=e.end.load(),value=e.cents.load();
        if(time>=start&&time<=end){cents=value;return true;}
        if(end<time&&end>previous){previous=end;left=value;}
        if(start>time&&start<next){next=start;right=value;}
    }
    if(previous>=0&&next-previous<=.15){cents=left+(right-left)*(time-previous)/(next-previous);return true;}
    return false;
}
void LiveWorkspace::appendConversation(const juce::String& role,const juce::String& text){const juce::ScopedLock guard(lock);data.conversation+=role+": "+text+"\n\n";if(data.conversation.length()>48000)data.conversation=data.conversation.getLastCharacters(48000);++data.revision;}
juce::ValueTree LiveWorkspace::save(){
    drain();const juce::ScopedLock guard(lock);juce::ValueTree out("LiveWorkspace");out.setProperty("conversation",data.conversation,nullptr);
    auto centres=[&](const char* name,const std::array<Distribution,12>& stats){juce::ValueTree group(name);for(size_t i=0;i<stats.size();++i){const auto c=stats[i].centre();if(!c.known)continue;juce::ValueTree row("Centre");row.setProperty("key",int(i),nullptr);row.setProperty("midi",c.midi,nullptr);row.setProperty("confidence",c.confidence,nullptr);row.setProperty("count",std::min(128,c.observations),nullptr);group.addChild(row,-1,nullptr);}out.addChild(group,-1,nullptr);};
    centres("Source",sourceStats);centres("Reference",referenceStats);
    juce::ValueTree frames("Frames");for(const auto& f:data.frames){juce::ValueTree row("Frame");row.setProperty("time",f.time,nullptr);row.setProperty("source",f.source,nullptr);row.setProperty("centre",f.sourceCentre,nullptr);row.setProperty("reference",f.reference,nullptr);row.setProperty("referenceCentre",f.referenceCentre,nullptr);row.setProperty("confidence",f.confidence,nullptr);row.setProperty("referenceConfidence",f.referenceConfidence,nullptr);frames.addChild(row,-1,nullptr);}out.addChild(frames,-1,nullptr);
    juce::ValueTree references("ReferenceFrames");for(const auto& f:referenceFrames){juce::ValueTree row("Frame");row.setProperty("time",f.time,nullptr);row.setProperty("midi",f.reference,nullptr);row.setProperty("centre",f.referenceCentre,nullptr);row.setProperty("confidence",f.referenceConfidence,nullptr);references.addChild(row,-1,nullptr);}out.addChild(references,-1,nullptr);
    juce::ValueTree edits("Edits");for(const auto& e:data.edits){juce::ValueTree row("Edit");row.setProperty("start",e.start,nullptr);row.setProperty("end",e.end,nullptr);row.setProperty("cents",e.cents,nullptr);edits.addChild(row,-1,nullptr);}out.addChild(edits,-1,nullptr);return out;
}
void LiveWorkspace::restore(const juce::ValueTree& tree){
    clear();const juce::ScopedLock guard(lock);if(!tree.hasType("LiveWorkspace"))return;
    data.conversation=tree.getProperty("conversation").toString().getLastCharacters(48000);
    auto centres=[&](const char* name,std::array<Distribution,12>& stats){for(const auto& row:tree.getChildWithName(name)){const int key=int(row.getProperty("key",-1));const double m=double(row.getProperty("midi",0)),c=double(row.getProperty("confidence",0));if(key<0||key>=12||!std::isfinite(m)||m<12||m>120||!std::isfinite(c))continue;for(int i=0;i<juce::jlimit(3,128,int(row.getProperty("count",3)));++i)stats[size_t(key)].add(m,c);}};
    centres("Source",sourceStats);centres("Reference",referenceStats);
    for(const auto& row:tree.getChildWithName("Frames")){if(data.frames.size()>=maxFrames)break;WorkspaceFrame f;f.time=double(row.getProperty("time",-1));f.source=float(row.getProperty("source",0));f.sourceCentre=float(row.getProperty("centre",0));f.reference=float(row.getProperty("reference",0));f.referenceCentre=float(row.getProperty("referenceCentre",0));f.confidence=float(row.getProperty("confidence",0));f.referenceConfidence=float(row.getProperty("referenceConfidence",0));if(std::isfinite(f.time)&&f.time>=0&&std::isfinite(f.source)&&std::isfinite(f.sourceCentre)&&std::isfinite(f.reference)&&std::isfinite(f.referenceCentre))data.frames.push_back(f);}
    std::sort(data.frames.begin(),data.frames.end(),[](const auto& a,const auto& b){return a.time<b.time;});
    const auto references=tree.getChildWithName("ReferenceFrames");
    if(references.isValid()){
        for(const auto& row:references){if(referenceFrames.size()>=maxFrames)break;WorkspaceFrame f;f.time=double(row.getProperty("time",-1));f.reference=float(row.getProperty("midi",0));f.referenceCentre=float(row.getProperty("centre",0));f.referenceConfidence=float(row.getProperty("confidence",0));rememberReference(f);}
    }else for(const auto& f:data.frames)rememberReference(f);
    if(references.isValid()||!referenceFrames.empty())rebuildReferenceStats();
    for(const auto& row:tree.getChildWithName("Edits")){if(data.edits.size()>=maxEdits)break;const double start=double(row.getProperty("start",-1)),end=double(row.getProperty("end",-1)),cents=double(row.getProperty("cents",0));if(std::isfinite(start)&&std::isfinite(end)&&std::isfinite(cents)&&start>=0&&end>start&&std::abs(cents)<=1200)data.edits.push_back({start,end,cents});}
    updateCentres();rebuildRegions();publishEdits();++data.revision;
}
}
