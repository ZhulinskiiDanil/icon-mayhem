#pragma once

#include <Geode/Geode.hpp>

#include <filesystem>
#include <string>
#include <vector>

// ! --- Presets --- !
// A preset is the whole look of the icon: the values of every look setting (see lookSettingKeys()).
// Saved as JSON files in the mod save folder and shared as the same JSON, through the clipboard
// or a file. Applying a preset just writes the settings, so the hair updates right away.

struct Preset
{
  std::string name;
  matjson::Value settings = matjson::Value::object(); // setting key -> value
};

namespace presets
{
  std::filesystem::path folder();
  // Saved file of a preset, the name made safe for the file system
  std::filesystem::path pathOf(std::string_view name);

  // The current look
  Preset capture(std::string name);
  // Settings missing from the preset (older version) go back to their defaults
  void apply(Preset const &preset);

  matjson::Value toJson(Preset const &preset);
  geode::Result<Preset> fromJson(matjson::Value const &json);
  geode::Result<Preset> parse(std::string_view text);
  geode::Result<Preset> readFile(std::filesystem::path const &path);

  // Saved presets sorted by name
  std::vector<Preset> list();
  bool exists(std::string_view name);
  // Overwrites a saved preset with the same name
  geode::Result<> save(Preset const &preset);
  geode::Result<> remove(std::string_view name);
  geode::Result<> writeFile(Preset const &preset, std::filesystem::path const &path);
}
