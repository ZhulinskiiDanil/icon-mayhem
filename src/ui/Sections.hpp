#pragma once

#include <string_view>
#include <vector>

// ! --- Customizer sections --- !
// Tabs of the customizer, each a list of mod setting keys in display order.
// A new customization only needs its settings in mod.json and a section here,
// presets pick its settings up automatically. Keys starting with '@' are special rows
// built by the customizer ("@looks": the looks per mode button).

struct CustomizerSection
{
  char const *name;
  std::vector<char const *> keys;
};

std::vector<CustomizerSection> const &customizerSections();

// Settings that make up the look of the icon: every key of every section except titles
// and the switches of the mod itself (enable, show in garage, game modes)
std::vector<std::string_view> const &lookSettingKeys();
