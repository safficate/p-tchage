#include "PluginProcessor.h"
#include "Localization.h"
#include "LocalizedFonts.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
int assertions = 0;
void check(bool pass, const juce::String& message) {
    ++assertions;
    if (!pass) throw std::runtime_error(message.toStdString());
}

juce::String utf(const char* value) { return juce::String::fromUTF8(value); }

template<class Predicate>
juce::Component* find(juce::Component& parent, Predicate predicate) {
    if (predicate(parent)) return &parent;
    for (auto* child : parent.getChildren())
        if (auto* result = find(*child, predicate)) return result;
    return nullptr;
}

juce::ComboBox& languageBox(juce::Component& editor) {
    auto* found = find(editor, [](juce::Component& item) { return item.getComponentID() == "uiLanguage"; });
    auto* box = dynamic_cast<juce::ComboBox*>(found);
    check(box != nullptr, "Real editor must expose a language selector with the stable component ID");
    return *box;
}

juce::Button& button(juce::Component& editor, const juce::String& label, int language) {
    const auto translated = mp::localize(label, language);
    auto* result = find(editor, [&](juce::Component& item) {
        const auto* candidate = dynamic_cast<juce::Button*>(&item);
        return candidate != nullptr && candidate->getButtonText() == translated;
    });
    auto* resultButton = dynamic_cast<juce::Button*>(result);
    check(resultButton != nullptr, "Expected translated control: " + label + " / " + translated);
    return *resultButton;
}

void click(juce::Button& control) {
    check(bool(control.onClick), "The tested control must have a real click handler");
    control.onClick();
}

juce::ValueTree dspParameters(PitchProcessor& processor) {
    auto state = processor.parameters.copyState();
    for (int i = state.getNumChildren(); --i >= 0;)
        if (state.getChild(i).getProperty("id").toString() == "uiLanguage") state.removeChild(i, nullptr);
    return state;
}

void writePng(juce::Component& editor, const juce::File& output) {
    editor.setSize(1260, 1000);
    juce::Image image(juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true, juce::SoftwareImageType());
    { juce::Graphics graphics(image); editor.paintEntireComponent(graphics, true); }
    if (output.getFileName().endsWith("-main.png")) {
        int textPixels = 0;
        for (int y = 20; y < 46; ++y)
            for (int x = 20; x < 600; ++x) {
                const auto colour = image.getPixelAt(x, y);
                if (colour.getRed() > 160 && colour.getGreen() > 160 && colour.getBlue() > 160) ++textPixels;
            }
        check(textPixels > 100, "Actual main-screen title must contain rendered text pixels, including on Windows without system UI fonts");
    }
    output.deleteFile();
    auto stream = output.createOutputStream();
    check(stream != nullptr, "Screenshot output stream must open");
    check(juce::PNGImageFormat().writeImageToStream(image, *stream), "Actual editor must render to a PNG");
    std::cout << "Rendered " << output.getFileName() << '\n';
}

void catalog() {
    check(mp::languageCount == 9, "The requested languages plus Portuguese and Italian must be available");
    const juce::StringArray codes{"en", "zh-Hans", "de", "tr", "fr", "ar", "es", "pt", "it"};
    const auto keys = mp::localizationKeys();
    check(keys.size() > 50, "The catalog must contain the interface and explanatory text, not only a language menu");
    const juce::String explanation = "Her modul ayri acilir. Otomatik: sinyal tabanli kurallar; egitilmis calgi tanima degildir.";
    std::set<juce::String> explanations;
    for (int language = 0; language < mp::languageCount; ++language) {
        check(mp::languageName(language).isNotEmpty(), "Every language must have a native display name");
        check(mp::languageCode(language) == codes[language], "Saved language IDs must retain their documented language codes");
        check(mp::rightToLeft(language) == (language == 5), "Arabic must be identified as right-to-left");
        for (const auto& key : keys) {
            const auto translated = mp::localize(key, language);
            check(translated.isNotEmpty(), "Empty translation for " + mp::languageCode(language) + ": " + key);
            check(!translated.containsChar(juce::juce_wchar(0xfffd)), "A translation must not contain a Unicode replacement character");
        }
        const auto translated = mp::localize(explanation, language);
        check(translated.length() > 20, "Every language needs the explanatory sentence, not a label placeholder");
        explanations.insert(translated);
        const juce::String arbitrary = "My track | DSP sample 1.25 c | My melody";
        check(mp::localize(arbitrary, language) == arbitrary, "Exact lookup must preserve user and track text");
        check(mp::localizeStatus(arbitrary, language) == arbitrary, "Status templates must not rewrite arbitrary user and track text");
        for (const auto& symbol : juce::StringArray{"Do", "Do#", "La4", "C#4", "c", "Hz", "ms", "MIDI", "VST3", "Dry/Wet", "Bypass"})
            check(mp::localize(symbol, language) == symbol, "Conventional symbol must remain unchanged: " + symbol);
        const auto gap = mp::localizeStatus("Duyulmamis alan var: 0.125 saniye. Eksik alanlari dinletin; bosluklar ses diye doldurulmadi.", language);
        check(gap.contains("0.125"), "A translated status must preserve the real measured gap value");
        if (language != 3) check(gap != "Duyulmamis alan var: 0.125 saniye. Eksik alanlari dinletin; bosluklar ses diye doldurulmadi.", "Known application status must be translated");
    }
    check(explanations.size() == size_t(mp::languageCount), "All languages must have actual distinct explanatory translations");
    check(mp::localize("ESITLE", -1) == mp::localize("ESITLE", 0), "Invalid language IDs must fall back to English");
    check(mp::localize("ESITLE", 99) == mp::localize("ESITLE", 0), "High invalid language IDs must fall back to English");
}

bool relevantGlyph(juce::juce_wchar character, int language) {
    if (language == 1) return character >= 0x2e80 && character <= 0xffff;
    if (language != 5) return character >= 0x20;
    return (character >= 0x600 && character <= 0x6ff) || (character >= 0x750 && character <= 0x77f)
        || (character >= 0x8a0 && character <= 0x8ff) || (character >= 0xfb50 && character <= 0xfdff)
        || (character >= 0xfe70 && character <= 0xfeff);
}

void fonts() {
    for (int language = 0; language < mp::languageCount; ++language) {
        const auto face = mp::uiFont(language, 16).getTypefacePtr();
        check(face != nullptr, "Every UI language must load from the bundled subsets");
        std::set<juce::juce_wchar> tested;
        auto strings = mp::localizationKeys();
        strings.add(mp::languageName(language));
        for (const auto& key : strings) {
            const auto translated = mp::localize(key, language);
            for (auto pointer = translated.getCharPointer(); !pointer.isEmpty();) {
                const auto code = pointer.getAndAdvance();
                if (!relevantGlyph(code, language) || !tested.insert(code).second) continue;
                const auto glyph = face->getNominalGlyphForCodepoint(code);
                check(glyph.has_value() && *glyph != 0, "Bundled font lacks catalog character U+" + juce::String::toHexString(int(code)) + " in " + mp::languageCode(language));
            }
        }
        check(tested.size() > 20, "Coverage test must examine actual script glyphs");
        std::cout << "Bundled " << mp::languageCode(language) << " font: " << tested.size() << " distinct catalog characters covered\n";
    }

    const auto arabic = utf("اللغة العربية");
    const auto font = mp::fontForText(arabic, 5, 24);
    const auto face = font.getTypefacePtr();
    std::set<int> nominal;
    for (auto pointer = arabic.getCharPointer(); !pointer.isEmpty();) {
        const auto code = pointer.getAndAdvance();
        if (const auto glyph = face->getNominalGlyphForCodepoint(code)) nominal.insert(int(*glyph));
    }
    juce::AttributedString text(arabic); text.setFont(font); text.setReadingDirection(juce::AttributedString::rightToLeft);
    juce::TextLayout layout; layout.createLayout(text, 800);
    check(layout.getNumLines() == 1, "Arabic phrase must shape into a real text line");
    int glyphCount = 0; bool contextual = false;
    for (int line = 0; line < layout.getNumLines(); ++line)
        for (const auto* run : layout.getLine(line).runs)
            for (const auto& glyph : run->glyphs) {
                check(glyph.glyphCode != 0, "Arabic layout must not contain a missing-glyph box");
                ++glyphCount; contextual |= !nominal.contains(glyph.glyphCode);
            }
    check(glyphCount > 5 && contextual, "Arabic layout must use contextual joined glyphs, not isolated nominal forms only");
}

void audioInvariant() {
    for (double rate : {44100., 48000.}) {
        PitchProcessor fixed, switched;
        fixed.prepareToPlay(rate, 257); switched.prepareToPlay(rate, 257);
        fixed.setParameter("globalCents", 5); switched.setParameter("globalCents", 5);
        juce::AudioBuffer<float> a(2, 257), b(2, 257); juce::MidiBuffer midiA, midiB;
        float largest = 0, peak = 0; int index = 0;
        for (int at = 0; at < int(rate * 2); at += 257, ++index) {
            const int count = std::min(257, int(rate * 2) - at);
            a.setSize(2, count, false, false, true); b.setSize(2, count, false, false, true);
            for (int sample = 0; sample < count; ++sample) {
                const float input = float(.2 * std::sin(2 * juce::MathConstants<double>::pi * mp::midiToHz(69.1) * (at + sample) / rate));
                for (int channel = 0; channel < 2; ++channel) {a.setSample(channel, sample, input); b.setSample(channel, sample, input);}
            }
            switched.setParameter("uiLanguage", float((index / 7) % mp::languageCount));
            fixed.processBlock(a, midiA); switched.processBlock(b, midiB);
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < count; ++sample) {
                    largest = std::max(largest, std::abs(a.getSample(channel, sample) - b.getSample(channel, sample)));
                    peak = std::max(peak, std::abs(a.getSample(channel, sample)));
                }
        }
        check(peak > .1f, "Audio language-invariance fixture must emit actual processed audio");
        check(largest == 0, "Switching languages during playback must leave emitted audio bit-identical to the English instance");
        std::cout << "Language switching audio invariant at " << rate << " Hz; maximum sample difference=" << largest << '\n';
    }
}

void interfaceAndState(const juce::File& output, bool screenshots) {
    PitchProcessor first, second;
    check(first.uiLanguage() == 0 && second.uiLanguage() == 0, "New instances must open in English regardless of system locale");
    first.setMeasuredCentre(0, -15); first.setTargetCentre(0, -10); first.setParameter("globalCents", 20);
    auto firstEditor = std::unique_ptr<juce::AudioProcessorEditor>(first.createEditor());
    std::cout << "Constructed first real editor\n";
    auto secondEditor = std::unique_ptr<juce::AudioProcessorEditor>(second.createEditor());
    std::cout << "Constructed second real editor\n";
    check(firstEditor != nullptr && secondEditor != nullptr, "Real localized editors must construct");
    auto& selector = languageBox(*firstEditor);
    check(selector.getNumItems() == mp::languageCount && selector.getSelectedId() == 1, "Corner language menu must list every language and initially select English");
    for (int language = 0; language < mp::languageCount; ++language)
        check(selector.getItemText(language) == mp::languageName(language), "Language names must be native self names");
    const auto initialDsp = dspParameters(first);
    auto& englishSecond = button(*secondEditor, "ESITLE", 0);
    const auto secondEnglishText = englishSecond.getButtonText();
    for (int language = 0; language < mp::languageCount; ++language) {
        selector.setSelectedId(language + 1, juce::sendNotificationSync);
        check(first.uiLanguage() == language, "Language menu must update the saved per-instance parameter immediately");
        check(button(*firstEditor, "ESITLE", language).getButtonText() == mp::localize("ESITLE", language), "Changing the menu must update the actual interface text immediately");
        check(dspParameters(first).isEquivalentTo(initialDsp), "Changing interface language must not change any DSP target, amount or smoothing parameter");
        check(second.uiLanguage() == 0 && englishSecond.getButtonText() == secondEnglishText, "One instance's language must not affect another instance");
        for (const auto& key : juce::StringArray{"Kendi hedeflerim", "Otomatik (sinyal davranisi)"}) {
            auto* box = dynamic_cast<juce::ComboBox*>(find(*firstEditor, [&](juce::Component& item) {
                auto* candidate = dynamic_cast<juce::ComboBox*>(&item);
                return candidate != nullptr && candidate->getItemText(0) == mp::localize(key, language);
            }));
            check(box != nullptr && box->getText() == mp::localize(key, language), "Selected combo caption must translate immediately: " + key);
        }
        std::cout << "Verified real interface language " << mp::languageCode(language) << '\n';
    }
    selector.setSelectedId(6, juce::sendNotificationSync);
    juce::MemoryBlock saved; first.getStateInformation(saved);
    PitchProcessor restored; restored.setStateInformation(saved.getData(), int(saved.getSize()));
    check(restored.uiLanguage() == 5, "Project-state round trip must retain the selected language");
    check(dspParameters(restored).isEquivalentTo(initialDsp), "Language state round trip must preserve all sound parameters");
    auto restoredEditor = std::unique_ptr<juce::AudioProcessorEditor>(restored.createEditor());
    check(languageBox(*restoredEditor).getSelectedId() == 6, "Reopened editor must show its saved language");
    button(*restoredEditor, "ESITLE", 5);

    auto oldParameters = first.parameters.copyState();
    bool removed = false;
    for (int i = oldParameters.getNumChildren(); --i >= 0;)
        if (oldParameters.getChild(i).getProperty("id").toString() == "uiLanguage") {oldParameters.removeChild(i, nullptr); removed = true;}
    check(removed, "Old-project fixture must omit the language parameter");
    juce::ValueTree oldProject("MicrotonalPitchMapPro"); oldProject.setProperty("version", mp::stateVersion, nullptr); oldProject.addChild(oldParameters, -1, nullptr);
    juce::MemoryOutputStream oldBytes; oldProject.writeToStream(oldBytes);
    restored.setStateInformation(oldBytes.getData(), int(oldBytes.getDataSize()));
    check(restored.uiLanguage() == 0, "Loading a pre-language project into an existing non-English instance must default to English");

    if (screenshots) {
        check(output.createDirectory().wasOk(), "Screenshot directory must be writable");
        for (int language : {0, 1, 5}) {
            selector.setSelectedId(language + 1, juce::sendNotificationSync);
            click(button(*firstEditor, "Ana ekran", language));
            const auto prefix = mp::languageCode(language);
            writePng(*firstEditor, output.getChildFile(prefix + "-main.png"));
            click(button(*firstEditor, "Smoother / Ayrintilar", language));
            click(button(*firstEditor, "Perde / Humanize", language));
            writePng(*firstEditor, output.getChildFile(prefix + "-pitch-detail.png"));
            click(button(*firstEditor, "Tini / Rezonans", language));
            writePng(*firstEditor, output.getChildFile(prefix + "-tone-detail.png"));
            click(button(*firstEditor, "Yerel Onarim", language));
            writePng(*firstEditor, output.getChildFile(prefix + "-repair-detail.png"));
            click(button(*firstEditor, "AI Sohbet", language));
            if(language==1||language==5){
                auto* prompt=dynamic_cast<juce::TextEditor*>(find(*firstEditor,[](juce::Component& item){return item.getComponentID()=="assistantPrompt";}));
                check(prompt!=nullptr&&prompt->getText().isEmpty(),"AI prompt fixture must remain empty to exercise its placeholder font");
                const auto code=juce::juce_wchar(language==1?0x4f60:0x0645);
                const auto glyph=prompt->getFont().getTypefacePtr()->getNominalGlyphForCodepoint(code);
                check(glyph.has_value()&&*glyph!=0,"Empty Chinese/Arabic input must already render its translated placeholder without a system script font");
            }
            writePng(*firstEditor, output.getChildFile(prefix + "-ai.png"));
            click(button(*firstEditor, "Gecmis", language));
            writePng(*firstEditor, output.getChildFile(prefix + "-history.png"));
        }
    }
}
}

int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        const bool stateOnly = argc > 1 && juce::String(argv[1]) == "--state-only";
        const auto output = argc > 1 && !stateOnly ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1])
                                                  : juce::File::getCurrentWorkingDirectory().getChildFile("localization-screenshots");
        catalog();
        if (!stateOnly) fonts();
        audioInvariant();
        interfaceAndState(output, !stateOnly);
        std::cout << assertions << " localization/catalog/interface/state assertions passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
