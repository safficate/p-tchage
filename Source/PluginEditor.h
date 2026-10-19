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
    int selected = -1;
private:
    PitchProcessor& processor;
    std::shared_ptr<const mp::Clip> data;
    juce::Image spectrum;
    std::vector<int> selection;
    int seenRevision = -1;
    double zoom = 1, offset = 0, lowPitch = 48, highPitch = 84, dragCents = 0;
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
    juce::Label statusLabel, title, noteLabel;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ComboBox octaveSelector;
    juce::TextEditor noteCents, noteStrength;
    juce::ProgressBar progressBar;
    double progressValue = 0;
    int lastSelected = -2;
    void timerCallback() override;
    void choose(int action);
    juce::TextButton& button(juce::String, std::function<void()>);
    void slider(juce::String id, juce::String label, double min, double max, double step, const juce::String& suffix);
    void attachGroups();
};
