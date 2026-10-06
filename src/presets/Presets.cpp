#include "Presets.hpp"

#include "../ui/Sections.hpp"

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


  matjson::Value settingValue(std::shared_ptr<SettingV3> const &setting)
  {
    if (auto bool_ = typeinfo_pointer_cast<BoolSettingV3>(setting))
      return bool_->getValue();
    if (auto int_ = typeinfo_pointer_cast<IntSettingV3>(setting))
      return int_->getValue();
    if (auto float_ = typeinfo_pointer_cast<FloatSettingV3>(setting))
      return float_->getValue();
    if (auto string = typeinfo_pointer_cast<StringSettingV3>(setting))
      return string->getValue();
    if (auto color = typeinfo_pointer_cast<Color3BSettingV3>(setting))
      return "#" + cc3bToHexString(color->getValue());
    return nullptr;
  }

  // Writes a preset value into a setting, false if it doesn't fit (wrong type, unknown option)
  bool setSettingValue(std::shared_ptr<SettingV3> const &setting, matjson::Value const &value)
  {
    if (auto bool_ = typeinfo_pointer_cast<BoolSettingV3>(setting))
    {
      if (!value.isBool())
        return false;
      bool_->setValue(value.asBool().unwrap());
      return true;
    }
    if (auto int_ = typeinfo_pointer_cast<IntSettingV3>(setting))
    {
      if (!value.isNumber())
        return false;
      auto number = static_cast<int64_t>(std::llround(value.asDouble().unwrap()));
      number = std::clamp(number, int_->getMinValue().value_or(number), int_->getMaxValue().value_or(number));
      int_->setValue(number);
      return true;
    }
    if (auto float_ = typeinfo_pointer_cast<FloatSettingV3>(setting))
    {
      if (!value.isNumber())
        return false;
      double number = value.asDouble().unwrap();
      number = std::clamp(number, float_->getMinValue().value_or(number), float_->getMaxValue().value_or(number));
      float_->setValue(number);
      return true;
    }
    if (auto string = typeinfo_pointer_cast<StringSettingV3>(setting))
    {
      if (!value.isString())
        return false;
      auto text = value.asString().unwrap();
      if (auto options = string->getEnumOptions();
          options && std::find(options->begin(), options->end(), text) == options->end())
        return false;
      string->setValue(text);
      return true;
    }
    if (auto color = typeinfo_pointer_cast<Color3BSettingV3>(setting))
    {
      if (!value.isString())
        return false;
      auto text = value.asString().unwrap();
      if (!text.empty() && text.front() == '#')
        text.erase(0, 1);
      auto parsed = cc3bFromHexString(text, true);
      if (!parsed)
        return false;
      color->setValue(parsed.unwrap());
      return true;
    }
    return false;
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
  {
    if (auto setting = Mod::get()->getSetting(key))
      preset.settings[key] = settingValue(setting);
  }
  return preset;
}

void presets::apply(Preset const &preset)
{
  for (auto key : lookSettingKeys())
  {
    auto setting = Mod::get()->getSetting(key);
    if (!setting)
      continue;

    auto value = preset.settings.get(key);
    if (!value || !setSettingValue(setting, value.unwrap()))
      setting->reset();
  }
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

Result<> presets::save(Preset const &preset)
{
  GEODE_UNWRAP(file::createDirectoryAll(folder()));
  return writeFile(preset, pathOf(preset.name));
}

Result<> presets::remove(std::string_view name)
{
  std::error_code error;
  std::filesystem::remove(pathOf(name), error);
  if (error)
    return Err("Unable to delete the preset: {}", error.message());
  return Ok();
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
                                std::pair{"tips-color", "tips-custom-color"}, std::pair{"hat-color", "hat-custom-color"}})
  {
    set[source] = "Custom";
    set[custom] = palette.accent;
  }
  return preset;
}

Preset presets::matchColors()
{
  Preset preset = capture("Match");
  for (auto key : {"face-lock-color", "bangs-color", "ponytail-color", "ear-color"})
    preset.settings[key] = "Hair";
  return preset;
}

