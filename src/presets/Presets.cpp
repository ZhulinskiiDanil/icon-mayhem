#include "Presets.hpp"

#include "../ui/Sections.hpp"

#include <algorithm>
#include <cmath>

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
