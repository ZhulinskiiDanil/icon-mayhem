#pragma once

#include <string_view>
#include <vector>

// ! --- Customizer sections --- !
// Tabs of the customizer, each a list of mod setting keys in display order.
// A new customization only needs its settings in mod.json and a section here,
// presets pick its settings up automatically.

struct CustomizerSection
{
  char const *name;
  std::vector<char const *> keys;
};

std::vector<CustomizerSection> const &customizerSections();

// Settings that make up the look of the icon: every key of every section except titles
// and the switches of the mod itself (enable, show in garage)
std::vector<std::string_view> const &lookSettingKeys();
