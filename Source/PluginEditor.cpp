#include "PluginEditor.h"

namespace { const juce::Colour background(0xff10151f), panel(0xff1a2331), accent(0xff55dfba), text(0xffe7edf7); }
double PitchMap::length() const { return data ? data->audio.getNumSamples() / data->sampleRate : 5; }
float PitchMap::x(double t) const { return float(48 + (t - offset) / length() * (getWidth() - 56) * zoom); }
float PitchMap::y(double m) const { return float(22 + (highPitch - m) / (highPitch - lowPitch) * (getHeight() - 42)); }
juce::Rectangle<float> PitchMap::bounds(const mp::Note& n) const { return {x(n.start), y(n.centre) - 8, std::max(5.0f, x(n.end) - x(n.start)), 16}; }
void PitchMap::refresh() {
    if (seenRevision == processor.revision) return;
    seenRevision = processor.revision; data = processor.clip();
    if (data && !data->notes.empty()) {
        auto [lo, hi] = std::minmax_element(data->notes.begin(), data->notes.end(), [](auto& a, auto& b) { return a.centre < b.centre; });
        lowPitch = std::floor(lo->centre) - 5; highPitch = std::max(lowPitch + 12, std::ceil(hi->centre) + 5);
    }
    if (data && data->audio.getNumSamples() >= 1024) {
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
                const double midi = highPitch - row / 127.0 * (highPitch - lowPitch);
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
    if (!data) { g.setColour(text); g.drawFittedText("Load a WAV or capture a track, then Analyze\nMonophonic audio | no hardware required", getLocalBounds().reduced(40), juce::Justification::centred, 3); return; }
    g.saveState(); g.reduceClipRegion(48, 22, getWidth() - 56, getHeight() - 42);
    if (spectrum.isValid()) g.drawImage(spectrum, int(x(0)), 22, int((getWidth() - 56) * zoom), getHeight() - 42, 0, 0, spectrum.getWidth(), spectrum.getHeight());
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
        g.setColour(juce::Colours::white.withAlpha(0.65f));
        const double correction = mp::correctionAt(*data, settings, (n.start + n.end) * 0.5);
        const float targetY = y(n.centre + (i == selected && dragCents != 0 ? dragCents / 100 : correction));
        g.drawLine(r.getX(), targetY, r.getRight(), targetY, 1.2f);
        if (std::find(selection.begin(), selection.end(), i) != selection.end()) { g.setColour(juce::Colours::white); g.drawRoundedRectangle(r, 4, 1); }
    }
    juce::Path pitch; bool first = true;
    for (auto& f : data->frames) { if (f.midi == 0) { first = true; continue; } if (first) pitch.startNewSubPath(x(f.time), y(f.midi)); else pitch.lineTo(x(f.time), y(f.midi)); first = false; }
    g.setColour(juce::Colour(0xffffcf73)); g.strokePath(pitch, juce::PathStrokeType(1.4f)); g.restoreState();
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
    if (data && selected >= 0 && std::abs(dragCents) > 0.01) { const auto& n = data->notes[size_t(selected)]; setNote((n.centre - n.key) * 100 + dragCents, processor.settings().strength, true); }
    dragCents = 0;
}
void PitchMap::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    if (e.mods.isShiftDown()) offset = juce::jlimit(0.0, std::max(0.0, length() - length() / zoom), offset - w.deltaY * length() / zoom);
    else { zoom = juce::jlimit(1.0, 32.0, zoom * std::exp(w.deltaY * 2)); offset = std::min(offset, length() - length() / zoom); }
    repaint();
}
bool PitchMap::keyPressed(const juce::KeyPress& k) { if (k.getModifiers().isCommandDown() && (k.getTextCharacter() == 'z' || k.getTextCharacter() == 'y')) { processor.undo(k.getTextCharacter() == 'y' || k.getModifiers().isShiftDown()); return true; } return false; }
void PitchMap::setNote(double cents, double strength, bool detach) {
    if (!data || selected < 0 || selected >= int(data->notes.size())) return;
    auto notes = data->notes; auto& n = notes[size_t(selected)]; n.detached = detach; n.target = juce::jlimit(-200.0, 200.0, cents); n.strength = juce::jlimit(0.0, 1.0, strength); processor.editNotes(std::move(notes));
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
    title.setText("MICROTONAL  /  PITCH MAP PRO", juce::dontSendNotification); title.setFont(juce::FontOptions(23, juce::Font::bold)); title.setColour(juce::Label::textColourId, text); addAndMakeVisible(title);
    addAndMakeVisible(map); addAndMakeVisible(statusLabel); addAndMakeVisible(noteLabel); addAndMakeVisible(progressBar);
    statusLabel.setColour(juce::Label::textColourId, text); noteLabel.setColour(juce::Label::textColourId, text);
    button("Load WAV", [this]{ choose(0); });
    button("Capture", [this]{ processor.capture(!processor.capturing); });
    button("Analyze", [this]{ processor.analyse(); }); button("Render", [this]{ processor.render(); });
    button("Export WAV", [this]{ choose(1); }); button("MIDI", [this]{ choose(2); }); button("Scala", [this]{ choose(3); });
    button("Save preset", [this]{ choose(4); }); button("Load preset", [this]{ choose(5); });
    button("Preview", [this]{ processor.preview(!processor.previewing); });
    for (auto pair : {std::pair{"DAW Render", "useRender"}, {"A: Original", "original"}, {"Bypass", "bypass"}, {"Formants", "formants"}, {"Octave mode", "octave"}}) {
        auto& b = button(pair.first, []{}); b.setClickingTogglesState(true);
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, pair.second, b));
    }
    button("Split", [this]{ map.split(); }); button("Merge", [this]{ map.merge(); }); button("Undo", [this]{ processor.undo(false); }); button("Redo", [this]{ processor.undo(true); });
    button("Set note", [this]{ map.setNote(noteCents.getText().getDoubleValue(), noteStrength.getText().getDoubleValue() / 100, true); });
    button("Rejoin group", [this]{ map.setNote(0, 1, false); });
    for (auto* ed : {&noteCents, &noteStrength}) { addAndMakeVisible(*ed); ed->setInputRestrictions(10, "-0123456789."); }
    noteCents.setText("0"); noteCents.setTextToShowWhenEmpty("Target cents", juce::Colours::grey);
    noteStrength.setText("100"); noteStrength.setTextToShowWhenEmpty("Strength %", juce::Colours::grey);
    addAndMakeVisible(octaveSelector); for (int i = -1; i <= 9; ++i) octaveSelector.addItem("Octave " + juce::String(i), i + 2); octaveSelector.setSelectedId(5);
    octaveSelector.onChange = [this]{ attachGroups(); };
    for (int i = 0; i < 12; ++i) slider("cent" + juce::String(i), mp::noteName(i + 60).dropLastCharacters(1), -200, 200, 0.01, " c");
    slider("quantize", "QUANTIZE  0-1", 0, 1, 0.001, ""); slider("tightness", "Intonation tightness", 0, 1, 0.001, "");
    slider("vibrato", "Vibrato preservation", 0, 1, 0.001, ""); slider("smoothing", "Transition smoothing", 0, 250, 1, " ms");
    slider("humanize", "Humanize maximum", 0, 50, 0.1, " c"); slider("seed", "Humanize seed", 1, 10000, 1, "");
    slider("reference", "Reference tuning", 400, 480, 0.1, " Hz"); slider("wet", "Wet / Dry", 0, 1, 0.001, "");
    slider("offset", "Timeline offset", 0, 36000, 0.001, " s");
    for (int i = 0; i < 12; ++i) slider("gq" + juce::String(i), "Strength x Quantize", 0, 1, 0.001, "");
    setResizable(true, true); setResizeLimits(1060, 800, 1800, 1200); setSize(1240, 900); map.refresh(); startTimerHz(10);
}
PitchEditor::~PitchEditor() { stopTimer(); setLookAndFeel(nullptr); }
juce::TextButton& PitchEditor::button(juce::String label, std::function<void()> action) {
    auto b = std::make_unique<juce::TextButton>(label); b->onClick = std::move(action); addAndMakeVisible(*b); buttons.push_back(std::move(b)); return *buttons.back();
}
void PitchEditor::slider(juce::String id, juce::String name, double min, double max, double step, const juce::String& suffix) {
    auto s = std::make_unique<juce::Slider>(); s->setSliderStyle(juce::Slider::LinearHorizontal); s->setRange(min, max, step); s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 75, 22); s->setTextValueSuffix(suffix); addAndMakeVisible(*s);
    auto l = std::make_unique<juce::Label>(); l->setText(name, juce::dontSendNotification); l->setColour(juce::Label::textColourId, text.withAlpha(0.8f)); addAndMakeVisible(*l);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, id, *s)); sliders.push_back(std::move(s)); labels.push_back(std::move(l));
}
void PitchEditor::attachGroups() {
    const bool octave = processor.parameters.getRawParameterValue("octave")->load() > 0.5f;
    for (int i = 0; i < 12; ++i) {
        sliderAttachments[size_t(i)].reset();
        const auto id = octave ? "oct" + juce::String(juce::jlimit(0, 127, (octaveSelector.getSelectedId() - 1) * 12 + i)) : "cent" + juce::String(i);
        sliderAttachments[size_t(i)] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, id, *sliders[size_t(i)]);
    }
}
void PitchEditor::paint(juce::Graphics& g) {
    g.fillAll(background); g.setColour(panel); g.fillRoundedRectangle(float(getWidth() - 270), 112, 252, float(getHeight() - 190), 9);
    g.setColour(accent.withAlpha(0.8f)); g.setFont(12); g.drawText("OFFLINE CORRECTION  |  MONOPHONIC", 24, 46, 600, 18, juce::Justification::left);
    g.setColour(text.withAlpha(0.55f)); g.drawText("Scroll: zoom  /  Shift+scroll: pan  /  Shift+click: multi-select  /  Drag: detach + target", 24, getHeight() - 62, getWidth() - 48, 18, juce::Justification::left);
}
void PitchEditor::resized() {
    title.setBounds(20, 10, getWidth() - 40, 34);
    for (int i = 0; i < 10; ++i) buttons[size_t(i)]->setBounds(22 + i * ((getWidth() - 44) / 10), 76, (getWidth() - 44) / 10 - 5, 28);
    const int right = getWidth() - 258;
    map.setBounds(22, 118, getWidth() - 306, getHeight() - 475);
    for (int i = 10; i < 15; ++i) buttons[size_t(i)]->setBounds(right + ((i - 10) % 2) * 119, 126 + ((i - 10) / 2) * 33, 113, 27);
    octaveSelector.setBounds(right + 119, 192, 113, 27);
    for (int i = 12; i < 21; ++i) { const int y = 236 + (i - 12) * 49; labels[size_t(i)]->setBounds(right, y, 230, 19); sliders[size_t(i)]->setBounds(right, y + 19, 230, 27); }
    const int row = getHeight() - 342, width = (getWidth() - 306) / 6;
    for (int i = 0; i < 12; ++i) { const int xx = 22 + (i % 6) * width, yy = row + (i / 6) * 100; labels[size_t(i)]->setBounds(xx, yy, width, 19); sliders[size_t(i)]->setBounds(xx, yy + 19, width - 5, 29); labels[size_t(i + 21)]->setBounds(xx, yy + 48, width, 17); sliders[size_t(i + 21)]->setBounds(xx, yy + 65, width - 5, 27); }
    const int editing = getHeight() - 139;
    for (int i = 15; i < 19; ++i) buttons[size_t(i)]->setBounds(22 + (i - 15) * 77, editing, 71, 25);
    noteCents.setBounds(335, editing, 67, 25); noteStrength.setBounds(408, editing, 67, 25);
    buttons[19]->setBounds(482, editing, 81, 25); buttons[20]->setBounds(570, editing, 106, 25);
    noteLabel.setBounds(22, editing + 31, getWidth() - 306, 22);
    progressBar.setBounds(getWidth() - 258, getHeight() - 67, 232, 14);
    statusLabel.setBounds(22, getHeight() - 36, getWidth() - 44, 26);
}
void PitchEditor::timerCallback() {
    map.refresh(); progressValue = processor.busy ? processor.progress.load() : 0; progressBar.setVisible(processor.busy);
    statusLabel.setText(processor.status(), juce::dontSendNotification);
    buttons[1]->setButtonText(processor.capturing ? "Stop capture" : "Capture"); buttons[9]->setButtonText(processor.previewing ? "Stop preview" : "Preview");
    for (int i = 0; i < 10; ++i) buttons[size_t(i)]->setEnabled(!processor.busy && (!processor.capturing || i == 1));
    const auto c = processor.clip();
    if (c && map.selected >= 0 && map.selected < int(c->notes.size())) {
        const auto& n = c->notes[size_t(map.selected)];
        noteLabel.setText(mp::noteName(n.key) + "  |  " + juce::String(mp::midiToHz(n.centre), 2) + " Hz  |  " + juce::String((n.centre - n.key) * 100, 2) + " cents  |  confidence " + juce::String(n.confidence * 100, 0) + "%  |  " + (n.detached ? "independent" : "grouped"), juce::dontSendNotification);
        if (lastSelected != map.selected) { noteCents.setText(juce::String(n.detached ? n.target : 0, 2)); noteStrength.setText(juce::String(n.detached ? n.strength * 100 : processor.settings().strength * 100, 1)); lastSelected = map.selected; }
    } else noteLabel.setText("Select a note. Fields: target cents, strength %. Merge joins the selected range.", juce::dontSendNotification);
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
