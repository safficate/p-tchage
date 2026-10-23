#include "LocalizedFonts.h"
#include "PitchmapFontData.h"

namespace mp {
namespace {
bool isChinese(juce::juce_wchar c) {
    return (c>=0x3000&&c<=0x303f)||(c>=0x3400&&c<=0x9fff)||(c>=0xff00&&c<=0xffef);
}
bool isArabic(juce::juce_wchar c) {
    return (c>=0x0600&&c<=0x08ff)||(c>=0xfb50&&c<=0xfdff)||(c>=0xfe70&&c<=0xfeff);
}
juce::Typeface::Ptr chineseFace() {
    static const auto face=juce::Typeface::createSystemTypefaceFor(PitchmapFontData::ChineseUI_ttf,PitchmapFontData::ChineseUI_ttfSize);
    return face;
}
juce::Typeface::Ptr arabicFace() {
    static const auto face=juce::Typeface::createSystemTypefaceFor(PitchmapFontData::ArabicUI_ttf,PitchmapFontData::ArabicUI_ttfSize);
    return face;
}
juce::Typeface::Ptr latinFace() {
    static const auto face=juce::Typeface::createSystemTypefaceFor(PitchmapFontData::LatinUI_ttf,PitchmapFontData::LatinUI_ttfSize);
    return face;
}
bool arabicText(const juce::String& text) {for(auto c:text)if(isArabic(c))return true;return false;}
}
juce::Font uiFont(int language,float height) {
    const auto face=language==1?chineseFace():language==5?arabicFace():latinFace();
    return juce::Font(face?juce::FontOptions(face).withHeight(height):juce::FontOptions(height));
}
juce::Font fontForText(const juce::String& text,int language,float height) {
    for(auto c:text)if(isChinese(c))return uiFont(1,height);
    if(arabicText(text))return uiFont(5,height);
    // Empty editors must already use the selected script for their placeholder
    // and for the first typed character on hosts without an installed CJK font.
    return uiFont(text.isEmpty()?language:0,height);
}
juce::Font LocalizedLookAndFeel::getTextButtonFont(juce::TextButton& b,int height) {
    return fontForText(b.getButtonText(),language,juce::jmin(15.0f,height*.6f));
}
juce::Font LocalizedLookAndFeel::getComboBoxFont(juce::ComboBox& b) {
    return fontForText(b.getText(),language,juce::jmin(15.0f,b.getHeight()*.7f));
}
juce::Font LocalizedLookAndFeel::getLabelFont(juce::Label& label) {
    return fontForText(label.getText(),language,label.getFont().getHeight());
}
juce::Font LocalizedLookAndFeel::getPopupMenuFont() {return uiFont(language,16);}
void LocalizedLookAndFeel::drawToggleButton(juce::Graphics& g,juce::ToggleButton& button,bool highlighted,bool down) {
    const float size=juce::jmin(15.0f,button.getHeight()*.75f),tick=size*1.1f;
    drawTickBox(g,button,4,(button.getHeight()-tick)*.5f,tick,tick,button.getToggleState(),button.isEnabled(),highlighted,down);
    g.setColour(button.findColour(juce::ToggleButton::textColourId).withMultipliedAlpha(button.isEnabled()?1.0f:.5f));
    g.setFont(fontForText(button.getButtonText(),language,size));
    g.drawFittedText(button.getButtonText(),button.getLocalBounds().withTrimmedLeft(juce::roundToInt(tick)+10).withTrimmedRight(2),
                    arabicText(button.getButtonText())?juce::Justification::centredRight:juce::Justification::centredLeft,1);
}
void LocalizedLookAndFeel::getIdealPopupMenuItemSize(const juce::String& text,bool separator,int standardHeight,int& width,int& height) {
    if(separator){width=50;height=standardHeight>0?standardHeight/10:10;return;}
    auto font=fontForText(text,language,16);
    if(standardHeight>0&&font.getHeight()>standardHeight/1.3f)font.setHeight(standardHeight/1.3f);
    height=standardHeight>0?standardHeight:juce::roundToInt(font.getHeight()*1.4f);
    width=juce::GlyphArrangement::getStringWidthInt(font,text)+height*2;
}
void LocalizedLookAndFeel::drawPopupMenuItem(juce::Graphics& g,const juce::Rectangle<int>& area,
    bool separator,bool active,bool highlighted,bool ticked,bool submenu,const juce::String& text,
    const juce::String& shortcut,const juce::Drawable* icon,const juce::Colour* colour) {
    if(separator){g.setColour(findColour(juce::PopupMenu::textColourId).withAlpha(.3f));g.fillRect(area.getX()+5,area.getCentreY(),area.getWidth()-10,1);return;}
    auto bounds=area.reduced(1);
    if(highlighted&&active){g.setColour(findColour(juce::PopupMenu::highlightedBackgroundColourId));g.fillRect(bounds);}
    const auto ink=highlighted&&active?findColour(juce::PopupMenu::highlightedTextColourId):(colour?*colour:findColour(juce::PopupMenu::textColourId));
    g.setColour(ink.withMultipliedAlpha(active?1.0f:.5f));bounds.reduce(juce::jmin(5,area.getWidth()/20),0);
    const float height=juce::jmin(16.0f,bounds.getHeight()/1.3f);
    auto font=fontForText(text,language,height);g.setFont(font);
    const auto iconArea=bounds.removeFromLeft(juce::roundToInt(height)).toFloat();
    if(icon)icon->drawWithin(g,iconArea,juce::RectanglePlacement::centred|juce::RectanglePlacement::onlyReduceInSize,1);
    else if(ticked){const auto tick=getTickShape(1);g.fillPath(tick,tick.getTransformToScaleToFit(iconArea.reduced(iconArea.getWidth()/5,0),true));}
    if(submenu){const auto arrow=bounds.removeFromRight(10);juce::Path p;p.startNewSubPath(float(arrow.getX()),float(arrow.getCentreY()-4));p.lineTo(float(arrow.getX()+5),float(arrow.getCentreY()));p.lineTo(float(arrow.getX()),float(arrow.getCentreY()+4));g.strokePath(p,juce::PathStrokeType(1.5f));}
    bounds.removeFromRight(3);
    g.drawFittedText(text,bounds,arabicText(text)?juce::Justification::centredRight:juce::Justification::centredLeft,1);
    if(shortcut.isNotEmpty()){g.setFont(font.withHeight(height*.75f));g.drawText(shortcut,bounds,juce::Justification::centredRight,true);}
}
}
