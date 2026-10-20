#pragma once
#include "PluginProcessor.h"

class PitchMap final : public juce::Component {
public:
    explicit PitchMap(PitchProcessor& p) : processor(p) { setWantsKeyboardFocus(true); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void refresh();
    void split();
    void merge();
    void setNote(double cents, double strength, bool detach);
    juce::String regionInfo() const;
    int selected = -1;
private:
    PitchProcessor& processor;
    std::shared_ptr<const mp::Clip> data;
    juce::Image spectrum;
    mp::CorrectionPlan planned;
    juce::String planFingerprint;
    std::vector<int> selection;
    int seenRevision = -1;
    std::vector<PitchProcessor::TracePoint> liveTrace;
    double zoom = 1, pitchZoom = 1, offset = 0, lowPitch = 48, highPitch = 84, dragCents = 0;
    double length() const;
    float x(double seconds) const;
    float y(double midi) const;
    juce::Rectangle<float> bounds(const mp::Note&) const;
};

class PitchEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit PitchEditor(PitchProcessor&);
    ~PitchEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    PitchProcessor& processor;
    juce::LookAndFeel_V4 theme;
    PitchMap map;
    juce::Label statusLabel, title, noteLabel, liveLabel;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::ComboBox>> noteDestinations;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> destinationAttachments;
    juce::ToggleButton directSwitch, adaptiveSwitch, minimumSwitch;
    juce::Slider precisionControl;
    juce::Label precisionLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> adaptiveAttachment, minimumAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> precisionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> directAttachment;
    std::array<juce::TextButton, 12> referenceKeys, sourceKeys;
    juce::ComboBox referenceSource, referenceOctave;
    juce::Label pairingLabel;
    juce::TextButton pairButton{"ESLESTIR"}, stopListening{"Dinlemeyi durdur"};
    int selectedSource = -1, selectedReference = 0;
    void updatePairing();
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ComboBox octaveSelector;
    juce::TextEditor noteCents, noteStrength;
    juce::ProgressBar progressBar;
    double progressValue = 0;
    int lastSelected = -2;
    bool advanced = false;
    void timerCallback() override;
    void choose(int action);
    juce::TextButton& button(juce::String, std::function<void()>);
    void slider(juce::String id, juce::String label, double min, double max, double step, const juce::String& suffix);
    void attachGroups();
};
