#include "LiveEditor.h"
#include <algorithm>
#include <cmath>

namespace {
const juce::Colour background{0xff101722},panel{0xff192431},ink{0xffe2ecf5},cyan{0xff35c4ed},amber{0xffffc65b},green{0xff59dda4};
juce::String note(int n){static const char* names[]={"Do","Do#","Re","Re#","Mi","Fa","Fa#","Sol","Sol#","La","La#","Si"};return names[(n%12+12)%12];}
class Curves final:public juce::Component {
public:
    explicit Curves(PitchProcessor& p):processor(p){setWantsKeyboardFocus(true);}
    PitchProcessor& processor;mp::WorkspaceSnapshot data;
    double begin=0,span=12,low=45,range=36;int selected=-1;float downY=0;double drag=0,dragBase=0;bool regions=false;
    float x(double t)const{return float(48+(t-begin)/span*(getWidth()-62));}
    float y(double midi)const{return float(10+(low+range-midi)/range*(getHeight()-34));}
    double at(float pixel)const{return begin+(pixel-48)/(getWidth()-62)*span;}
    void paint(juce::Graphics& g)override{
        g.fillAll(background);g.setFont(11.0f);
        for(int k=int(std::ceil(low));k<=low+range;++k){const auto yy=y(k);g.setColour(ink.withAlpha(k%12==0?.24f:.07f));g.drawHorizontalLine(int(yy),48,float(getWidth()-10));if(range<=30||k%2==0){g.setColour(ink.withAlpha(.7f));g.drawText(note(k)+juce::String(k/12-1),1,int(yy)-7,43,15,juce::Justification::right);}}
        for(int i=0;i<9;++i){const double t=begin+span*i/8;g.setColour(ink.withAlpha(.08f));g.drawVerticalLine(int(x(t)),8,float(getHeight()-24));g.setColour(ink.withAlpha(.65f));g.drawText(juce::String(t,1),int(x(t))-25,getHeight()-22,50,18,juce::Justification::centred);}
        g.saveState();g.reduceClipRegion(48,0,getWidth()-60,getHeight()-22);
        if(regions)for(size_t i=0;i<data.regions.size();++i){const auto& r=data.regions[i];if(r.end<begin||r.start>begin+span)continue;const auto colour=juce::Colour::fromHSV(float(r.key%12)/12,.6f,.9f,.15f);g.setColour(int(i)==selected?cyan.withAlpha(.25f):colour);g.fillRoundedRectangle(x(r.start),y(r.midi+.35),std::max(3.0f,x(r.end)-x(r.start)),std::max(8.0f,y(r.midi-.35)-y(r.midi+.35)),3);}
        juce::Path source,target,reference;bool active=false,refActive=false;double last=-1;
        for(const auto& f:data.frames){if(f.time<begin-.1)continue;if(f.time>begin+span+.1)break;
            if(f.source>0&&f.confidence>=.8){const int key=int(std::lround(f.sourceCentre>0?f.sourceCentre:f.source));const double centre=f.sourceCentre>0?f.sourceCentre:f.source;
                double planned=f.source+processor.targetForSource(key%12,centre)-centre;
                for(auto it=data.edits.rbegin();it!=data.edits.rend();++it)if(f.time>=it->start&&f.time<=it->end){planned+=it->cents/100;break;}
                if(!active||f.time-last>.12){source.startNewSubPath(x(f.time),y(f.source));target.startNewSubPath(x(f.time),y(planned));}else{source.lineTo(x(f.time),y(f.source));target.lineTo(x(f.time),y(planned));}active=true;last=f.time;
            }else active=false;
            if(f.reference>0){if(!refActive)reference.startNewSubPath(x(f.time),y(f.reference));else reference.lineTo(x(f.time),y(f.reference));refActive=true;}else refActive=false;
        }
        g.setColour(amber);g.strokePath(source,juce::PathStrokeType(1.6f));g.setColour(cyan);g.strokePath(target,juce::PathStrokeType(1.4f));g.setColour(green);g.strokePath(reference,juce::PathStrokeType(1.2f));
        g.setColour(ink.withAlpha(.5f));g.drawVerticalLine(int(x(processor.transportSeconds())),0,float(getHeight()));g.restoreState();
        g.setColour(ink.withAlpha(.7f));g.drawText("Sari: kaynak / Mavi: hedef plani / Yesil: host sidechain",52,2,getWidth()-60,16,juce::Justification::left);
    }
    void mouseDown(const juce::MouseEvent& e)override{downY=e.position.y;drag=0;const double time=at(e.position.x);selected=-1;for(size_t i=0;i<data.regions.size();++i)if(time>=data.regions[i].start&&time<=data.regions[i].end){selected=int(i);break;}dragBase=0;if(selected>=0){const auto& r=data.regions[size_t(selected)];for(const auto& edit:data.edits)if(std::abs(r.start-edit.start)<.02&&std::abs(r.end-edit.end)<.02)dragBase=edit.cents;}repaint();}
    void mouseDrag(const juce::MouseEvent& e)override{if(selected>=0){drag=dragBase+(downY-e.position.y)*range*100/std::max(1,getHeight()-34);const auto& r=data.regions[size_t(selected)];processor.editLiveRegion(r.start,r.end,drag);repaint();}}
    void mouseUp(const juce::MouseEvent&)override{if(selected>=0&&std::abs(drag)>.01){const auto& r=data.regions[size_t(selected)];processor.editLiveRegion(r.start,r.end,drag);drag=0;}}
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w)override{
        if(e.mods.isAltDown()){low=juce::jlimit(0.0,116.0-range,low+w.deltaY*range*.4);}
        else if(e.mods.isCtrlDown()){const double anchor=low+range*(1-e.position.y/std::max(1,getHeight()));range=juce::jlimit(3.0,100.0,range*std::exp(-w.deltaY*2));low=juce::jlimit(0.0,127.0-range,anchor-range*(1-e.position.y/std::max(1,getHeight())));}
        else if(e.mods.isShiftDown()){begin=std::max(0.0,begin-w.deltaY*span*.5);}
        else{const double anchor=at(e.position.x);span=juce::jlimit(.25,900.0,span*std::exp(-w.deltaY*2));begin=std::max(0.0,anchor-(e.position.x-48)/std::max(1,getWidth()-62)*span);}
        repaint();
    }
    void fit(){if(data.frames.empty())return;begin=std::max(0.0,data.frames.front().time-.1);span=std::max(1.0,data.frames.back().time-begin+.2);double a=127,b=0;for(const auto& f:data.frames)if(f.source>0){a=std::min(a,double(f.source));b=std::max(b,double(f.source));}if(b>0){low=std::max(0.0,a-2);range=std::max(6.0,b-a+4);}repaint();}
};
}

struct LiveEditor::Page final:juce::Component,private juce::Timer,private juce::ScrollBar::Listener {
    static constexpr int width=1240,height=970,left=180;
    LiveEditor& owner;PitchProcessor& p;juce::LookAndFeel_V4 theme;Curves graph;
    juce::ComboBox scale,reference,instances,expression;
    juce::TextButton equal{"ESITLE"},reread{"Bastan oku"},complete{"Tum parca burada"},details{"Smoother / Ayrintilar"},home{"Ana ekran"},region{"Nota / bolge"},chat{"AI Sohbet"},align{"Merkezi hizala"},copy{"Raporu kopyala"},send{"Gonder"},history{"Gecmis"};
    juce::ToggleButton bypass{"Bypass"},natural{"Dogal duzeltme"},follow{"Takip"};
    juce::Slider global,globalNotes,wet;
    std::array<juce::TextEditor,12> sources;
    std::array<juce::Slider,12> targets;
    std::array<juce::TextButton,12> sourceTones,targetTones;
    std::array<bool,12> sourceHeld{},targetHeld{};
    std::array<juce::ToggleButton,12> centres;
    std::array<juce::ComboBox,12> destinations;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> destinationBindings;
    std::vector<std::unique_ptr<juce::Slider>> advanced;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<juce::String> advancedIds,advancedNames;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderBindings;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonBindings;
    juce::TextEditor apiKey,model,input,conversation,report;
    juce::ScrollBar horizontal{false},vertical{true};
    mp::WorkspaceSnapshot snapshot;std::vector<PitchProcessor::ReferenceInstance> refs;
    int view=0,ticks=0,detailTab=0;
    juce::TextButton recordAudio{"12 sn cikis ornegi al"},sendAudio{"Sesle birlikte sor"};
    juce::TextButton pitchTab{"Perde / Humanize"},toneTab{"Tini / Rezonans"},repairTab{"Yerel Onarim"};float tickerOffset=0;bool updating=false;juce::String ticker;
    explicit Page(LiveEditor& editor,PitchProcessor& processor):owner(editor),p(processor),graph(p){
        theme.setColour(juce::ResizableWindow::backgroundColourId,background);theme.setColour(juce::Slider::rotarySliderFillColourId,cyan);theme.setColour(juce::Slider::thumbColourId,cyan);theme.setColour(juce::TextButton::buttonColourId,panel);theme.setColour(juce::TextEditor::backgroundColourId,panel);theme.setColour(juce::TextEditor::textColourId,ink);theme.setColour(juce::Label::textColourId,ink);setLookAndFeel(&theme);
        p.enterLiveWorkspace();
        for(auto* b:{&pitchTab,&toneTab,&repairTab,&recordAudio,&sendAudio})addAndMakeVisible(*b);
        pitchTab.onClick=[this]{detailTab=0;resized();repaint();};toneTab.onClick=[this]{detailTab=1;resized();repaint();};repairTab.onClick=[this]{detailTab=2;resized();repaint();};
        for(auto* b:{&equal,&reread,&complete,&details,&home,&region,&chat,&align,&copy,&send,&history})addAndMakeVisible(*b);
        for(auto* b:{&bypass,&natural,&follow})addAndMakeVisible(*b);
        buttonBindings.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,"bypass",bypass));buttonBindings.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,"natural",natural));
        for(int n:{100,125,150,175,200})scale.addItem(juce::String(n)+"%",n);addAndMakeVisible(scale);scale.setSelectedId(int(value("uiScale")),juce::dontSendNotification);scale.onChange=[this]{p.setParameter("uiScale",float(scale.getSelectedId()));owner.applyScale(float(scale.getSelectedId())/100);};
        equal.onClick=[this]{p.equalizeMeasured();};reread.onClick=[this]{p.resetLiveLearning();};complete.onClick=[this]{p.markComplete();graph.fit();};details.onClick=[this]{setView(1);};home.onClick=[this]{setView(0);};region.onClick=[this]{setView(2);};chat.onClick=[this]{setView(3);};history.onClick=[this]{report.setText(p.activityText(),false);setView(4);};
        bind(global,"globalCents");global.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);global.setTextBoxStyle(juce::Slider::TextBoxBelow,false,140,24);
        bind(globalNotes,"globalNotes");globalNotes.setSliderStyle(juce::Slider::IncDecButtons);globalNotes.setTextBoxStyle(juce::Slider::TextBoxLeft,false,90,24);
        bind(wet,"wet");wet.setSliderStyle(juce::Slider::LinearHorizontal);
        for(int i=0;i<12;++i){const auto j=size_t(i);addAndMakeVisible(sources[j]);sources[j].setJustification(juce::Justification::centred);sources[j].onReturnKey=[this,i]{const auto text=sources[size_t(i)].getText();if(text.containsOnly("+-0123456789. ")&&text.trim().isNotEmpty())p.setMeasuredCentre(i,text.getDoubleValue());sources[size_t(i)].giveAwayKeyboardFocus();};
            auto& s=targets[j];addAndMakeVisible(s);s.setRange(-1400,1400,.01);s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,82,24);s.setTextValueSuffix(" c");s.onValueChange=[this,i]{if(!updating)p.setTargetCentre(i,targets[size_t(i)].getValue());};
            auto& sourceTone=sourceTones[j];sourceTone.setButtonText("Dinle");addAndMakeVisible(sourceTone);sourceTone.onStateChange=[this,i]{const bool down=sourceTones[size_t(i)].isDown();if(down!=sourceHeld[size_t(i)]){sourceHeld[size_t(i)]=down;if(down)p.auditionCentre(i,false);else p.releaseAuditionTone();}};
            auto& targetTone=targetTones[j];targetTone.setButtonText("Dinle");addAndMakeVisible(targetTone);targetTone.onStateChange=[this,i]{const bool down=targetTones[size_t(i)].isDown();if(down!=targetHeld[size_t(i)]){targetHeld[size_t(i)]=down;if(down)p.auditionCentre(i,true);else p.releaseAuditionTone();}};
            auto& destination=destinations[j];addAndMakeVisible(destination);for(int step=-12;step<=12;++step)destination.addItem(note(i)+" > "+note(i+step)+(std::abs(step)==12?(step>0?" +8":" -8"):""),step+13);destinationBindings.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters,"step"+juce::String(i),destination));
            auto& c=centres[j];addAndMakeVisible(c);c.setRadioGroupId(713);c.onClick=[this,i]{p.setParameter("centreNote",float(i));};}
        reference.addItem("Kendi hedeflerim",1);reference.addItem("Host sidechain: canli",2);reference.addItem("Host sidechain: ogrenilen",3);reference.addItem("Diger eklenti",4);addAndMakeVisible(reference);reference.onChange=[this]{p.setReferenceMode(reference.getSelectedId()-1);};
        addAndMakeVisible(instances);instances.onChange=[this]{const int i=instances.getSelectedId()-1;if(i>=0&&i<int(refs.size()))p.selectReferenceInstance(refs[size_t(i)].id);};align.onClick=[this]{p.alignReferenceCentre();};
        expression.addItem("Otomatik (sinyal davranisi)",1);expression.addItem("Surekli / yayli",2);expression.addItem("Atakli / telli",3);addAndMakeVisible(expression);expression.onChange=[this]{p.setParameter("expression",float(expression.getSelectedId()-1));};
        for(auto pair:{std::pair{"pitchOn","Perde smoother acik"},{"pitchAuto","Gecis davranisi otomatik"},{"amountAuto","Duzeltme miktari otomatik"},{"toneOn","Tini / rezonans smoother acik"},{"toneAuto","Rezonans esigi otomatik"},{"harmonicGuard","Harmonik koruma"},{"attackGuard","Atak koruma"},{"formants","Spektral zarf telafisi"},{"toneRange0","100-220 Hz"},{"toneRange1","280-600 Hz"},{"toneRange2","770-1700 Hz"},{"toneRange3","2200-4800 Hz"},{"toneRange4","6200-14000 Hz"},{"humanOn","Ifade sapmasi acik"},{"humanAuto","Ifade sapmasi otomatik"},{"finishOn","Yerel onarim filtresi acik"},{"finishAuto","Onarim miktari otomatik"},{"finishRegion","Yalniz secili zaman araligi"}}){auto b=std::make_unique<juce::ToggleButton>(pair.second);addAndMakeVisible(*b);buttonBindings.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.parameters,pair.first,*b));toggles.push_back(std::move(b));}
        for(auto pair:{std::pair{"pitchAmount","Duzeltme miktari (%)"},{"glideMs","Gecis / glide suresi (ms)"},{"centreMs","Merkez izleme (ms)"},{"maxRate","Duzeltme egimi (cent/sn)"},{"centreNarrow","Merkez daraltma (%)"},{"vibratoKeep","Vibrato koruma (%)"},{"toneDepth","Azami rezonans azaltma (dB)"},{"toneThreshold","Rezonans esigi (dB)"},{"toneAttack","Rezonans atagi (ms)"},{"toneRelease","Rezonans birakma (ms)"},{"toneLow","Alt sinir (Hz)"},{"toneHigh","Ust sinir (Hz)"},{"toneMix","Tini smoother karisimi (%)"},{"humanDepth","Ifade sapmasi (cent)"},{"humanTime","Sapma hareket suresi (ms)"},{"finishCutoff","Onarim filtre frekansi (Hz)"},{"finishAmount","Onarim karisimi (%)"},{"finishStart","Bolge baslangici (sn)"},{"finishEnd","Bolge bitisi (sn)"}}){auto s=std::make_unique<juce::Slider>();bind(*s,pair.first);s->setSliderStyle(juce::Slider::LinearHorizontal);s->setTextBoxStyle(juce::Slider::TextBoxRight,false,95,26);advancedIds.push_back(pair.first);advancedNames.push_back(pair.second);advanced.push_back(std::move(s));}
        for(auto* e:{&apiKey,&model,&input,&conversation,&report})addAndMakeVisible(*e);apiKey.setPasswordCharacter('*');apiKey.setTextToShowWhenEmpty("OpenAI API anahtari (yalniz bu oturum)",ink.withAlpha(.5f));model.setText("gpt-4.1-mini");input.setMultiLine(true);input.setTextToShowWhenEmpty("Neyi degistirmek istiyorsun?",ink.withAlpha(.5f));conversation.setMultiLine(true);conversation.setReadOnly(true);report.setMultiLine(true);report.setReadOnly(true);
        recordAudio.onClick=[this]{p.captureAssistantAudio();};sendAudio.onClick=[this]{p.askAssistant(apiKey.getText().trim(),model.getText().trim(),input.getText(),true);};
        send.onClick=[this]{if(!input.getText().trim().isEmpty()){p.askAssistant(apiKey.getText().trim(),model.getText().trim(),input.getText());input.clear();}};copy.onClick=[this]{juce::SystemClipboard::copyTextToClipboard(p.feedbackText());};
        addAndMakeVisible(graph);addAndMakeVisible(horizontal);addAndMakeVisible(vertical);horizontal.setAutoHide(false);vertical.setAutoHide(false);
        horizontal.addListener(this);vertical.addListener(this);
        follow.setToggleState(true,juce::dontSendNotification);setSize(width,height);setView(0);timerCallback();startTimerHz(15);
    }
    ~Page()override{stopTimer();p.stopAudition();setLookAndFeel(nullptr);}
    double value(const juce::String& id)const{return p.parameters.getRawParameterValue(id)->load();}
    void bind(juce::Slider& s,const juce::String& id){addAndMakeVisible(s);sliderBindings.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters,id,s));}
    void setView(int n){view=n;resized();repaint();}
    void resized()override{
        scale.setBounds(897,16,90,30);bypass.setBounds(1090,16,115,30);home.setBounds(20,16,130,30);home.setVisible(view!=0);
        for(auto* b:{&equal,&reread,&complete,&details,&region,&align})b->setVisible(view==0);natural.setVisible(view==0||view==1);reference.setVisible(view==0);instances.setVisible(view==0);global.setVisible(view==0);globalNotes.setVisible(view==0);wet.setVisible(view==0);expression.setVisible(view==1);
        for(size_t i=0;i<12;++i){sources[i].setVisible(view==0);targets[i].setVisible(view==0);centres[i].setVisible(view==0);destinations[i].setVisible(view==0);sourceTones[i].setVisible(view==0);targetTones[i].setVisible(view==0);const int x=left+int(i)*86;sourceTones[i].setBounds(x+20,300,46,23);targetTones[i].setBounds(x+20,548,46,23);sources[i].setBounds(x+3,239,80,28);destinations[i].setBounds(x+2,427,82,24);targets[i].setBounds(x+2,450,82,96);centres[i].setBounds(x+30,403,30,24);}
        equal.setBounds(510,340,210,32);reread.setBounds(22,281,142,30);complete.setBounds(1020,184,196,30);global.setBounds(23,416,140,117);globalNotes.setBounds(23,540,140,28);
        natural.setBounds(885,375,195,28);details.setBounds(1080,375,140,28);reference.setBounds(20,593,260,30);instances.setBounds(290,593,270,30);align.setBounds(570,593,170,30);region.setBounds(1020,915,200,30);chat.setBounds(1060,930,155,30);history.setBounds(1120,143,95,28);
        graph.setVisible(view==0||view==2);horizontal.setVisible(graph.isVisible());vertical.setVisible(graph.isVisible());follow.setVisible(graph.isVisible());graph.regions=view==2;
        if(view==0)graph.setBounds(20,640,1180,247);else if(view==2)graph.setBounds(20,245,1180,630);
        horizontal.setBounds(graph.getX(),graph.getBottom()+2,graph.getWidth(),15);vertical.setBounds(graph.getRight()+2,graph.getY(),15,graph.getHeight());follow.setBounds(20,910,90,25);wet.setBounds(150,920,260,30);
        pitchTab.setVisible(view==1);toneTab.setVisible(view==1);repairTab.setVisible(view==1);
        pitchTab.setBounds(30,185,360,32);toneTab.setBounds(430,185,360,32);repairTab.setBounds(830,185,360,32);
        int toggleRow=0;
        for(size_t i=0;i<toggles.size();++i){const bool pitch=i<3||i==7||i==13||i==14,tone=i>=3&&i<=12&&i!=7,repair=i>=15;
            const bool show=view==1&&((detailTab==0&&pitch)||(detailTab==1&&tone)||(detailTab==2&&repair));toggles[i]->setVisible(show);if(show)toggles[i]->setBounds(30,245+toggleRow++*33,530,28);}
        expression.setVisible(view==1&&detailTab==0);expression.setBounds(30,520,530,30);
        int sliderRow=0;
        for(size_t i=0;i<advanced.size();++i){const bool show=view==1&&((detailTab==0&&(i<6||i==13||i==14))||(detailTab==1&&i>=6&&i<=12)||(detailTab==2&&i>=15));advanced[i]->setVisible(show);if(show)advanced[i]->setBounds(830,260+sliderRow++*55,360,31);}
        natural.setBounds(view==1?30:885,view==1?91:375,view==1?250:195,28);
        for(auto* e:{&apiKey,&model,&input,&conversation})e->setVisible(view==3);copy.setVisible(view==3||view==4);send.setVisible(view==3);report.setVisible(view==4);
        recordAudio.setVisible(view==3);sendAudio.setVisible(view==3);recordAudio.setBounds(25,871,230,32);sendAudio.setBounds(275,871,230,32);
        apiKey.setBounds(25,203,780,32);model.setBounds(820,203,385,32);conversation.setBounds(25,285,1180,450);input.setBounds(25,752,1020,110);send.setBounds(1060,752,145,38);copy.setBounds(1060,810,145,36);report.setBounds(25,210,1180,675);
    }
    void paint(juce::Graphics& g)override{
        g.fillAll(background);g.setColour(ink);g.setFont(24.0f);g.drawText(view==0?"PITCHMAP PRO 3.1.0":"PITCHMAP / "+juce::String(view==1?"SMOOTHER & HUMANIZE":view==2?"NOTA / BOLGE":view==3?"AI ASISTAN":"ISLEM GECMISI"),view==0?20:165,15,700,34,juce::Justification::left);
        g.setFont(12.0f);g.setColour(p.liveInputLevel()>.0001f?green:ink.withAlpha(.6f));g.drawText(p.liveInputLevel()>.0001f?"Anlik isleniyor / otomatik duzeltiliyor":"Kanal sinyali bekleniyor",520,55,670,24,juce::Justification::right);
        g.setColour(ink.withAlpha(.7f));g.setFont(11.0f);g.drawText(p.status(),20,55,490,24,juce::Justification::left);
        g.setColour(panel);g.fillRoundedRectangle(20,139,1198,36,5);g.setColour(cyan);g.setFont(12.0f);
        g.saveState();g.reduceClipRegion(25,141,1080,32);g.drawText(ticker.isEmpty()?"Islemler burada sagdan sola akar. Motor komutlari gosterilir; bagimsiz cikis olcumu degildir.":ticker,30-int(tickerOffset),146,std::max(2200,int(ticker.length()*7.5)),22,juce::Justification::left);g.restoreState();
        if(view==0){
            g.setColour(amber);g.drawText("KAYNAK PROFILI (cent)",20,88,165,32,juce::Justification::left);juce::Path profile;bool active=false;for(int i=0;i<12;++i){const auto& c=snapshot.source[size_t(i)];const bool manual=value("sourceOn"+juce::String(i))>.5;if(c.known||manual){const float x=float(left+i*86+42),y=109-float(manual?value("source"+juce::String(i)):c.cents)*.25f;if(!active)profile.startNewSubPath(x,y);else profile.lineTo(x,y);g.fillEllipse(x-3,y-3,6,6);active=true;}else active=false;}g.strokePath(profile,juce::PathStrokeType(1.4f));
            g.setColour(ink);g.setFont(15.0f);g.drawText("KAYNAKTA OLCULEN",20,184,350,28,juce::Justification::left);g.drawText("HEDEFLERIM",20,375,175,28,juce::Justification::left);g.setFont(12.0f);g.drawText("Hizalama merkezi",185,380,220,20,juce::Justification::left);g.drawText("TUMUNU KAYDIR",20,405,150,20,juce::Justification::left);
            int total=0;for(const auto& c:snapshot.source)total+=c.observations;
            for(int i=0;i<12;++i){const int x=left+i*86;const auto& c=snapshot.source[size_t(i)];const bool manual=value("sourceOn"+juce::String(i))>.5;const double percent=total>0?100.0*c.observations/total:0;
                g.setColour(panel);g.drawRoundedRectangle(float(x),216,84,111,4,1);g.setColour(ink);g.drawText(note(i),x,217,84,21,juce::Justification::centred);g.drawText(note(i),x,428,84,20,juce::Justification::centred);
                g.setColour(manual?cyan:c.known?amber:ink.withAlpha(.5f));g.drawText(manual?"Elle":c.known?juce::String(percent,percent>0&&percent<1?2:1)+"%":"Okunmadi",x,278,84,20,juce::Justification::centred);g.fillRect(x+7,294,int(70*percent/100),5);
            }
            g.setColour(ink.withAlpha(.7f));g.drawText("Genel kaydirma hedef sayilarina dahildir; ikinci kez eklenmez.",185,574,1000,18,juce::Justification::left);g.drawText("Okunan bolge: "+juce::String(snapshot.frames.empty()?0:snapshot.frames.back().time,2)+" s",20,220,150,19,juce::Justification::left);g.drawText(value("complete")>.5?"Tum parca isaretlendi":"Yalniz duyulan alanlar",20,247,152,19,juce::Justification::left);g.drawText("Fare: zaman zoom | Ctrl: perde zoom | Shift: zaman kaydir | Alt: perde kaydir",160,907,820,24,juce::Justification::left);
        }else if(view==1){
            g.setColour(ink);g.setFont(14.0f);g.drawText("Her modul ayri acilir. Otomatik: sinyal tabanli kurallar; egitilmis calgi tanima degildir.",310,91,890,30,juce::Justification::left);
            g.setFont(12.0f);g.drawText((p.preparedPlayback()?"Hazir bolgesel ses / canli olcumler: ":"Etkin gecis: ")+juce::String(p.effectiveSmoothing(),1)+" ms | Etkin miktar: "+juce::String(p.effectiveAmount()*100,1)+"%",30,505,1170,26,juce::Justification::left);
            int sliderRow=0;for(size_t i=0;i<advancedNames.size();++i)if(advanced[i]->isVisible()){g.drawText(advancedNames[i],610,260+sliderRow++*55,215,31,juce::Justification::left);}
            g.drawText(detailTab==0?"%100 = tam hedef. Sapma acilirsa bilerek ek cent hareketi eklenir.":detailTab==1?"Kesme yapan dinamik EQ. Calgi karakteri dinleyerek degerlendirilmelidir.":"Istege bagli son filtre. Temel perde hedefini degistirmez; tiniyi koyulastirabilir.",30,770,1170,35,juce::Justification::left);
            if(detailTab==1)for(int i=0;i<20;++i){const float reduction=p.toneReduction(i);g.setColour(cyan);g.fillRect(40+i*57,919-int(reduction*5),35,int(reduction*5));g.setColour(ink.withAlpha(.7f));g.drawText(juce::String(p.toneFrequency(i),0),30+i*57,925,55,17,juce::Justification::centred);}
            if(detailTab==2)g.drawText("Etkin onarim karisimi: "+juce::String(p.finishAmount()*100,1)+"%",30,825,1150,30,juce::Justification::left);
        }else if(view==2){g.setColour(ink);g.drawText("Algilanan bolgeler. Dikey surukleme: yerel ek cent. Bagimsiz polifonik ses ayirma henuz yok.",25,195,1170,35,juce::Justification::left);}
        else if(view==3){g.setColour(ink);g.drawText("OpenAI API baglantisi. Gonder: analiz metni. Sesle sor: aldigin 12 sn islenmis ses de gonderilir. Anahtar projeye kaydedilmez.",25,245,1180,30,juce::Justification::left);}
        g.setColour(ink.withAlpha(.5f));g.setFont(11.0f);g.drawText("Olculen kaynak / hedef plani / DSP komutu ayri bilgilerdir. Sonar'da yerel test gereklidir.",20,952,1200,17,juce::Justification::left);
    }
    void scrollBarMoved(juce::ScrollBar* bar,double start)override{if(updating)return;if(bar==&horizontal){graph.begin=start;follow.setToggleState(false,juce::dontSendNotification);}else graph.low=127-start-graph.range;graph.repaint();}
    void timerCallback()override{
        updating=true;snapshot=p.workspaceSnapshot();graph.data=snapshot;
        for(int i=0;i<12;++i){const auto j=size_t(i);const auto& c=snapshot.source[j];const bool manual=value("sourceOn"+juce::String(i))>.5;
            if(!sources[j].hasKeyboardFocus(false))sources[j].setText(manual?juce::String(value("source"+juce::String(i)),2):c.known?juce::String(c.cents,2):"--",false);
            if(!targets[j].isMouseButtonDown()&&!targets[j].hasKeyboardFocus(true)){const double base=manual?value("source"+juce::String(i)):c.known?c.cents:0;targets[j].setValue((p.targetForSource(i,60+i+base/100)-60-i-value("step"+juce::String(i))-value("globalNotes"))*100,juce::dontSendNotification);}
            centres[j].setToggleState(i==int(value("centreNote")),juce::dontSendNotification);
        }
        reference.setSelectedId(int(value("referenceMode"))+1,juce::dontSendNotification);expression.setSelectedId(int(value("expression"))+1,juce::dontSendNotification);
        if(++ticks%30==0){const auto fresh=p.referenceInstances();bool changed=fresh.size()!=refs.size();if(!changed)for(size_t i=0;i<fresh.size();++i)changed|=fresh[i].id!=refs[i].id||fresh[i].name!=refs[i].name;if(changed){const int old=instances.getSelectedId();refs=fresh;instances.clear(juce::dontSendNotification);for(size_t i=0;i<refs.size();++i)instances.addItem(refs[i].name,int(i)+1);instances.setSelectedId(old,juce::dontSendNotification);}}
        conversation.setText(snapshot.conversation,false);send.setEnabled(!p.assistantBusy.load());sendAudio.setEnabled(!p.assistantBusy.load()&&p.assistantAudioReady());recordAudio.setButtonText(p.assistantAudioReady()?"Ses ornegi hazir / yeniden al":"12 sn ornek: "+juce::String(p.assistantCaptureProgress()*100,0)+"%");
        if(scale.getSelectedId()!=int(value("uiScale"))){scale.setSelectedId(int(value("uiScale")),juce::dontSendNotification);owner.applyScale(float(value("uiScale"))/100);}
        const auto activity=p.activityText();if(!activity.isEmpty()){ticker=activity.getLastCharacters(2500);if(!juce::Rectangle<int>(20,139,1198,36).contains(getMouseXYRelative()))tickerOffset+=1.2f;const int max=std::max(0,int(ticker.length()*7.5)-1050);if(tickerOffset>max)tickerOffset=0;}
        if(follow.getToggleState()&&p.transportIsPlaying())graph.begin=std::max(0.0,p.transportSeconds()-graph.span*.7);
        const double end=snapshot.frames.empty()?12:snapshot.frames.back().time;horizontal.setRangeLimits(0,std::max(end+1,graph.begin+graph.span));horizontal.setCurrentRange(graph.begin,graph.span,juce::dontSendNotification);vertical.setRangeLimits(0,127);vertical.setCurrentRange(127-graph.low-graph.range,graph.range,juce::dontSendNotification);
        updating=false;graph.repaint();repaint();
    }
};

LiveEditor::LiveEditor(PitchProcessor& p):AudioProcessorEditor(&p){
    page=std::make_unique<Page>(*this,p);extent.addAndMakeVisible(*page);viewport.setViewedComponent(&extent,false);addAndMakeVisible(viewport);setResizable(true,true);setResizeLimits(900,620,2400,1800);applyScale(float(p.parameters.getRawParameterValue("uiScale")->load())/100);
}
LiveEditor::~LiveEditor(){viewport.setViewedComponent(nullptr,false);}
void LiveEditor::resized(){viewport.setBounds(getLocalBounds());}
void LiveEditor::applyScale(float scale){
    scale=juce::jlimit(1.0f,2.0f,scale);page->setTransform(juce::AffineTransform::scale(scale));extent.setSize(int(Page::width*scale),int(Page::height*scale));
    const auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();const int w=display?std::max(900,display->userArea.getWidth()-70):1240,h=display?std::max(620,display->userArea.getHeight()-110):850;setSize(std::min(extent.getWidth()+15,w),std::min(extent.getHeight()+15,h));resized();
}
