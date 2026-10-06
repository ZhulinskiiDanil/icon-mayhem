#include "Looks.hpp"
#include "Presets.hpp"

#include <array>
#include <optional>
#include <unordered_map>

using namespace geode::prelude;

// ! --- Looks --- !

namespace
{
  constexpr char const *kModeLooksSave = "mode-looks";
  constexpr char const *kPlayerTwoSave = "player2-look";
  constexpr char const *kPlayerTwoModesSave = "player2-mode-looks";
  constexpr size_t kModeCount = static_cast<size_t>(GameMode::Count);
  constexpr std::array<char const *, kModeCount> kModeKeys{"cube", "ship", "ball", "ufo", "wave",
                                                          "robot", "spider", "swing", "jetpack"};
  constexpr std::array<char const *, kModeCount> kModeNames{"Cube", "Ship", "Ball", "UFO", "Wave",
                                                           "Robot", "Spider", "Swing", "Jetpack"};

  // The saved assignments, read once
  struct Assignments
  {
    std::array<std::string, kModeCount> modes;
    std::array<std::string, kModeCount> playerTwoModes;
    std::string playerTwo;
  };

  void readModes(char const *key, std::array<std::string, kModeCount> &out)
  {
    auto const saved = Mod::get()->getSavedValue<matjson::Value>(key, matjson::Value::object());
    for (size_t i = 0; i < kModeCount; ++i)
    {
      if (auto name = saved.get(kModeKeys[i]); name && name.unwrap().isString())
        out[i] = name.unwrap().asString().unwrapOr("");
    }
  }

  Assignments &assignments()
  {
    static Assignments cache = []
    {
      Assignments out;
      readModes(kModeLooksSave, out.modes);
      readModes(kPlayerTwoModesSave, out.playerTwoModes);
      out.playerTwo = Mod::get()->getSavedValue<std::string>(kPlayerTwoSave, "");
      return out;
    }();
    return cache;
  }

  void saveModes(char const *key, std::array<std::string, kModeCount> const &modes)
  {
    auto json = matjson::Value::object();
    for (size_t i = 0; i < kModeCount; ++i)
    {
      if (!modes[i].empty())
        json[kModeKeys[i]] = modes[i];
    }
    Mod::get()->setSavedValue(key, json);
  }

  // Preset settings by name, read from disk once; nullopt for a missing preset
  std::unordered_map<std::string, std::optional<matjson::Value>> &presetCache()
  {
    static std::unordered_map<std::string, std::optional<matjson::Value>> cache;
    return cache;
  }

  struct CachedConfig
  {
    bool valid = false;
    unsigned version = 0;
    HairConfig config;
  };

  std::unordered_map<std::string, CachedConfig> &configCache()
  {
    static std::unordered_map<std::string, CachedConfig> cache;
    return cache;
  }
}

char const *looks::modeName(GameMode mode)
{
  return kModeNames[static_cast<size_t>(mode)];
}

std::string looks::forMode(GameMode mode, bool playerTwo)
{
  auto const &saved = assignments();
  return (playerTwo ? saved.playerTwoModes : saved.modes)[static_cast<size_t>(mode)];
}

void looks::setForMode(GameMode mode, bool playerTwo, std::string const &name)
{
  auto &saved = assignments();
  auto &modes = playerTwo ? saved.playerTwoModes : saved.modes;
  modes[static_cast<size_t>(mode)] = name;
  saveModes(playerTwo ? kPlayerTwoModesSave : kModeLooksSave, modes);
}

std::string looks::forPlayerTwo()
{
  return assignments().playerTwo;
}

void looks::setForPlayerTwo(std::string const &name)
{
  assignments().playerTwo = name;
  Mod::get()->setSavedValue(kPlayerTwoSave, name);
}

std::string looks::lookFor(bool playerTwo, GameMode mode)
{
  auto const &saved = assignments();
  bool const known = mode != GameMode::Count;
  auto const index = static_cast<size_t>(mode);
  if (playerTwo)
  {
    if (known && !saved.playerTwoModes[index].empty())
      return saved.playerTwoModes[index];
    if (!saved.playerTwo.empty())
      return saved.playerTwo;
  }
  return known ? saved.modes[index] : "";
}

HairConfig looks::configFor(std::string const &name)
{
  if (name.empty())
    return HairConfig::load();

  auto &cached = configCache()[name];
  if (cached.valid && cached.version == HairConfig::version())
    return cached.config;

  auto &settings = presetCache()[name];
  if (!settings)
  {
    if (auto preset = presets::find(name))
      settings = preset->settings;
  }

  cached.config = settings ? HairConfig::load(&*settings) : HairConfig::load();
  cached.version = HairConfig::version();
  cached.valid = true;
  return cached.config;
}

void looks::invalidate()
{
  presetCache().clear();
  configCache().clear();
}
