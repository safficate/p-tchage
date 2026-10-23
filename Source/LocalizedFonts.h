#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mp {
// Embedded, OFL-licensed UI subsets make the translated controls independent
// of optional Chinese/Arabic Windows language packs. OS fallback stays enabled
// for arbitrary user text outside the catalog's Chinese character set.
juce::Font uiFont(int language, float height);
juce::Font fontForText(const juce::String& text, int language, float height);

class LocalizedLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    void setLanguage(int value) { language=value; }
    juce::Font getTextButtonFont(juce::TextButton&,int) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override;
    void getIdealPopupMenuItemSize(const juce::String&,bool,int,int&,int&) override;
    void drawPopupMenuItem(juce::Graphics&,const juce::Rectangle<int>&,
                          bool,bool,bool,bool,bool,const juce::String&,
                          const juce::String&,const juce::Drawable*,const juce::Colour*) override;
private:
    int language=0;
};
}
