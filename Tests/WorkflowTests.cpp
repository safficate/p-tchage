#include "PluginEditor.h"
#include <iostream>
#include <stdexcept>

static int assertions=0;
static void check(bool ok,const char* message){++assertions;if(!ok)throw std::runtime_error(message);}
static void pump(int milliseconds=650){
    const auto end=juce::Time::getMillisecondCounter()+juce::uint32(milliseconds);
    do{juce::Timer::callPendingTimersSynchronously();juce::Thread::sleep(5);}while(juce::Time::getMillisecondCounter()<end);
}
static void finish(PitchProcessor& p){
    const auto end=juce::Time::getMillisecondCounter()+30000;
    while(p.busy&&juce::Time::getMillisecondCounter()<end){juce::Timer::callPendingTimersSynchronously();juce::Thread::sleep(5);}
    check(!p.busy,"Background operation must finish");
}
struct Cursor:juce::AudioPlayHead{
    int64_t sample=30*48000;
    juce::Optional<PositionInfo> getPosition()const override{PositionInfo p;p.setTimeInSamples(sample);p.setIsPlaying(true);return p;}
};
static double pitch(PitchProcessor& p,bool preview=false){
    juce::AudioBuffer<float> block(2,4096);juce::MidiBuffer midi;
    for(int i=0;i<(preview?4:1);++i){block.clear();p.processBlock(block,midi);}
    const auto [hz,confidence]=mp::detectPitch(block.getReadPointer(0),4096,48000);
    check(hz>0&&confidence>.9,"Audition/host callback must produce a real voiced output");return (mp::hzToMidi(hz)-69)*100;
}
int main(int argc,char** argv){
    std::cout<<std::unitbuf;juce::ScopedJuceInitialiser_GUI gui;
    try{
        mp::Clip source;source.sampleRate=48000;source.audio.setSize(2,144000);
        for(int i=0;i<source.audio.getNumSamples();++i){const double phase=2*juce::MathConstants<double>::pi*mp::midiToHz(69.1)*i/48000;
            for(int ch=0;ch<2;++ch)source.audio.setSample(ch,i,float(.15*(std::sin(phase)+.3*std::sin(2*phase)+.15*std::sin(3*phase))));}
        const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("pitchmap-workflow",".wav");
        check(mp::writeWav(file,source.audio,source.sampleRate),"Create unanalysed WAV fixture");
        PitchProcessor p;p.prepareToPlay(48000,256);p.setParameter("formants",0);p.setParameter("cent9",5);
        Cursor cursor;p.setPlayHead(&cursor);juce::AudioBuffer<float> block(2,256);juce::MidiBuffer midi;block.clear();p.processBlock(block,midi);
        p.load(file);finish(p);file.deleteFile();
        std::cout<<"IMPORT offset="<<juce::String(p.parameters.getRawParameterValue("offset")->load(),9)<<" status="<<p.status()<<'\n';
        check(p.clip()&&p.clip()->notes.empty(),"Import loads audio without inventing a pitch map");
        check(std::abs(p.parameters.getRawParameterValue("offset")->load()-30)<.001,"Imported WAV begins at the supplied DAW cursor, not project zero");
        check(p.parameters.getRawParameterValue("useRender")->load()==1,"Imported WAV uses the visible clip playback mode");
        p.analyse();finish(p);pump();
        check(!p.clip()->notes.empty(),"Analysis must establish a measured source centre");
        check(!p.correctionResult()&&p.parameters.getRawParameterValue("enabled")->load()==0,"Analysis alone proposes; it does not enable or publish corrected PCM");
        const auto plan=mp::planCorrection(*p.clip(),p.settings());
        check(!plan.regions.empty()&&std::abs(plan.regions.front().requestedCents+5)<.3,"Source A +10 c to user target A +5 c must propose -5 c");
        PitchMap view(p);view.refresh();
        check(view.proposalText().contains("DAYANAK:")&&view.proposalText().contains("fark")&&view.proposalText().contains("ONERI:"),"Proposal explains basis, source-to-target difference and pending state");
        p.preview(true);const double original=pitch(p,true);p.preview(false);
        check(std::abs(original-10)<.4,"Full original clip is audible before Apply with correction disabled");
        p.applyCorrection();finish(p);pump();
        auto applied=p.correctionResult();check(applied&&applied->plan.regions.front().verificationAvailable,"Apply publishes measured correction evidence");
        cursor.sample=31*48000;const double first=pitch(p);
        std::cout<<"ABSOLUTE source=+10 target=+5 measured="<<first<<" cents from A440\n";
        check(std::abs(first-5)<1,"Apply must send the requested -5 c change to the DAW callback at the imported timeline position");
        view.refresh();check(view.proposalText().contains("SONUC:")&&view.proposalText().contains("OLCULEN"),"Applied view shows measured output and deviation");
        p.setParameter("cent9",-5);pump(800);
        check(p.correctionResult()==applied,"Changing a target must not silently render a new clip");
        check(std::abs(pitch(p)-first)<.1,"Until Apply, cached playback retains the last applied sound");
        view.refresh();check(view.proposalText().contains("ONERI:"),"Changed targets show a pending proposal instead of stale verified success");
        const auto exported=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("pitchmap-applied-export",".wav");
        p.exportAudio(exported);finish(p);juce::String error;const auto savedAudio=mp::loadAudio(exported,error);exported.deleteFile();
        check(savedAudio!=nullptr,"Applied rendered PCM must export successfully");
        const auto exportedPitch=mp::detectPitch(savedAudio->audio.getReadPointer(0,48000),4096,48000).first;
        check(exportedPitch>0&&std::abs((mp::hzToMidi(exportedPitch)-69)*100-first)<.5,"WAV export must preserve the heard adaptive render, not silently apply pending targets with another engine");

        p.applyCorrection();finish(p);check(p.correctionResult()!=applied,"Second Apply replaces the old rendered result");
        check(std::abs(pitch(p)+5)<1,"Second Apply really changes output to the new -5 c target");
        p.setParameter("direct",1);p.setParameter("referenceMatch",0);p.setParameter("globalCents",5);p.setParameter("cent9",2);
        const auto relative=mp::planCorrection(*p.clip(),p.settings());
        check(std::abs(relative.regions.front().requestedCents-7)<.01,"Relative mode adds global +5 and note +2 without snapping the source centre");
        p.auditionTarget(9,4,0);const double tone=pitch(p,true);p.stopAudition();
        check(std::abs(tone-17)<.5,"Relative target key auditions source +10 plus requested +7, not standard A +2");
        p.applyCorrection();finish(p);const double shifted=pitch(p);
        std::cout<<"RELATIVE source=+10 global=+5 note=+2 measured="<<shifted<<" cents from A440\n";
        check(std::abs(shifted-17)<1,"Relative amount must reach actual output PCM");
        p.setParameter("bypass",1);p.preview(true);const double bypassed=pitch(p,true);p.preview(false);p.setParameter("bypass",0);
        check(std::abs(bypassed-10)<.4,"Bypass during local clip audition plays the original instead of silence");
        juce::MemoryBlock saved;p.getStateInformation(saved);PitchProcessor restored;restored.setStateInformation(saved.getData(),int(saved.getSize()));
        check(restored.parameters.getRawParameterValue("globalCents")->load()==5&&restored.settings().directShift&&std::abs(restored.parameters.getRawParameterValue("offset")->load()-30)<.001,"Host state restores relative mode, global cents and clip position");
        auto v3=juce::ValueTree::readFromData(saved.getData(),saved.getSize());v3.setProperty("version",3,nullptr);
        auto params=v3.getChildWithName("Parameters");for(int i=params.getNumChildren()-1;i>=0;--i)if(params.getChild(i)["id"].toString()=="globalCents")params.removeChild(i,nullptr);
        juce::MemoryOutputStream stream;v3.writeToStream(stream);restored.setParameter("globalCents",99);restored.setStateInformation(stream.getData(),int(stream.getDataSize()));
        check(restored.parameters.getRawParameterValue("globalCents")->load()==0&&restored.settings().adaptive,"Version 3 migration clears new relative offset while preserving adaptive mode");
        PitchProcessor quick;quick.prepareToPlay(48000,256);quick.setParameter("cent9",5);quick.setParameter("formants",0);
        juce::ValueTree raw("MicrotonalPitchMapPro");raw.setProperty("version",mp::stateVersion,nullptr);raw.addChild(mp::clipToTree(source),-1,nullptr);
        juce::MemoryOutputStream rawStream;raw.writeToStream(rawStream);quick.setStateInformation(rawStream.getData(),int(rawStream.getDataSize()));
        quick.applyCorrection();finish(quick);pump();finish(quick);
        check(quick.correctionResult()&&quick.clip()->adaptiveAnalysis,"Apply on an unanalysed clip must complete analysis then actual rendering without a second click");
        if(argc==2){
            const juce::File folder(argv[1]);folder.createDirectory();
            auto editor=std::unique_ptr<juce::AudioProcessorEditor>(p.createEditor());
            for(const auto& size:{juce::Point<int>(1060,680),juce::Point<int>(1240,820)}){
                editor->setSize(size.x,size.y);pump(120);
                juce::Image image(juce::Image::ARGB,size.x,size.y,true,juce::SoftwareImageType());
                {juce::Graphics graphics(image);editor->paintEntireComponent(graphics,true);}
                auto stream=folder.getChildFile("workflow-"+juce::String(size.x)+".png").createOutputStream();
                check(stream&&juce::PNGImageFormat().writeImageToStream(image,*stream),"Save actual workflow editor painting for visual review");
            }
        }
        p.setPlayHead(nullptr);
        std::cout<<assertions<<" correction workflow assertions passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL after "<<assertions<<" checks: "<<e.what()<<'\n';return 1;}
}
