#include "PluginEditor.h"

namespace {
const juce::Colour background(0xff10151f), panel(0xff1a2331), accent(0xff55dfba), text(0xffe7edf7);
juce::String noteGroup(int key) {
    static const char* names[] = {"Do (C)", "Do# (C#)", "Re (D)", "Re# (D#)", "Mi (E)", "Fa (F)", "Fa# (F#)", "Sol (G)", "Sol# (G#)", "La (A)", "La# (A#)", "Si (B)"};
    return names[(key % 12 + 12) % 12];
}
}
double PitchMap::length() const { return data ? data->audio.getNumSamples() / data->sampleRate : 5; }
float PitchMap::x(double t) const { return float(48 + (t - offset) / length() * (getWidth() - 56) * zoom); }
float PitchMap::y(double m) const { return float(22 + (highPitch - m) / (highPitch - lowPitch) * (getHeight() - 42)); }
juce::Rectangle<float> PitchMap::bounds(const mp::Note& n) const { return {x(n.start), y(n.centre) - 8, std::max(5.0f, x(n.end) - x(n.start)), 16}; }
juce::String PitchMap::regionInfo() const {
    if (selected < 0 || selected >= int(planned.regions.size())) return {};
    const auto& r = planned.regions[size_t(selected)];
    auto info=" | Istek "+juce::String(r.requestedCents,2)+" c | Plan "+juce::String(r.appliedCents,2)+" c";
    if(shownResult && shownResult->source==data && shownResult->fingerprint==planFingerprint && selected<int(shownResult->plan.regions.size())) {
        const auto& actual=shownResult->plan.regions[size_t(selected)];
        info+=actual.verificationAvailable?" | Olculen "+juce::String(actual.achievedCents,2)+" c | Sapma "+juce::String(actual.achievedCents-r.requestedCents,2)+" c":" | Sonuc olcumu guvenilir degil";
    }
    return info+" | Belirsizlik "+juce::String(r.uncertaintyCents,2)+" c";
}
void PitchMap::refresh() {
    for (const auto& point : processor.takeTrace()) {
        if (!liveTrace.empty() && point.time < liveTrace.back().time) liveTrace.clear();
        liveTrace.push_back(point);
    }
    if (!liveTrace.empty()) {
        const double cutoff = liveTrace.back().time - 10;
        std::erase_if(liveTrace, [cutoff](const auto& p) { return p.time < cutoff; });
    }
    const bool changed = seenRevision != processor.revision;
    if (changed) { seenRevision = processor.revision; data = processor.clip(); }
    const auto result=processor.correctionResult();
    if(data) {
        const auto settings=processor.settings(); const auto fingerprint=mp::settingsToTree(settings).toXmlString();
        if(changed || fingerprint!=planFingerprint || result!=shownResult) {
            planFingerprint=fingerprint;shownResult=result;planned=mp::planCorrection(*data,settings);
            const bool measured=settings.adaptive && result && result->source==data && result->fingerprint==fingerprint && !result->plan.regions.empty();
            proposalDisplay=settings.directShift?"DAYANAK: Kaynaga ekle: genel "+juce::String(settings.globalCents,2)+" c + kaynak notanin cent/nota ayari.\n":"DAYANAK: Senin hedef haritan; referans "+juce::String(settings.reference,1)+" Hz. Varsayilan 12 esit aralik; makam tahmin edilmez.\n";
            proposalDisplay+=!settings.adaptive && result && result->source==data && result->fingerprint==fingerprint ? "SONUC: Eski motor renderi hazir; bu modda bolgesel cikis dogrulamasi yok.\n" : measured?"SONUC: Render hazir. OLCULEN = cikis - kaynak; sapma = olculen - istek.\n":"ONERI: Henuz uygulanmadi. ONERIYI UYGULA ile sesi isle.\n";
            if(data->notes.empty())proposalDisplay+="Guvenilir merkez yok. ANALIZ ET / AYAR BELIRLE ile basla; ses degistirilmedi.\n";
            const auto& rows=measured?result->plan.regions:planned.regions;
            for(size_t i=0;i<rows.size();++i) {
                const auto& r=rows[i];const int key=int(std::lround(r.centre));const double target=r.centre+r.requestedCents/100;
                const int targetKey=int(std::lround(target));
                juce::String why;
                if(r.decision==mp::RegionDecision::Review)why=r.overlap?"Cakisan kuyruklar: dokunma":"Merkez guvensiz: dokunma";
                else if(r.decision==mp::RegionDecision::Reduced)why="Gecis kisa: hareketi korumak icin azaltildi";
                else if(r.decision==mp::RegionDecision::Shared)why="Cakisan seslere tek ortak kaydirma";
                else if(r.decision==mp::RegionDecision::None)why="Fark sifir veya belirsizlik/hassasiyet sinirinda: aynen birak";
                else why=r.discrete?"Kararli govdeyi kaydir, atagi koru":"Kararli merkezi kaydir, perde hareketini koru";
                proposalDisplay+=juce::String(r.start,2)+"-"+juce::String(r.end,2)+" s | "+mp::noteName(key)+" "+juce::String((r.centre-key)*100,2)+" c -> "+mp::noteName(targetKey)+" "+juce::String((target-targetKey)*100,2)+" c | fark "+juce::String(r.requestedCents,2)+" c | plan "+juce::String(r.appliedCents,2)+" c";
                if(measured && std::abs(r.appliedCents)>1e-9)proposalDisplay+=r.verificationAvailable?" | OLCULEN "+juce::String(r.achievedCents,2)+" c, sapma "+juce::String(r.achievedCents-r.requestedCents,2)+" c"+(r.verificationPassed?"":" (teyit edilmedi)"):" | Guvenilir cikis olcumu YOK";
                proposalDisplay+=" | merkez belirsizligi ~"+juce::String(r.uncertaintyCents,2)+" c | "+why+"\n";
            }
        }
    } else { planned={};shownResult.reset();proposalDisplay="Once WAV yukle veya ARA klibini ac. Hedef haritani ya da eklenecek cent'i sec. Analiz, oneriyi belirler; Uygula sesi degistirir."; }
    if (data && !data->notes.empty()) {
        const auto settings = processor.settings();
        double low = 128, high = 0;
        for (const auto& n : data->notes) {
            const auto target = n.centre + mp::correctionAt(*data, settings, (n.start + n.end) * 0.5);
            low = std::min({low, n.centre, target}); high = std::max({high, n.centre, target});
        }
        const double centre = (low + high) * 0.5, range = std::max(12.0, high - low + 4) / pitchZoom;
        lowPitch = centre - range / 2; highPitch = centre + range / 2;
    }
    if (changed && data && data->audio.getNumSamples() >= 1024) {
        // Pixels are computed on the CPU. A native Direct2D image would trigger
        // GPU readback/synchronisation for every setPixelAt call on Windows.
        spectrum = juce::Image(juce::Image::RGB, 512, 128, true, juce::SoftwareImageType());
        juce::dsp::FFT fft(10); std::array<float, 2048> values{};
        for (int column = 0; column < 512; ++column) {
            const int pos = int(double(column) / 511 * (data->audio.getNumSamples() - 1024));
            values.fill(0);
            for (int i = 0; i < 1024; ++i) values[size_t(i)] = data->audio.getSample(0, pos + i) * float(0.5 - 0.5 * std::cos(2 * juce::MathConstants<double>::pi * i / 1023));
            fft.performFrequencyOnlyForwardTransform(values.data());
            for (int row = 0; row < 128; ++row) {
                const double midi = 120 - row / 127.0 * 108;
                const int bin = juce::jlimit(1, 511, int(mp::midiToHz(midi) / data->sampleRate * 1024));
                const float intensity = juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(values[size_t(bin)] / 256, -90.0f) + 75) / 65);
                spectrum.setPixelAt(column, row, juce::Colour::fromFloatRGBA(0.05f + intensity * 0.15f, 0.08f + intensity * 0.25f, 0.14f + intensity * 0.27f, 1));
            }
        }
    }
    repaint();
}
void PitchMap::paint(juce::Graphics& g) {
    g.fillAll(background); const auto plot = getLocalBounds().reduced(1).toFloat();
    g.setColour(panel); g.drawRoundedRectangle(plot, 7, 1);
    if (!data) {
        if (liveTrace.empty()) { g.setColour(text); g.drawFittedText("Sonar klibini Region FX / ARA ile acin veya WAV yukleyin.\nCanli FX modunda gelen ses ve sidechain pitch'i burada cizilir.", getLocalBounds().reduced(40), juce::Justification::centred, 3); return; }
        double low = 128, high = 0;
        for (const auto& p : liveTrace) for (float pitch : {p.source, p.reference}) if (pitch > 0) { low = std::min(low, double(pitch)); high = std::max(high, double(pitch)); }
        if (high == 0) return;
        low = std::floor(low) - 2; high = std::max(low + 12, std::ceil(high) + 2);
        const double centre = (low + high) * 0.5, range = (high - low) / pitchZoom; low = centre - range / 2; high = centre + range / 2;
        const double end = liveTrace.back().time, begin = std::max(0.0, end - 10);
        auto xx = [&](double t) { return float(48 + (t - begin) / std::max(1.0, end - begin) * (getWidth() - 56)); };
        auto yy = [&](double pitch) { return float(22 + (high - pitch) / (high - low) * (getHeight() - 42)); };
        for (int note = int(low); note <= int(high); ++note) { g.setColour(text.withAlpha(0.08f)); g.drawHorizontalLine(int(yy(note)), 48, float(getWidth())); g.setFont(11); g.setColour(text.withAlpha(0.7f)); g.drawText(mp::noteName(note), 2, int(yy(note)) - 7, 42, 14, juce::Justification::centredRight); }
        for (bool reference : {false, true}) {
            juce::Path curve; bool start = true;
            for (const auto& p : liveTrace) {
                const float pitch = reference ? p.reference : p.source;
                if (pitch <= 0) { start = true; continue; }
                if (start) curve.startNewSubPath(xx(p.time), yy(pitch)); else curve.lineTo(xx(p.time), yy(pitch)); start = false;
            }
            g.setColour(reference ? juce::Colour(0xff70afff) : juce::Colour(0xffffcf73)); g.strokePath(curve, juce::PathStrokeType(reference ? 1.2f : 1.8f));
        }
        g.setColour(text); g.setFont(11); g.drawText("Sari: bu kanal / Mavi: secilen referans (oktav korunur)", 52, 2, getWidth() - 60, 18, juce::Justification::left);
        return;
    }
    g.saveState(); g.reduceClipRegion(48, 22, getWidth() - 56, getHeight() - 42);
    if (spectrum.isValid()) {
        const int top = juce::jlimit(0, 126, int((120 - highPitch) / 108 * 128));
        const int bottom = juce::jlimit(top + 1, 128, int((120 - lowPitch) / 108 * 128));
        g.drawImage(spectrum, int(x(0)), 22, int((getWidth() - 56) * zoom), getHeight() - 42, 0, top, spectrum.getWidth(), bottom - top);
    }
    for (int m = int(lowPitch); m <= int(highPitch); ++m) { g.setColour(juce::Colours::white.withAlpha(m % 12 == 0 ? 0.14f : 0.05f)); g.drawHorizontalLine(int(y(m)), 48, float(getWidth())); }
    for (int tick = 0; tick < 12; ++tick) { const double t = offset + tick * length() / zoom / 10; g.setColour(juce::Colours::white.withAlpha(0.07f)); g.drawVerticalLine(int(x(t)), 22, float(getHeight() - 20)); }
    // Actual source waveform, downsampled for display.
    juce::Path wave;
    for (int px = 48; px < getWidth() - 8; ++px) {
        const double t = offset + (px - 48.0) / (getWidth() - 56) * length() / zoom;
        const int i = juce::jlimit(0, data->audio.getNumSamples() - 1, int(t * data->sampleRate));
        const float yy = float(getHeight() - 52) - data->audio.getSample(0, i) * 22;
        if (px == 48) wave.startNewSubPath(float(px), yy); else wave.lineTo(float(px), yy);
    }
    g.setColour(accent.withAlpha(0.22f)); g.strokePath(wave, juce::PathStrokeType(1));
    const auto settings = processor.settings();
    for (int i = 0; i < int(data->notes.size()); ++i) {
        const auto& n = data->notes[size_t(i)]; auto r = bounds(n);
        g.setColour(n.confidence >= 0.9 ? accent.withAlpha(i == selected ? 0.9f : 0.5f) : juce::Colours::orange.withAlpha(0.6f));
        g.fillRoundedRectangle(r, 4); g.setColour(background);
        g.setFont(11); g.drawText(mp::noteName(n.key) + " " + juce::String((n.centre - n.key) * 100, 1) + "c", r.reduced(3, 0), juce::Justification::centredLeft, true);
        g.setColour(juce::Colour(0xff70afff).withAlpha(0.85f));
        const double correction = mp::correctionAt(*data, settings, (n.start + n.end) * 0.5);
        const float targetY = y(n.centre + (i == selected && dragCents != 0 ? dragCents / 100 : correction));
        g.drawLine(r.getX(), targetY, r.getRight(), targetY, 1.2f);
        if (std::find(selection.begin(), selection.end(), i) != selection.end()) { g.setColour(juce::Colours::white); g.drawRoundedRectangle(r, 4, 1); }
    }
    juce::Path pitch; bool first = true;
    for (auto& f : data->frames) { if (f.midi == 0) { first = true; continue; } if (first) pitch.startNewSubPath(x(f.time), y(f.midi)); else pitch.lineTo(x(f.time), y(f.midi)); first = false; }
    g.setColour(juce::Colour(0xffffcf73)); g.strokePath(pitch, juce::PathStrokeType(1.4f));
    if (settings.adaptive) { juce::Path target; bool start = true; for (const auto& f : data->frames) { if (f.midi <= 0) { start = true; continue; } const auto yy = y(f.midi + planned.at(f.time) / 100); if (start) target.startNewSubPath(x(f.time), yy); else target.lineTo(x(f.time), yy); start = false; } g.setColour(juce::Colour(0xff70afff)); g.strokePath(target, juce::PathStrokeType(1.5f)); }
    if (processor.parameters.getRawParameterValue("follow")->load() > 0) {
        juce::Path reference; bool start = true;
        for (const auto& p : liveTrace) {
            if (p.reference <= 0) { start = true; continue; }
            if (start) reference.startNewSubPath(x(p.time - data->anchorSeconds), y(p.reference)); else reference.lineTo(x(p.time - data->anchorSeconds), y(p.reference)); start = false;
        }
        g.setColour(juce::Colour(0xff70afff)); g.strokePath(reference, juce::PathStrokeType(1.4f));
    }
    g.restoreState();
    g.setFont(11); g.setColour(text.withAlpha(0.7f));
    for (int m = int(lowPitch); m <= int(highPitch); ++m) g.drawText(mp::noteName(m), 2, int(y(m)) - 7, 42, 14, juce::Justification::centredRight);
    for (int i = 0; i <= 10; ++i) { const double t = offset + i * length() / zoom / 10; g.drawText(juce::String(t, 1) + "s", int(x(t)), 1, 48, 17, juce::Justification::centredLeft); }
}
void PitchMap::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus(); selected = -1;
    if (!e.mods.isShiftDown()) selection.clear();
    if (data) for (int i = 0; i < int(data->notes.size()); ++i) if (bounds(data->notes[size_t(i)]).expanded(0, 4).contains(e.position)) { selected = i; if (std::find(selection.begin(), selection.end(), i) == selection.end()) selection.push_back(i); break; }
    dragCents = 0; repaint();
}
void PitchMap::mouseDrag(const juce::MouseEvent& e) { if (selected >= 0) { dragCents = -e.getDistanceFromDragStartY() * (highPitch - lowPitch) * 100 / (getHeight() - 42); repaint(); } }
void PitchMap::mouseUp(const juce::MouseEvent&) {
    if (data && selected >= 0 && std::abs(dragCents) > 0.01) {
        const auto& n = data->notes[size_t(selected)]; const auto s = processor.settings();
        const double initial = s.directShift ? mp::correctionAt(*data, s, (n.start + n.end) * 0.5) * 100 : (n.centre - n.key) * 100;
        setNote(initial + dragCents, s.directShift ? 1 : s.strength, true);
    }
    dragCents = 0;
}
void PitchMap::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    if (e.mods.isCtrlDown()) { pitchZoom = juce::jlimit(1.0, 32.0, pitchZoom * std::exp(w.deltaY * 2)); refresh(); }
    else if (e.mods.isShiftDown()) offset = juce::jlimit(0.0, std::max(0.0, length() - length() / zoom), offset - w.deltaY * length() / zoom);
    else { zoom = juce::jlimit(1.0, 32.0, zoom * std::exp(w.deltaY * 2)); offset = std::min(offset, length() - length() / zoom); }
    repaint();
}
bool PitchMap::keyPressed(const juce::KeyPress& k) { if (k.getModifiers().isCommandDown() && (k.getTextCharacter() == 'z' || k.getTextCharacter() == 'y')) { processor.undo(k.getTextCharacter() == 'y' || k.getModifiers().isShiftDown()); return true; } return false; }
void PitchMap::setNote(double cents, double strength, bool detach) {
    if (!data || selected < 0 || selected >= int(data->notes.size())) return;
    auto notes = data->notes; auto& n = notes[size_t(selected)]; n.detached = detach; n.target = juce::jlimit(-1400.0, 1400.0, cents); n.strength = juce::jlimit(0.0, 1.0, strength); processor.editNotes(std::move(notes));
}
void PitchMap::split() {
    if (!data || selected < 0) return; auto notes = data->notes; auto n = notes[size_t(selected)]; const double mid = (n.start + n.end) * 0.5;
    notes[size_t(selected)].end = mid; n.start = mid; notes.insert(notes.begin() + selected + 1, n); processor.editNotes(std::move(notes));
}
void PitchMap::merge() {
    if (!data || selection.size() < 2) return;
    std::sort(selection.begin(), selection.end()); const int a = selection.front(), b = selection.back();
    auto notes = data->notes; double sum = 0, weight = 0;
    for (int i = a; i <= b; ++i) { const auto& n = notes[size_t(i)]; sum += n.centre * (n.end - n.start); weight += n.end - n.start; }
    notes[size_t(a)].end = notes[size_t(b)].end; notes[size_t(a)].centre = sum / weight; notes[size_t(a)].key = int(std::lround(sum / weight));
    notes.erase(notes.begin() + a + 1, notes.begin() + b + 1); selected = a; selection = {a}; processor.editNotes(std::move(notes));
}

PitchEditor::PitchEditor(PitchProcessor& p) : AudioProcessorEditor(p), processor(p), map(p), progressBar(progressValue) {
    theme.setColour(juce::ResizableWindow::backgroundColourId, background);
    theme.setColour(juce::Slider::thumbColourId, accent); theme.setColour(juce::Slider::trackColourId, accent);
    theme.setColour(juce::TextButton::buttonColourId, panel); theme.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff227966));
    theme.setColour(juce::TextButton::textColourOffId, text); theme.setColour(juce::TextButton::textColourOnId, text);
    theme.setColour(juce::Slider::textBoxTextColourId, text); theme.setColour(juce::Slider::textBoxOutlineColourId, panel);
    setLookAndFeel(&theme); setWantsKeyboardFocus(true);
    title.setText("PITCHMAP PRO 3.0.1  /  AYAR -> SONUC", juce::dontSendNotification); title.setFont(juce::FontOptions(23, juce::Font::bold)); title.setColour(juce::Label::textColourId, text); addAndMakeVisible(title);
    addAndMakeVisible(map); addAndMakeVisible(statusLabel); addAndMakeVisible(noteLabel); addAndMakeVisible(progressBar); addAndMakeVisible(liveLabel);
    liveLabel.setColour(juce::Label::textColourId, accent);
    statusLabel.setColour(juce::Label::textColourId, text); noteLabel.setColour(juce::Label::textColourId, text);
    button("Load WAV", [this]{ choose(0); });
    button("Capture", [this]{ processor.capture(!processor.capturing); });
    button("ANALIZ ET / AYAR BELIRLE", [this]{ processor.analyse(); }); button("Apply map", [this]{ processor.applyCorrection(); });
    button("Export WAV", [this]{ choose(1); }); button("MIDI", [this]{ choose(2); }); button("Scala", [this]{ choose(3); });
    button("Save preset", [this]{ choose(4); }); button("Load preset", [this]{ choose(5); });
    button("Preview", [this]{ processor.preview(!processor.previewing); });
    for (auto pair : {std::pair{"Use note map", "useRender"}, {"A: Original", "original"}, {"Bypass", "bypass"}, {"Formants", "formants"}, {"Octave mode", "octave"}}) {
        auto& b = button(pair.first, []{}); b.setClickingTogglesState(true);
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, pair.second, b));
    }
    button("Split", [this]{ map.split(); }); button("Merge", [this]{ map.merge(); }); button("Undo", [this]{ processor.undo(false); }); button("Redo", [this]{ processor.undo(true); });
    button("Set note", [this]{ map.setNote(noteCents.getText().getDoubleValue(), noteStrength.getText().getDoubleValue() / 100, true); });
    button("Rejoin group", [this]{ map.setNote(0, 1, false); });
    button("ONERIYI UYGULA", [this]{ processor.applyCorrection(); });
    buttons.back()->setTooltip("Klip haritasini veya canli kanali alt hedef tonlarina uydurur. Duzeltmeyi etkinlestirir; yuzde gerekmez.");
    button("Gelismis ayarlar", [this]{ advanced = !advanced; resized(); });
    button("Ayarlari sifirla", [this]{ processor.setParameter("globalCents",0); for (int i = 0; i < 12; ++i) { processor.setParameter("cent" + juce::String(i), 0); processor.setParameter("step" + juce::String(i), 0); } });
    button("Islemi iptal et", [this]{ processor.cancelWork(); });
    adaptiveSwitch.setButtonText("Dogal bolgesel (klip / offline)"); addAndMakeVisible(adaptiveSwitch);
    adaptiveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "adaptive", adaptiveSwitch);
    adaptiveSwitch.onClick = [this]{
        if (adaptiveSwitch.getToggleState()) {
            // Explicitly choosing the new workflow must not leave hidden legacy
            // octave/percentage controls steering the visible pitch-class keys.
            processor.setParameter("octave",0);
            if (processor.parameters.getRawParameterValue("direct")->load()<0.5f)
                processor.setParameter("referenceMatch",1);
        }
        attachGroups(); resized();
    };
    minimumSwitch.setButtonText("Gereksiz islemden kacin"); addAndMakeVisible(minimumSwitch);
    minimumAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "minimumIntervention", minimumSwitch);
    precisionControl.setRange(0.1,25,0.1); precisionControl.setTextValueSuffix(" c"); precisionControl.setTextBoxStyle(juce::Slider::TextBoxRight,false,65,24); addAndMakeVisible(precisionControl);
    precisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"precision",precisionControl);
    precisionLabel.setText("Hedef merkez hassasiyeti",juce::dontSendNotification); addAndMakeVisible(precisionLabel);
    directSwitch.setButtonText("Dogrudan cent / nota (yuzdesiz)"); addAndMakeVisible(directSwitch);
    directAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "direct", directSwitch);
    directSwitch.onClick = [this]{ processor.setParameter("referenceMatch", directSwitch.getToggleState() ? 0 : 1); processor.setParameter("follow", 0); resized(); };
    buttons[10]->setTooltip("Off: process incoming DAW audio directly. On: play the edited loaded/captured note map on the DAW timeline.");
    for (auto* ed : {&noteCents, &noteStrength}) { addAndMakeVisible(*ed); ed->setInputRestrictions(10, "-0123456789."); }
    noteCents.setText("0"); noteCents.setTextToShowWhenEmpty("Cent shift", juce::Colours::grey);
    noteStrength.setText("100"); noteStrength.setTextToShowWhenEmpty("Strength %", juce::Colours::grey);
    addAndMakeVisible(octaveSelector); for (int i = -1; i <= 9; ++i) octaveSelector.addItem("Octave " + juce::String(i), i + 2); octaveSelector.setSelectedId(5);
    octaveSelector.onChange = [this]{ attachGroups(); };
    for (int i = 0; i < 12; ++i) {
        slider("cent" + juce::String(i), noteGroup(i) + "  /  ince cent", -200, 200, 0.01, " c");
        sliders.back()->setSliderStyle(juce::Slider::IncDecButtons); sliders.back()->setIncDecButtonsMode(juce::Slider::incDecButtonsDraggable_Horizontal); sliders.back()->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 22);
        auto destination = std::make_unique<juce::ComboBox>();
        for (int step = -12; step <= 12; ++step) destination->addItem(noteGroup(i + step) + "  " + (step > 0 ? "+" : "") + juce::String(step * 100) + " c", step + 13);
        destination->setTooltip("Hedef nota. Do -> Re = +200 cent; Do -> Do# = +100 cent. Alt siradaki HEDEF notanin cent akordu kullanilir.");
        addAndMakeVisible(*destination);
        destinationAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters, "step" + juce::String(i), *destination));
        noteDestinations.push_back(std::move(destination));
        auto& referenceKey = referenceKeys[size_t(i)]; referenceKey.setButtonText(noteGroup(i)); addAndMakeVisible(referenceKey);
        referenceKey.onClick = [this, i]{ selectedReference = i; processor.auditionTarget(i, referenceOctave.getSelectedId() - 2,map.selected); updatePairing(); };
        auto& sourceKey = sourceKeys[size_t(i)]; sourceKey.setButtonText(noteGroup(i)); addAndMakeVisible(sourceKey);
        sourceKey.onClick = [this, i]{
            selectedSource = i;
            const int note = processor.auditionSource(i); map.selected = note;
            if (auto c = processor.clip(); c && note >= 0) referenceOctave.setSelectedId(c->notes[size_t(note)].key / 12 + 1, juce::dontSendNotification);
            updatePairing();
        };
    }
    addAndMakeVisible(referenceSource); referenceSource.addItem("Program tonlari / klavye", 1); referenceSource.addItem("Sidechain: cent uyumu", 2); referenceSource.addItem("Sidechain: nota + cent", 3);
    referenceSource.setSelectedId(int(processor.parameters.getRawParameterValue("follow")->load()) + 1, juce::dontSendNotification);
    referenceSource.onChange = [this]{ processor.setParameter("follow", float(referenceSource.getSelectedId() - 1)); updatePairing(); };
    addAndMakeVisible(referenceOctave); for (int octave = -1; octave <= 9; ++octave) referenceOctave.addItem("Ton oktavi " + juce::String(octave), octave + 2); referenceOctave.setSelectedId(6, juce::dontSendNotification);
    addAndMakeVisible(pairingLabel); pairingLabel.setColour(juce::Label::textColourId, accent);
    addAndMakeVisible(pairButton); pairButton.onClick = [this]{
        if (selectedSource < 0 || processor.settings().directShift) return;
        int step = selectedReference - selectedSource; while (step > 6) step -= 12; while (step < -6) step += 12;
        processor.setParameter("step" + juce::String(selectedSource), float(step)); processor.setParameter("referenceMatch", 1); processor.setParameter("direct", 0); processor.setParameter("follow", 0);
        referenceSource.setSelectedId(1, juce::dontSendNotification); updatePairing();
    };
    correctionGoal.addItem("Hedef / makam notalarina akortla",1);correctionGoal.addItem("Kaynaga cent / nota ekle",2);addAndMakeVisible(correctionGoal);
    correctionGoal.onChange=[this]{const bool relative=correctionGoal.getSelectedId()==2;processor.setParameter("direct",relative?1:0);processor.setParameter("referenceMatch",relative?0:1);processor.setParameter("follow",0);processor.setParameter("octave",0);attachGroups();updatePairing();resized();};
    addAndMakeVisible(allCents);allCents.setTextBoxStyle(juce::Slider::TextBoxRight,false,78,24);allCents.setTextValueSuffix(" c");
    allCentsAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"globalCents",allCents);
    allCentsLabel.setText("Kaydin tamamina eklenecek",juce::dontSendNotification);addAndMakeVisible(allCentsLabel);
    addAndMakeVisible(clipStart);clipStart.setTextBoxStyle(juce::Slider::TextBoxRight,false,85,24);clipStart.setTextValueSuffix(" s");
    clipStartAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"offset",clipStart);
    clipStartLabel.setText("WAV baslangici",juce::dontSendNotification);addAndMakeVisible(clipStartLabel);
    proposalBox.setColour(juce::TextEditor::backgroundColourId,panel);proposalBox.setColour(juce::TextEditor::textColourId,text);
    proposalBox.setMultiLine(true);proposalBox.setReadOnly(true);proposalBox.setScrollbarsShown(true);proposalBox.setFont(juce::FontOptions(13));addAndMakeVisible(proposalBox);
    addAndMakeVisible(stopListening); stopListening.onClick = [this]{ processor.stopAudition(); };
    slider("quantize", "QUANTIZE  0-100%", 0, 1, 0.001, ""); slider("tightness", "Intonation tightness", 0, 1, 0.001, "");
    slider("vibrato", "Vibrato preservation", 0, 1, 0.001, ""); slider("smoothing", "Transition smoothing", 0, 250, 1, " ms");
    slider("humanize", "Humanize maximum", 0, 50, 0.1, " c"); slider("seed", "Humanize seed", 1, 10000, 1, "");
    slider("reference", "Reference tuning", 400, 480, 0.1, " Hz"); slider("wet", "Wet / Dry", 0, 1, 0.001, "");
    slider("offset", "Timeline offset", 0, 36000, 0.001, " s");
    for (int i = 0; i < 12; ++i) slider("gq" + juce::String(i), "Group strength", 0, 1, 0.001, "");
    advanced = processor.parameters.getRawParameterValue("referenceMatch")->load() < 0.5f && processor.parameters.getRawParameterValue("direct")->load() < 0.5f;
    setResizable(true, true); setResizeLimits(1060, 680, 1800, 1200); const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay(); setSize(display ? juce::jlimit(1060,1240,display->userArea.getWidth()-80) : 1240, display ? juce::jlimit(680,820,display->userArea.getHeight()-80) : 820); updatePairing(); map.refresh(); timerCallback(); startTimerHz(10);
}
PitchEditor::~PitchEditor() { stopTimer(); processor.stopAudition(); setLookAndFeel(nullptr); }
void PitchEditor::updatePairing() {
    pairingLabel.setText(processor.settings().directShift ? "Goreli: genel cent + kaynak notanin ayari. Alt tus, bu kaynaktan hesaplanan hedef tonu calar." : selectedSource < 0 ? "Ust: gercek kaynak. Alt: hedef tonu. Hesabin dayanak ve sonucunu asagida gorun." : "Kaynak " + noteGroup(selectedSource) + " -> Referans " + noteGroup(selectedReference) + ". Eslestir, sonra Uygula.", juce::dontSendNotification);
    pairButton.setEnabled(selectedSource >= 0 && !processor.settings().directShift);
    for (int i = 0; i < 12; ++i) { referenceKeys[size_t(i)].setToggleState(i == selectedReference, juce::dontSendNotification); sourceKeys[size_t(i)].setToggleState(i == selectedSource, juce::dontSendNotification); }
}
juce::TextButton& PitchEditor::button(juce::String label, std::function<void()> action) {
    auto b = std::make_unique<juce::TextButton>(label); b->onClick = std::move(action); addAndMakeVisible(*b); buttons.push_back(std::move(b)); return *buttons.back();
}
void PitchEditor::slider(juce::String id, juce::String name, double min, double max, double step, const juce::String& suffix) {
    auto s = std::make_unique<juce::Slider>(); s->setSliderStyle(juce::Slider::LinearHorizontal); s->setRange(min, max, step); s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 75, 22); s->setTextValueSuffix(suffix); addAndMakeVisible(*s);
    if (id.startsWith("cent")) s->setTooltip("Alttaki referans tonunun ince akordu. +5 c standart notanin 5 cent tizidir. Kaynak Do'yu Re referansina eslestirirseniz Re'nin buradaki akordu kullanilir. Yuzde gerekmez.");
    auto l = std::make_unique<juce::Label>(); l->setText(name, juce::dontSendNotification); l->setColour(juce::Label::textColourId, text.withAlpha(0.8f)); addAndMakeVisible(*l);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, id, *s)); sliders.push_back(std::move(s)); labels.push_back(std::move(l));
}
void PitchEditor::attachGroups() {
    const bool octave = processor.parameters.getRawParameterValue("octave")->load() > 0.5f;
    for (int i = 0; i < 12; ++i) {
        sliderAttachments[size_t(i)].reset();
        const int key = (octaveSelector.getSelectedId() - 1) * 12 + i;
        const bool supported = !octave || key < 128;
        sliders[size_t(i)]->setEnabled(supported);
        labels[size_t(i)]->setText(octave ? (supported ? mp::noteName(key) : "Outside MIDI range") : noteGroup(i) + " / ince cent", juce::dontSendNotification);
        if (supported) {
            const auto id = octave ? "oct" + juce::String(key) : "cent" + juce::String(i);
            sliderAttachments[size_t(i)] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, id, *sliders[size_t(i)]);
        }
    }
}
void PitchEditor::paint(juce::Graphics& g) {
    g.fillAll(background);
    if (advanced) { g.setColour(panel); g.fillRoundedRectangle(float(getWidth() - 270), 152, 252, float(getHeight() - 230), 9); }
    g.setColour(text.withAlpha(0.55f)); g.drawText("Scroll: zaman zoom  /  Ctrl+scroll: cent zoom  /  Shift+scroll: kaydir  /  Sari: kaynak  /  Mavi: planlanan hedef veya canli referans", 24, getHeight() - 62, getWidth() - 48, 18, juce::Justification::left);
}
void PitchEditor::resized() {
    const bool direct = processor.parameters.getRawParameterValue("direct")->load() > 0.5f;
    const int contentWidth = getWidth() - (advanced ? 306 : 44);
    title.setBounds(20, 10, getWidth() - 290, 34);
    buttons[21]->setBounds(getWidth() - 256, 14, 232, 30);
    liveLabel.setBounds(20, 46, getWidth() - 40, 22);
    correctionGoal.setBounds(22,112,290,26);referenceSource.setBounds(320,112,230,26);referenceOctave.setBounds(getWidth()-160,112,138,26);stopListening.setBounds(778,112,std::min(148,std::max(72,getWidth()-946)),26);
    buttons[10]->setVisible(advanced && !processor.araConnected()); buttons[10]->setBounds(getWidth()-258, getHeight()-100, 232, 24);
    buttons[11]->setBounds(558,112,102,26);buttons[12]->setBounds(668,112,102,26);
    buttons[23]->setBounds(getWidth() - 190, getHeight() - 126, 168, 25); buttons[22]->setBounds(getWidth() - 190, 76, 168, 28);
    int toolbar = 0; const int toolbarWidth = (getWidth() - 230) / (advanced ? 9 : 4);
    for (int i = 0; i < 10; ++i) {
        const bool visible = i != 3 && (advanced || i == 0 || i == 2 || i == 4 || i == 9);
        buttons[size_t(i)]->setVisible(visible);
        if (visible) buttons[size_t(i)]->setBounds(22 + toolbar++ * toolbarWidth, 76, toolbarWidth - 5, 28);
    }
    pairingLabel.setBounds(22, 252, contentWidth - 150, 28); pairButton.setBounds(22 + contentWidth - 142, 252, 138, 28);
    const int available=getHeight()-518,chart=std::max(80,int(available*.55));
    map.setBounds(22,288,contentWidth,chart);proposalBox.setBounds(22,300+chart,contentWidth,std::max(50,available-chart-12));
    allCents.setVisible(direct);allCentsLabel.setVisible(direct);allCentsLabel.setBounds(22,226,188,24);allCents.setBounds(212,226,250,24);
    const bool wav=processor.clip()!=nullptr && !processor.araConnected();clipStart.setVisible(wav);clipStartLabel.setVisible(wav);
    clipStartLabel.setBounds(22+contentWidth-290,226,116,24);clipStart.setBounds(22+contentWidth-170,226,170,24);
    pairButton.setEnabled(selectedSource>=0 && !direct);pairButton.setButtonText(direct?"GORELI CENT MODU":"ESLESTIR");
    for(int i=0;i<12;++i)sliders[size_t(i)]->setTooltip(direct?"Bu KAYNAK notaya eklenecek cent. Genel cent buna eklenir. Alt tusa basinca analiz edilen kaynagin hesaplanan hedef tonu duyulur.":"Bu HEDEF notanin standart perdeye gore cent akordu. Kaynak-hedef farkini oneri panelinden oku.");
    const int right = getWidth() - 258;
    const bool adaptive = processor.parameters.getRawParameterValue("adaptive")->load() > 0.5f;
    adaptiveSwitch.setBounds(advanced ? right : 22, advanced ? 188 : getHeight()-126, advanced ? 232 : 285, 26);
    minimumSwitch.setVisible(advanced && adaptive); minimumSwitch.setBounds(right,256,232,26);
    precisionLabel.setVisible(advanced && adaptive); precisionLabel.setBounds(right,290,232,22);
    precisionControl.setVisible(advanced && adaptive); precisionControl.setBounds(right,313,232,28);
    buttons[24]->setBounds(getWidth()-420,getHeight()-36,190,26);
    directSwitch.setVisible(advanced); directSwitch.setBounds(right, 156, 232, 26);
    buttons[13]->setVisible(advanced); buttons[13]->setBounds(right, 220, 112, 26);
    buttons[14]->setVisible(advanced && !adaptive); buttons[14]->setBounds(right, 220, 112, 26);
    octaveSelector.setVisible(advanced && !adaptive); octaveSelector.setBounds(right + 119, 220, 113, 26);
    const bool legacy = !direct && processor.parameters.getRawParameterValue("referenceMatch")->load() < 0.5f;
    for (int i = 12; i < 21; ++i) {
        const int y = adaptive && i >= 18 ? 360 + (i-18)*60 : 256 + (i - 12) * 49;
        const bool visible = advanced && (!adaptive || i >= 18);
        labels[size_t(i)]->setVisible(visible); sliders[size_t(i)]->setVisible(visible);
        sliders[size_t(i)]->setEnabled(legacy || i == 15 || i == 18 || i == 19 || i == 20);
        labels[size_t(i)]->setBounds(right, y, 230, 19); sliders[size_t(i)]->setBounds(right, y + 19, 230, 27);
    }
    const int width = contentWidth / 12;
    for (int i = 0; i < 12; ++i) {
        const int xx = 22 + i * width;
        labels[size_t(i)]->setVisible(false);
        sourceKeys[size_t(i)].setBounds(xx, 152, width - 5, 40);
        sliders[size_t(i)]->setBounds(xx, getHeight() - 182, width - 5, 48);
        sliders[size_t(i)]->setTextBoxStyle(juce::Slider::TextBoxBelow, false, width - 7, 23);
        referenceKeys[size_t(i)].setBounds(xx, getHeight() - 224, width - 5, 36);
        noteDestinations[size_t(i)]->setVisible(true); noteDestinations[size_t(i)]->setBounds(xx, 198, width - 5, 25);
        labels[size_t(i + 21)]->setVisible(false); sliders[size_t(i + 21)]->setVisible(false);
    }
    const int editing = getHeight() - 126;
    for (int i = 15; i <= 20; ++i) buttons[size_t(i)]->setVisible(advanced);
    for (int i = 15; i < 19; ++i) buttons[size_t(i)]->setBounds(22 + (i - 15) * 77, editing, 71, 25);
    noteCents.setVisible(advanced); noteCents.setTooltip("Yalniz secili nota: hedef notanin standart perdesine gore cent (referans modunda), kaynaga eklenen cent (dogrudan modda).");
    noteCents.setBounds(335, editing, 67, 25); noteStrength.setVisible(advanced && legacy); noteStrength.setBounds(408, editing, 67, 25);
    buttons[19]->setBounds(482, editing, 81, 25); buttons[20]->setBounds(570, editing, 106, 25);
    noteLabel.setBounds(22, editing + 31, contentWidth, 22);
    progressBar.setBounds(getWidth() - 258, getHeight() - 66, 232, 14);
    statusLabel.setBounds(22, getHeight() - 36, getWidth() - (processor.busy ? 450 : 44), 26);
    repaint();
}
void PitchEditor::timerCallback() {
    map.refresh();
    if(proposalBox.getText()!=map.proposalText())proposalBox.setText(map.proposalText(),false);
    const bool relativeNow=processor.settings().directShift;
    if(correctionGoal.getSelectedId()!=(relativeNow?2:1)){correctionGoal.setSelectedId(relativeNow?2:1,juce::dontSendNotification);resized();}
    progressValue = processor.busy ? processor.progress.load() : 0; progressBar.setVisible(processor.busy);
    const auto param = [this](const char* id) { return processor.parameters.getRawParameterValue(id)->load(); };
    const bool mapMode = param("useRender") > 0.5f;
    const bool direct = param("direct") > 0.5f;
    const bool match = param("referenceMatch") > 0.5f;
    const int follow = int(param("follow"));
    const float detected = processor.liveMidi();
    const auto inputDb = juce::Decibels::gainToDecibels(processor.liveInputLevel(), -100.0f);
    juce::String live = processor.araConnected() ? "ARA / SONAR KLIPLERI" : mapMode ? "KAYNAK KLIP" : "CANLI KANAL";
    live += "  |  Input " + juce::String(inputDb, 0) + " dB";
    if (!mapMode && detected > 0) {
        const int key = juce::jlimit(0, 127, int(std::lround(detected)));
        const auto id = param("octave") > 0.5f ? "oct" + juce::String(key) : "cent" + juce::String(key % 12);
        const auto fine = processor.parameters.getRawParameterValue(id)->load();
        const int step = int(processor.parameters.getRawParameterValue("step" + juce::String(key % 12))->load());
        live += "  |  " + mp::noteName(key);
        if (direct || match) live += " -> " + mp::noteName(key + step) + "  |  Uygulanan kaydirma " + juce::String(processor.liveCents(), 1) + " c";
        else live += "  |  Target " + juce::String(fine, 1) + " c  |  Shift " + juce::String(processor.liveCents(), 1) + " c";
    }
    if (follow > 0) live += processor.referencePitch() > 0 ? "  |  Sidechain referansi algilandi" : "  |  Sidechain referansi YOK / belirsiz";
    else if (direct) live += "  |  Kaynaga ekle: genel " + juce::String(param("globalCents"),2) + " c + nota ayari";
    else if (match) live += "  |  Hedef tonlara akortla";
    else live += "  |  Legacy target / Quantize " + juce::String(param("quantize") * 100, 0) + "%";
    if (param("bypass") > 0.5f || param("original") > 0.5f) live += "  |  ORIGINAL / BYPASS";
    else if (param("enabled") < 0.5f) live += "  |  Duzeltme kapali: ONERIYI UYGULA";
    else if (!direct && !match && param("quantize") == 0) live += "  |  Target correction OFF";
    if (param("adaptive") > 0.5f) live += mapMode || processor.araConnected() ? "  |  Dogal bolgesel plan" : "  |  Bolgesel plan icin klip analizi gerekir";
    liveLabel.setText(live, juce::dontSendNotification);
    for (int i = 0; i < 12; ++i) labels[size_t(i)]->setColour(juce::Label::textColourId, !mapMode && detected > 0 && (int(std::lround(detected)) % 12) == i ? accent : text.withAlpha(0.8f));
    buttons[10]->setButtonText(mapMode ? "Source: Note map" : "Source: Live");
    const auto c = processor.clip();
    buttons[21]->setButtonText(follow > 0 ? "REF. KANALA UYDUR" : "ONERIYI UYGULA");
    buttons[24]->setVisible(processor.busy);
    buttons[21]->setEnabled(!processor.busy && !processor.capturing);
    referenceSource.setSelectedId(follow + 1, juce::dontSendNotification);
    for (int group = 0; group < 12; ++group) {
        bool found = false;
        if (c) for (const auto& note : c->notes) if (note.key % 12 == group) { found = true; break; }
        sourceKeys[size_t(group)].setEnabled(found);
    }
    statusLabel.setText(processor.status(), juce::dontSendNotification);
    buttons[1]->setButtonText(processor.capturing ? "Stop capture" : "Capture"); buttons[9]->setButtonText(processor.previewing ? "DINLEMEYI DURDUR" : "DINLE / A-B");
    for (int i = 0; i < 10; ++i) buttons[size_t(i)]->setEnabled(!processor.busy && (!processor.capturing || i == 1));
    buttons[9]->setEnabled(!processor.busy && c!=nullptr);
    buttons[1]->setEnabled(!processor.araConnected() && !processor.busy);
    if (c && map.selected >= 0 && map.selected < int(c->notes.size())) {
        const auto& n = c->notes[size_t(map.selected)];
        noteLabel.setText(mp::noteName(n.key) + "  |  " + juce::String(mp::midiToHz(n.centre), 2) + " Hz  |  " + juce::String((n.centre - n.key) * 100, 2) + " cents  |  confidence " + juce::String(n.confidence * 100, 0) + "%  |  " + (n.detached ? "independent" : "grouped"), juce::dontSendNotification);
        noteLabel.setTooltip(noteLabel.getText() + map.regionInfo());
        if (param("adaptive") > 0.5f) noteLabel.setText(noteLabel.getText() + map.regionInfo(), juce::dontSendNotification);
        if (lastSelected != map.selected) { noteCents.setText(juce::String(n.detached ? n.target : 0, 2)); noteStrength.setText(juce::String(n.detached ? n.strength * 100 : processor.settings().strength * 100, 1)); lastSelected = map.selected; }
    } else noteLabel.setText(direct ? "Kaynak tonu dinle -> genel veya nota cent degerini gir -> oneride farki oku -> Uygula -> DINLE / A-B -> Sonar Bounce." : "UST: kaynak. ALT: hedef tonlari. Hedef cent ayarla -> gerekirse Eslestir -> oneride farki oku -> Uygula -> DINLE / A-B.", juce::dontSendNotification);
    const bool octave = processor.parameters.getRawParameterValue("octave")->load() > 0.5f;
    if (octave != octaveSelector.isEnabled()) { octaveSelector.setEnabled(octave); attachGroups(); }
    map.repaint();
}
bool PitchEditor::keyPressed(const juce::KeyPress& key) { return map.keyPressed(key); }
void PitchEditor::choose(int action) {
    static const char* titles[] = {"Load audio", "Export corrected WAV", "Export MIDI", "Import 12-step Scala", "Save complete preset", "Load preset"};
    static const char* masks[] = {"*.wav;*.aif;*.aiff;*.flac", "*.wav", "*.mid", "*.scl", "*.mpmap", "*.mpmap"};
    chooser = std::make_unique<juce::FileChooser>(titles[action], juce::File{}, masks[action]);
    const bool save = action == 1 || action == 2 || action == 4;
    juce::Component::SafePointer<PitchEditor> safe(this);
    chooser->launchAsync((save ? juce::FileBrowserComponent::saveMode : juce::FileBrowserComponent::openMode) | juce::FileBrowserComponent::canSelectFiles, [safe, action](const juce::FileChooser& fc) {
        if (!safe) return; auto file = fc.getResult(); if (file == juce::File{}) return;
        switch (action) { case 0: safe->processor.load(file); break; case 1: safe->processor.exportAudio(file.withFileExtension("wav")); break; case 2: safe->processor.exportMidi(file.withFileExtension("mid")); break; case 3: safe->processor.importScale(file); break; case 4: safe->processor.preset(file.withFileExtension("mpmap"), true); break; case 5: safe->processor.preset(file, false); break; }
    });
}
