#pragma once
#include <juce_core/juce_core.h>

namespace mp {
inline constexpr int languageCount = 9;
// Stable project-state IDs: en, zh-Hans, de, tr, fr, ar, es, pt, it.
juce::String languageName(int language);
juce::String languageCode(int language);
bool rightToLeft(int language) noexcept;
juce::StringArray localizationKeys();
// Exact catalog lookup, including English aliases. Unknown/user strings stay intact.
juce::String localize(const juce::String& text, int language);
// Only known application-generated numeric/status templates are transformed.
// Never apply this to arbitrary user/assistant conversation or track names.
juce::String localizeStatus(const juce::String& text, int language);
}
