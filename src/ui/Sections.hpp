#pragma once

#include <string>
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

// A block of a tab: the settings under one title (or before the first title, named after the tab).
// Its first setting is the switch of the block when it turns something on and off (a toggle, an
// option list with "None" or "Off", a count from 0); the customizer shows it in the header
struct CustomizerGroup
{
  std::string id;   // the title key, or "<tab>-top"
  std::string name; // the title, or the tab name
  size_t section = 0;
  char const *master = nullptr;   // the switch, if the block has one
  bool hidesWhenOff = false;      // off hides the other settings (toggles and option lists)
  std::vector<char const *> keys; // the rest, in order
};

// Every block of every tab, in order
std::vector<CustomizerGroup> const &customizerGroups();
// The switch of the block is on (a block without one always is)
bool groupIsOn(CustomizerGroup const &group);
// Fine tuning (positions, insets, tilts, detail) that waits behind "More" in its block
bool isFineTuning(std::string_view key);

// Settings that make up the look of the icon: every key of every section except titles
// and the switches of the mod itself (customization, show in garage, game modes)
std::vector<std::string_view> const &lookSettingKeys();
