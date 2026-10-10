#pragma once

#include <Geode/Geode.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// ! --- Settings --- !
// Every setting of the mod by key, wherever it lives:
// - the look (hair, front, extras, effects, physics) and the game modes are defined in
//   resources/look-settings.json and kept in the config folder, look.json: only what differs from
//   the defaults, readable, and picked up a moment after it is edited by hand;
// - the few general ones (the master switch, quality, focus mode, online, emote keys) stay in
//   Geode's settings page of the mod.
// Values are JSON like in presets: true, 12, 0.5, "Option", "#ff80c0".
// Any change bumps HairConfig::version(), so the hair picks it up.

namespace settings
{
  enum class Type
  {
    Title,
    Bool,
    Int,
    Float,
    Choice, // a string with options
    Color,
    Other, // Geode only: keybinds, buttons
  };

  struct Def
  {
    std::string key;
    std::string name;
    std::string description;
    Type type = Type::Other;
    matjson::Value fallback; // the default
    double min = 0.0;
    double max = 1.0;
    double step = 0.0; // slider snap, 0 for none
    std::vector<std::string> options;
    bool inGeode = false; // a general setting on Geode's page
  };

  // Reads the definitions and look.json (moving the values over from Geode's settings the first
  // time), then watches the file
  void load();

  // The setting, nullptr for an unknown key
  Def const *def(std::string_view key);
  // Keys of the look file in definition order, titles included
  std::vector<std::string> const &fileKeys();

  // The value, or the default
  matjson::Value get(std::string_view key);
  float number(std::string_view key);
  int integer(std::string_view key);
  bool flag(std::string_view key);
  std::string text(std::string_view key);
  cocos2d::ccColor3B color(std::string_view key);

  // Writes a value: numbers are clamped, colors may be "#rrggbb" or {r, g, b}.
  // False when it doesn't fit (wrong type, unknown option); nothing changes then
  bool set(std::string_view key, matjson::Value const &value);
  void reset(std::string_view key);
  bool isDefault(std::string_view key);

  // A value as the setting keeps it, or nothing when it doesn't fit
  std::optional<matjson::Value> normalize(Def const &def, matjson::Value const &value);

  // Bumped when look.json was edited outside the game and read again (the customizer redraws)
  unsigned revision();
  // Writes look.json now if a change waits (the game closing)
  void flush();

  std::filesystem::path filePath();
}
