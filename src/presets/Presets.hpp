#pragma once

#include <Geode/Geode.hpp>

#include <filesystem>
#include <optional>
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
  bool builtIn = false; // shipped with the mod, can't be deleted
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
  // A random cute look on top of the current one, physics and the cap gap stay as they are
  Preset surprise();
  // The current look with a random palette: the hair color and an accent for the accessories
  Preset surpriseColors();
  // The current look with every part of the hair (face locks, bangs, tails, ears) colored like the hair
  Preset matchColors();

  // Default values of every look setting, and a look without the settings at their defaults
  // (small enough to send to other players)
  matjson::Value defaults();
  matjson::Value compact(matjson::Value const &settings);
  // The settings of a look on top of the defaults, so missing ones don't come from this player's settings
  matjson::Value withDefaults(matjson::Value const &settings);

  matjson::Value toJson(Preset const &preset);
  geode::Result<Preset> fromJson(matjson::Value const &json);
  geode::Result<Preset> parse(std::string_view text);
  geode::Result<Preset> readFile(std::filesystem::path const &path);

  // Saved presets sorted by name
  std::vector<Preset> list();
  // Presets shipped with the mod (resources/presets/preset-*.json), sorted by name
  std::vector<Preset> builtIn();
  bool exists(std::string_view name);
  // A saved preset by name, or a built-in one
  std::optional<Preset> find(std::string_view name);

  // Favorites, switched through from the pause menu
  std::vector<std::string> favorites();
  bool isFavorite(std::string_view name);
  void setFavorite(std::string_view name, bool favorite);
  // Applies the favorite after the one applied last, gives its name (nothing without favorites)
  std::optional<std::string> applyNextFavorite();
  // Overwrites a saved preset with the same name
  geode::Result<> save(Preset const &preset);
  geode::Result<> remove(std::string_view name);
  geode::Result<> writeFile(Preset const &preset, std::filesystem::path const &path);
}
