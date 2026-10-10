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
  // Nothing on the icon: every look setting at its default and the hair off, to build a look from scratch
  void applyEmpty();

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

  // Overwrites a saved preset with the same name
  geode::Result<> save(Preset const &preset);
  geode::Result<> remove(std::string_view name);
  // Renames a saved preset, the looks that wear it go along
  geode::Result<> rename(std::string_view from, std::string const &to);
  // `base`, or "base 2", "base 3"... the first name no saved preset has
  std::string uniqueName(std::string const &base);

  // The current look and the preset it came from: the one loaded or saved last. Empty when it
  // came from none (an empty look, or that preset was deleted)
  std::string current();
  bool currentIsBuiltIn();
  // The current look now is the preset `name`: changes are counted from here
  void markCurrent(std::string_view name, bool builtIn);
  // The look changed since it was loaded or saved
  bool modified();
  // Applies a preset as the current look
  void load(Preset const &preset);
  // A look from no preset that is exactly one of the saved presets becomes that preset
  // (looks from before the mod remembered where they came from)
  void adoptMatchingPreset();

  // Saved presets loaded or saved lately, the latest first
  std::vector<std::string> recent();

  // Editing the look of an icon in the customizer: the main look and the preset it came from
  // wait aside, saved, so even a restart in the middle brings them back. `editing` is the preset
  // of the icon: what changed in it stays as its draft when the main look comes back
  void stashMainLook(std::string const &editing);
  bool hasStashedLook();
  void restoreMainLook();

  // Unsaved changes to a preset (an icon look edited in the customizer and not saved yet). Worn
  // everywhere like the preset itself, so it can be tried in a level, until it is saved (save()
  // drops it) or the preset is loaded again in the customizer
  std::optional<matjson::Value> draft(std::string_view name);
  // Keeps `settings` as the draft of `name`; nothing when they are the saved preset
  void setDraft(std::string const &name, matjson::Value const &settings);
  void clearDraft(std::string_view name);
  geode::Result<> writeFile(Preset const &preset, std::filesystem::path const &path);
}
