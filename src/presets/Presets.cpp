#include "Presets.hpp"

#include "../settings/Settings.hpp"
#include "../ui/Sections.hpp"
#include "Looks.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

using namespace geode::prelude;

// ! --- Format --- !

namespace
{
  constexpr char const *kFormat = "icon-mayhem-preset";
  constexpr int kVersion = 1;
  constexpr char const *kCurrentSave = "current-preset";
  constexpr char const *kCurrentBuiltInSave = "current-preset-built-in";
  constexpr char const *kBaselineSave = "current-preset-look"; // the look when it was loaded or saved
  constexpr char const *kRecentSave = "recent-presets";
  constexpr char const *kStashSave = "stashed-main-look";
  constexpr size_t kMaxRecent = 64;

  void replaceName(std::vector<std::string> &names, std::string_view from, std::string const &to)
  {
    for (auto &name : names)
    {
      if (name == from)
        name = to;
    }
  }

  // Put on the top of the recent list
  void touch(std::string_view name)
  {
    auto list = Mod::get()->getSavedValue<std::vector<std::string>>(kRecentSave, {});
    std::erase(list, std::string(name));
    list.insert(list.begin(), std::string(name));
    if (list.size() > kMaxRecent)
      list.resize(kMaxRecent);
    Mod::get()->setSavedValue(kRecentSave, list);
  }

  // Values of a setting are the same; colors may differ in letter case, numbers by rounding
  bool sameValue(matjson::Value const &a, matjson::Value const &b)
  {
    if (a.isNumber() && b.isNumber())
      return std::abs(a.asDouble().unwrap() - b.asDouble().unwrap()) < 1e-4;
    if (a.isString() && b.isString())
      return utils::string::toLower(a.asString().unwrap()) == utils::string::toLower(b.asString().unwrap());
    return a == b;
  }

  // Preset name -> file name, anything unusual becomes "_"
  std::string fileName(std::string_view name)
  {
    std::string out;
    for (char c : name)
    {
      bool const safe = std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '-' || c == '_';
      out += safe ? c : '_';
    }
    if (out.empty())
      out = "preset";
    return out + ".json";
  }
}

// ! --- Look --- !

std::filesystem::path presets::folder()
{
  return Mod::get()->getSaveDir() / "presets";
}

std::filesystem::path presets::pathOf(std::string_view name)
{
  return folder() / fileName(name);
}

Preset presets::capture(std::string name)
{
  Preset preset;
  preset.name = std::move(name);
  for (auto key : lookSettingKeys())
    preset.settings[key] = settings::get(key);
  return preset;
}

void presets::apply(Preset const &preset)
{
  for (auto key : lookSettingKeys())
  {
    auto value = preset.settings.get(key);
    if (!value || !settings::set(key, value.unwrap()))
      settings::reset(key);
  }
}

// ! --- Defaults --- !

matjson::Value presets::defaults()
{
  static matjson::Value const values = []
  {
    auto out = matjson::Value::object();
    for (auto key : lookSettingKeys())
    {
      if (auto def = settings::def(key))
        out[key] = def->fallback;
    }
    return out;
  }();
  return values;
}

matjson::Value presets::compact(matjson::Value const &settings)
{
  auto const base = defaults();
  auto out = matjson::Value::object();
  for (auto const &[key, value] : settings)
  {
    auto fallback = base.get(key);
    if (!fallback || fallback.unwrap() != value)
      out[key] = value;
  }
  return out;
}

matjson::Value presets::withDefaults(matjson::Value const &settings)
{
  auto out = defaults();
  for (auto const &[key, value] : settings)
    out[key] = value;
  return out;
}

// ! --- JSON --- !

matjson::Value presets::toJson(Preset const &preset)
{
  auto json = matjson::Value::object();
  json["format"] = kFormat;
  json["version"] = kVersion;
  json["name"] = preset.name;
  json["settings"] = preset.settings;
  return json;
}

Result<Preset> presets::fromJson(matjson::Value const &json)
{
  if (!json.isObject())
    return Err("Not a preset: expected a JSON object");

  auto format = json.get("format");
  if (!format || !format.unwrap().isString() || format.unwrap().asString().unwrap() != kFormat)
    return Err("Not an Icon Mayhem preset");

  auto settings = json.get("settings");
  if (!settings || !settings.unwrap().isObject())
    return Err("The preset has no settings");

  Preset preset;
  if (auto name = json.get("name"); name && name.unwrap().isString())
    preset.name = name.unwrap().asString().unwrap();
  if (preset.name.empty())
    preset.name = "Imported";
  preset.settings = settings.unwrap();
  return Ok(std::move(preset));
}

Result<Preset> presets::parse(std::string_view text)
{
  auto json = matjson::Value::parse(text);
  if (!json)
    return Err("Not valid JSON: {}", json.unwrapErr().message);
  return fromJson(json.unwrap());
}

Result<Preset> presets::readFile(std::filesystem::path const &path)
{
  GEODE_UNWRAP_INTO(auto json, file::readJson(path));
  return fromJson(json);
}

// ! --- Files --- !

std::vector<Preset> presets::list()
{
  std::vector<Preset> out;

  auto files = file::readDirectory(folder());
  if (!files)
    return out;

  for (auto const &path : files.unwrap())
  {
    if (path.extension() != ".json")
      continue;

    auto preset = readFile(path);
    if (!preset)
    {
      log::warn("Skipping preset {}: {}", utils::string::pathToString(path), preset.unwrapErr());
      continue;
    }
    out.push_back(std::move(preset).unwrap());
  }

  std::sort(out.begin(), out.end(), [](Preset const &a, Preset const &b)
            { return a.name < b.name; });
  return out;
}

std::vector<Preset> presets::builtIn()
{
  std::vector<Preset> out;

  auto files = file::readDirectory(Mod::get()->getResourcesDir());
  if (!files)
    return out;

  for (auto const &path : files.unwrap())
  {
    auto const name = utils::string::pathToString(path.filename());
    if (path.extension() != ".json" || !name.starts_with("preset-"))
      continue;

    auto preset = readFile(path);
    if (!preset)
    {
      log::warn("Skipping built-in preset {}: {}", name, preset.unwrapErr());
      continue;
    }
    preset.unwrap().builtIn = true;
    out.push_back(std::move(preset).unwrap());
  }

  std::sort(out.begin(), out.end(), [](Preset const &a, Preset const &b)
            { return a.name < b.name; });
  return out;
}

bool presets::exists(std::string_view name)
{
  std::error_code error;
  return std::filesystem::exists(pathOf(name), error);
}

std::optional<Preset> presets::find(std::string_view name)
{
  if (exists(name))
  {
    if (auto preset = readFile(pathOf(name)))
      return std::move(preset).unwrap();
  }
  for (auto &preset : builtIn())
  {
    if (preset.name == name)
      return std::move(preset);
  }
  return std::nullopt;
}

Result<> presets::save(Preset const &preset)
{
  GEODE_UNWRAP(file::createDirectoryAll(folder()));
  looks::invalidate();
  GEODE_UNWRAP(writeFile(preset, pathOf(preset.name)));
  touch(preset.name);
  return Ok();
}

Result<> presets::remove(std::string_view name)
{
  std::error_code error;
  std::filesystem::remove(pathOf(name), error);
  looks::invalidate();
  if (error)
    return Err("Unable to delete the preset: {}", error.message());

  auto recent = presets::recent();
  std::erase(recent, std::string(name));
  Mod::get()->setSavedValue(kRecentSave, recent);
  // The look stays on, it just isn't saved anywhere anymore
  if (current() == name && !currentIsBuiltIn())
    Mod::get()->setSavedValue<std::string>(kCurrentSave, "");
  return Ok();
}

Result<> presets::rename(std::string_view from, std::string const &to)
{
  if (to.empty())
    return Err("The name is empty");
  if (to == from)
    return Ok();

  auto preset = readFile(pathOf(from));
  if (!preset)
    return Err("Unable to read the preset: {}", preset.unwrapErr());

  // Only the letter case changes: on Windows that's the same file
  bool const sameFile = utils::string::toLower(utils::string::pathToString(pathOf(from))) ==
                        utils::string::toLower(utils::string::pathToString(pathOf(to)));
  if (!sameFile && exists(to))
    return Err("A preset named \"{}\" already exists", to);

  auto renamed = std::move(preset).unwrap();
  renamed.name = to;
  GEODE_UNWRAP(file::createDirectoryAll(folder()));
  GEODE_UNWRAP(writeFile(renamed, pathOf(to)));
  if (!sameFile)
  {
    std::error_code error;
    std::filesystem::remove(pathOf(from), error);
  }

  auto recent = presets::recent();
  replaceName(recent, from, to);
  Mod::get()->setSavedValue(kRecentSave, recent);
  if (current() == from && !currentIsBuiltIn())
    Mod::get()->setSavedValue(kCurrentSave, to);
  looks::renamePreset(std::string(from), to);
  looks::invalidate();
  return Ok();
}

std::string presets::uniqueName(std::string const &base)
{
  std::string name = base;
  for (int i = 2; exists(name); ++i)
    name = fmt::format("{} {}", base, i);
  return name;
}

// ! --- Current look --- !

std::string presets::current()
{
  return Mod::get()->getSavedValue<std::string>(kCurrentSave, "");
}

bool presets::currentIsBuiltIn()
{
  return Mod::get()->getSavedValue<bool>(kCurrentBuiltInSave, false);
}

void presets::markCurrent(std::string_view name, bool builtIn)
{
  Mod::get()->setSavedValue(kCurrentSave, std::string(name));
  Mod::get()->setSavedValue(kCurrentBuiltInSave, builtIn);
  Mod::get()->setSavedValue(kBaselineSave, capture("").settings);
}

namespace
{
  // Two looks have the same value for every look setting
  bool sameLook(matjson::Value const &look, matjson::Value const &baseline)
  {
    for (auto key : lookSettingKeys())
    {
      auto now = look.get(key);
      auto then = baseline.get(key);
      if (!now || !then)
      {
        if (static_cast<bool>(now) != static_cast<bool>(then))
          return false;
        continue;
      }
      if (!sameValue(now.unwrap(), then.unwrap()))
        return false;
    }
    return true;
  }
}

bool presets::modified()
{
  auto const baseline = Mod::get()->getSavedValue<matjson::Value>(kBaselineSave, matjson::Value::object());
  return !sameLook(capture("").settings, baseline);
}

void presets::adoptMatchingPreset()
{
  if (!current().empty())
    return;
  auto const look = capture("").settings;
  for (auto const &preset : list())
  {
    if (sameLook(look, withDefaults(preset.settings)))
    {
      markCurrent(preset.name, false);
      return;
    }
  }
}

void presets::load(Preset const &preset)
{
  apply(preset);
  markCurrent(preset.name, preset.builtIn);
  if (!preset.builtIn)
    touch(preset.name);
}

std::vector<std::string> presets::recent()
{
  return Mod::get()->getSavedValue<std::vector<std::string>>(kRecentSave, {});
}

void presets::stashMainLook()
{
  auto stash = matjson::Value::object();
  stash["look"] = capture("").settings;
  stash["name"] = current();
  stash["builtIn"] = currentIsBuiltIn();
  stash["baseline"] = Mod::get()->getSavedValue<matjson::Value>(kBaselineSave, matjson::Value::object());
  Mod::get()->setSavedValue(kStashSave, stash);
}

bool presets::hasStashedLook()
{
  auto const stash = Mod::get()->getSavedValue<matjson::Value>(kStashSave, matjson::Value());
  return stash.isObject() && stash.contains("look");
}

void presets::restoreMainLook()
{
  auto const stash = Mod::get()->getSavedValue<matjson::Value>(kStashSave, matjson::Value());
  if (!stash.isObject())
    return;

  if (auto look = stash.get("look"); look && look.unwrap().isObject())
  {
    Preset preset;
    preset.settings = look.unwrap();
    apply(preset);
  }
  std::string name;
  if (auto value = stash.get("name"); value && value.unwrap().isString())
    name = value.unwrap().asString().unwrap();
  bool builtIn = false;
  if (auto value = stash.get("builtIn"); value && value.unwrap().isBool())
    builtIn = value.unwrap().asBool().unwrap();
  Mod::get()->setSavedValue(kCurrentSave, name);
  Mod::get()->setSavedValue(kCurrentBuiltInSave, builtIn);
  if (auto value = stash.get("baseline"); value && value.unwrap().isObject())
    Mod::get()->setSavedValue(kBaselineSave, value.unwrap());
  Mod::get()->setSavedValue(kStashSave, matjson::Value());
}

Result<> presets::writeFile(Preset const &preset, std::filesystem::path const &path)
{
  return file::writeStringSafe(path, toJson(preset).dump());
}

// ! --- Surprise --- !

namespace
{
  // Hair color and an accent for ties, bows, clips and flowers that go well together
  struct Palette
  {
    char const *hair;
    char const *accent;
  };

  constexpr std::array kPalettes{
      Palette{"#262A4D", "#E0405A"}, // navy and red
      Palette{"#7A4E3A", "#FFB7C5"}, // brown and pink
      Palette{"#2B1B17", "#4FA3E0"}, // almost black and blue
      Palette{"#F2C14E", "#E0405A"}, // blonde and red
      Palette{"#E87BA8", "#FFFFFF"}, // pink and white
      Palette{"#C7D3E6", "#8FB3FF"}, // silver and light blue
      Palette{"#5B3A8C", "#FFD84A"}, // purple and yellow
      Palette{"#FF9EB5", "#7A4E3A"}, // light pink and brown
      Palette{"#3A2A80", "#FF6FAE"}, // indigo and hot pink
      Palette{"#A0522D", "#6DBE5A"}, // auburn and green
  };
}

Preset presets::surprise()
{
  std::mt19937 rng(std::random_device{}());
  auto chance = [&](float probability)
  { return std::uniform_real_distribution<float>(0.f, 1.f)(rng) < probability; };
  auto between = [&](float low, float high)
  { return std::uniform_real_distribution<float>(low, high)(rng); };
  auto pick = [&](std::initializer_list<char const *> options)
  {
    auto const index = std::uniform_int_distribution<size_t>(0, options.size() - 1)(rng);
    return *(options.begin() + index);
  };
  auto round = [](float value, float step)
  { return std::round(value / step) * step; };

  Preset preset = capture("Surprise");
  auto &set = preset.settings;
  auto const &palette = kPalettes[std::uniform_int_distribution<size_t>(0, kPalettes.size() - 1)(rng)];

  // Hairstyle
  set["color-source"] = "Custom";
  set["custom-color"] = palette.hair;
  set["style"] = pick({"Flowing", "Flowing", "Long"});
  set["density"] = static_cast<int>(between(28.f, 56.f));
  set["hair-length"] = round(between(12.f, 30.f), .5f);
  set["lock-width"] = round(between(2.2f, 3.2f), .1f);
  set["hair-shine"] = chance(.6f);
  set["dyed-tips"] = chance(.25f);
  set["tips-color"] = "Custom";
  set["tips-custom-color"] = palette.accent;

  // Front
  set["bangs"] = chance(.85f);
  set["bangs-length"] = round(between(7.f, 11.f), .5f);
  set["bangs-density"] = static_cast<int>(between(7.f, 13.f));
  set["bangs-arc-size"] = round(between(0.f, 4.f), .5f);
  set["face-locks"] = chance(.5f);
  set["braid-face-locks"] = chance(.3f);
  set["clip-count"] = chance(.3f) ? static_cast<int>(between(1.f, 2.99f)) : 0;
  set["clip-style"] = pick({"Star", "Heart", "X pin", "Bar"});
  set["clip-color"] = "Custom";
  set["clip-custom-color"] = palette.accent;

  // Extras
  set["ponytail"] = pick({"None", "None", "Ponytail", "Twin tails"});
  set["braid-tails"] = chance(.35f);
  set["ponytail-color"] = "Hair";
  set["ponytail-tie"] = pick({"Scrunchie", "Bow"});
  set["tie-color"] = "Custom";
  set["tie-custom-color"] = palette.accent;
  set["ahoge"] = chance(.3f) ? 1 : 0;
  bool const flowers = chance(.25f);
  set["flowers"] = flowers ? pick({"Sakura", "Daisy", "Crown"}) : "None";
  set["flower-color"] = "Custom";
  set["flower-custom-color"] = palette.accent;
  set["head-bow"] = !flowers && chance(.25f);
  set["bow-color"] = "Custom";
  set["bow-custom-color"] = palette.accent;
  set["ears"] = chance(.15f) ? pick({"Cat", "Bunny", "Fox"}) : "None";
  set["ear-color"] = "Hair";
  set["headband"] = chance(.15f);
  set["headband-color"] = "Custom";
  set["headband-custom-color"] = palette.accent;
  set["headband-deco"] = pick({"None", "Bow", "Cat ears"});
  set["halo"] = chance(.08f);
  set["scarf"] = chance(.15f);
  set["scarf-color"] = "Custom";
  set["scarf-custom-color"] = palette.accent;
  set["streaks"] = chance(.2f) ? static_cast<int>(between(1.f, 3.99f)) : 0;
  set["streak-placement"] = pick({"Bangs", "Bangs", "Face locks", "Front", "Scattered"});
  set["streak-color"] = palette.accent;
  set["headphones"] = chance(.1f) ? pick({"Plain", "Cat ears"}) : "None";
  set["headphones-light"] = palette.accent;
  set["glasses"] = chance(.12f) ? pick({"Round", "Hearts", "Stars"}) : "None";
  set["earrings"] = chance(.15f) ? pick({"Drops", "Hearts", "Stars", "Pearls"}) : "None";
  set["earring-color"] = "Custom";
  set["earring-custom-color"] = palette.accent;
  set["bell"] = chance(.1f);

  // Effects
  set["blush"] = chance(.7f);
  bool const sparkles = chance(.25f);
  set["sparkles"] = sparkles ? pick({"Hearts", "Sparkles", "Both"}) : "None";
  set["petals"] = !sparkles && chance(.15f);

  return preset;
}

Preset presets::surpriseColors()
{
  std::mt19937 rng(std::random_device{}());
  auto const &palette = kPalettes[std::uniform_int_distribution<size_t>(0, kPalettes.size() - 1)(rng)];

  Preset preset = capture("Colors");
  auto &set = preset.settings;
  set["color-source"] = "Custom";
  set["custom-color"] = palette.hair;

  // Every accessory with a custom color gets the accent
  for (auto [source, custom] : {std::pair{"tie-color", "tie-custom-color"}, std::pair{"bow-color", "bow-custom-color"},
                                std::pair{"clip-color", "clip-custom-color"}, std::pair{"flower-color", "flower-custom-color"},
                                std::pair{"headband-color", "headband-custom-color"}, std::pair{"scarf-color", "scarf-custom-color"},
                                std::pair{"tips-color", "tips-custom-color"}, std::pair{"hat-color", "hat-custom-color"},
                                std::pair{"earring-color", "earring-custom-color"}})
  {
    set[source] = "Custom";
    set[custom] = palette.accent;
  }
  set["streak-color"] = palette.accent;
  set["headphones-light"] = palette.accent;
  return preset;
}

void presets::applyEmpty()
{
  for (auto key : lookSettingKeys())
    settings::reset(key);
  settings::set("enabled", false);
  markCurrent("", false);
}

Preset presets::matchColors()
{
  Preset preset = capture("Match");
  for (auto key : {"face-lock-color", "bangs-color", "ponytail-color", "ear-color"})
    preset.settings[key] = "Hair";
  return preset;
}

