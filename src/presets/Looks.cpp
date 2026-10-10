#include "Looks.hpp"
#include "Presets.hpp"

// More Icons is optional: its functions are reached through Geode events, without linking to it
#define MORE_ICONS_EVENTS
#include <hiimjustin000.more_icons/include/MoreIcons.hpp>

#include <algorithm>
#include <cctype>
#include <array>
#include <map>
#include <optional>
#include <unordered_map>

using namespace geode::prelude;

// ! --- Looks --- !

namespace
{
  constexpr char const *kModeLooksSave = "mode-looks";
  constexpr char const *kPlayerTwoSave = "player2-look";
  constexpr char const *kPlayerTwoModesSave = "player2-mode-looks";
  constexpr char const *kIconLooksSave = "icon-looks";
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
    std::map<std::string, std::string> icons; // "cube:37" -> preset
  };

  std::string iconKey(GameMode mode, int icon)
  {
    return fmt::format("{}:{}", kModeKeys[static_cast<size_t>(mode)], icon);
  }

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
      auto const icons = Mod::get()->getSavedValue<matjson::Value>(kIconLooksSave, matjson::Value::object());
      for (auto const &[key, value] : icons)
      {
        if (value.isString())
          out.icons[key] = value.asString().unwrapOr("");
      }
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

  constexpr std::string_view kRemotePrefix = "@remote:";
  constexpr std::string_view kBuiltInPrefix = "@builtin:";

  std::unordered_map<std::string, matjson::Value> &remoteLooks()
  {
    static std::unordered_map<std::string, matjson::Value> looks;
    return looks;
  }

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

std::string looks::forIcon(GameMode mode, int icon)
{
  if (mode == GameMode::Count || icon < 0)
    return "";
  auto const &icons = assignments().icons;
  auto it = icons.find(iconKey(mode, icon));
  return it == icons.end() ? "" : it->second;
}

void looks::setForIcon(GameMode mode, int icon, std::string const &name)
{
  auto &icons = assignments().icons;
  if (name.empty())
    icons.erase(iconKey(mode, icon));
  else
    icons[iconKey(mode, icon)] = name;

  auto json = matjson::Value::object();
  for (auto const &[key, value] : icons)
    json[key] = value;
  Mod::get()->setSavedValue(kIconLooksSave, json);
}

std::vector<std::pair<GameMode, int>> looks::iconsWithLooks()
{
  std::vector<std::pair<GameMode, int>> out;
  for (auto const &[key, value] : assignments().icons)
  {
    auto const colon = key.find(':');
    if (colon == std::string::npos)
      continue;
    auto const type = std::string_view(key).substr(0, colon);
    auto const mode = std::find(kModeKeys.begin(), kModeKeys.end(), type);
    auto const icon = numFromString<int>(std::string_view(key).substr(colon + 1));
    if (mode != kModeKeys.end() && icon)
      out.emplace_back(static_cast<GameMode>(mode - kModeKeys.begin()), icon.unwrap());
  }
  return out;
}

int looks::equippedIcon(GameMode mode)
{
  auto gm = GameManager::get();
  switch (mode)
  {
  case GameMode::Cube:
    return gm->getPlayerFrame();
  case GameMode::Ship:
    return gm->getPlayerShip();
  case GameMode::Ball:
    return gm->getPlayerBall();
  case GameMode::Ufo:
    return gm->getPlayerBird();
  case GameMode::Wave:
    return gm->getPlayerDart();
  case GameMode::Robot:
    return gm->getPlayerRobot();
  case GameMode::Spider:
    return gm->getPlayerSpider();
  case GameMode::Swing:
    return gm->getPlayerSwing();
  case GameMode::Jetpack:
    return gm->getPlayerJetpack();
  default:
    return -1;
  }
}

IconType looks::iconTypeOf(GameMode mode)
{
  switch (mode)
  {
  case GameMode::Ship:
    return IconType::Ship;
  case GameMode::Ball:
    return IconType::Ball;
  case GameMode::Ufo:
    return IconType::Ufo;
  case GameMode::Wave:
    return IconType::Wave;
  case GameMode::Robot:
    return IconType::Robot;
  case GameMode::Spider:
    return IconType::Spider;
  case GameMode::Swing:
    return IconType::Swing;
  case GameMode::Jetpack:
    return IconType::Jetpack;
  default:
    return IconType::Cube;
  }
}

std::string looks::forCustomIcon(GameMode mode, std::string const &name)
{
  if (mode == GameMode::Count || name.empty())
    return "";
  auto const &icons = assignments().icons;
  auto it = icons.find(fmt::format("{}:mi:{}", kModeKeys[static_cast<size_t>(mode)], name));
  return it == icons.end() ? "" : it->second;
}

void looks::setForCustomIcon(GameMode mode, std::string const &name, std::string const &preset)
{
  auto &icons = assignments().icons;
  auto const key = fmt::format("{}:mi:{}", kModeKeys[static_cast<size_t>(mode)], name);
  if (preset.empty())
    icons.erase(key);
  else
    icons[key] = preset;

  auto json = matjson::Value::object();
  for (auto const &[iconKey, value] : icons)
    json[iconKey] = value;
  Mod::get()->setSavedValue(kIconLooksSave, json);
}

std::string looks::equippedCustomIcon(GameMode mode, bool dual)
{
  if (mode == GameMode::Count)
    return "";
  auto info = more_icons::activeIcon(iconTypeOf(mode), dual);
  return info ? info->getName() : "";
}

std::string looks::linkedLook(GameMode mode)
{
  mode = linkModeOf(mode);
  auto const custom = equippedCustomIcon(mode);
  return custom.empty() ? forIcon(mode, equippedIcon(mode)) : forCustomIcon(mode, custom);
}

GameMode looks::linkModeOf(GameMode mode)
{
  return mode == GameMode::Ship || mode == GameMode::Ufo || mode == GameMode::Jetpack ? GameMode::Cube : mode;
}

std::string looks::wornLook(bool playerTwo, GameMode mode)
{
  // A look tried on covers everything, for player 1
  if (!playerTwo && !tryOn().empty())
    return tryOn();

  GameMode const linked = linkModeOf(mode);
  // The icon link of the rider, then the look of the mode itself
  if (mode != linked)
  {
    auto const custom = equippedCustomIcon(linked, playerTwo);
    auto const look = custom.empty() ? forIcon(linked, equippedIcon(linked)) : forCustomIcon(linked, custom);
    if (!look.empty() && !(playerTwo && (!forMode(mode, true).empty() || !forPlayerTwo().empty())))
      return look;
    return lookFor(playerTwo, mode);
  }
  return lookFor(playerTwo, mode, equippedIcon(mode), equippedCustomIcon(mode, playerTwo));
}

std::string looks::lookFor(bool playerTwo, GameMode mode, int icon, std::string const &custom)
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
  // A More Icons icon covers the game icon under it
  if (auto look = custom.empty() ? forIcon(mode, icon) : forCustomIcon(mode, custom); !look.empty())
    return look;
  return known ? saved.modes[index] : "";
}

HairConfig looks::configFor(std::string const &name)
{
  if (name.empty())
    return HairConfig::load();

  // Another player's look: on top of the defaults, not of this player's own settings
  if (name.starts_with(kRemotePrefix))
  {
    auto const &remotes = remoteLooks();
    auto it = remotes.find(name);
    if (it == remotes.end())
    {
      auto const defaults = presets::defaults();
      HairConfig nothing = HairConfig::load(&defaults);
      nothing.enabled = false;
      return nothing;
    }
    auto full = presets::withDefaults(it->second);
    HairConfig config = HairConfig::load(&full);
    if (auto enabled = it->second.get("enabled"); enabled && enabled.unwrap().isBool())
      config.enabled = enabled.unwrap().asBool().unwrap();
    return config;
  }

  auto &cached = configCache()[name];
  if (cached.valid && cached.version == HairConfig::version())
    return cached.config;

  auto const settings = presetSettings(name);
  cached.config = settings ? HairConfig::load(&*settings) : HairConfig::load();
  cached.version = HairConfig::version();
  cached.valid = true;
  return cached.config;
}

std::optional<matjson::Value> looks::presetSettings(std::string const &name)
{
  auto &settings = presetCache()[name];
  if (!settings && name.starts_with(kBuiltInPrefix))
  {
    auto const plain = name.substr(kBuiltInPrefix.size());
    for (auto const &preset : presets::builtIn())
    {
      if (preset.name == plain)
        settings = preset.settings;
    }
  }
  else if (!settings)
  {
    // Changes not saved yet are worn too, to try them in a level
    if (auto unsaved = presets::draft(name))
      settings = *unsaved;
    else if (auto preset = presets::find(name))
      settings = preset->settings;
  }
  return settings;
}

std::string looks::builtInLook(std::string const &name)
{
  return fmt::format("{}{}", kBuiltInPrefix, name);
}

void looks::renamePreset(std::string const &from, std::string const &to)
{
  auto &saved = assignments();
  auto rename = [&](std::string &name)
  {
    if (name == from)
      name = to;
  };
  for (auto &name : saved.modes)
    rename(name);
  for (auto &name : saved.playerTwoModes)
    rename(name);
  rename(saved.playerTwo);
  for (auto &[key, name] : saved.icons)
    rename(name);

  saveModes(kModeLooksSave, saved.modes);
  saveModes(kPlayerTwoModesSave, saved.playerTwoModes);
  Mod::get()->setSavedValue(kPlayerTwoSave, saved.playerTwo);
  auto json = matjson::Value::object();
  for (auto const &[key, value] : saved.icons)
    json[key] = value;
  Mod::get()->setSavedValue(kIconLooksSave, json);
}

void looks::invalidate()
{
  presetCache().clear();
  configCache().clear();
}

std::string looks::remoteName(int player)
{
  return fmt::format("{}{}", kRemotePrefix, player);
}

bool looks::hasRemote(int player)
{
  return remoteLooks().contains(remoteName(player));
}

void looks::setRemote(int player, matjson::Value look)
{
  remoteLooks()[remoteName(player)] = std::move(look);
  // The rigs reload their config on the next frame
  HairConfig::bumpVersion();
}

void looks::clearRemotes()
{
  // Gifts and the gallery stay, the players of the level are gone
  std::erase_if(remoteLooks(), [](auto const &entry)
                {
                  auto const key = std::string_view(entry.first).substr(kRemotePrefix.size());
                  return !key.empty() && std::isdigit(static_cast<unsigned char>(key.front()));
                });
}

std::string looks::setRemoteLook(std::string const &key, matjson::Value look)
{
  std::string const name = fmt::format("{}{}", kRemotePrefix, key);
  remoteLooks()[name] = std::move(look);
  HairConfig::bumpVersion();
  return name;
}

std::optional<matjson::Value> looks::remoteLook(std::string const &name)
{
  auto const &remotes = remoteLooks();
  auto it = remotes.find(name);
  if (it == remotes.end())
    return std::nullopt;
  return it->second;
}

namespace
{
  std::string &tryOnName()
  {
    static std::string name;
    return name;
  }
}

void looks::setTryOn(std::string const &name)
{
  tryOnName() = name;
  HairConfig::bumpVersion();
}

std::string const &looks::tryOn()
{
  return tryOnName();
}
