#include "Settings.hpp"

#include "../hair/HairConfig.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <system_error>

using namespace geode::prelude;

// ! --- State --- !

namespace
{
  constexpr char const *kDefinitions = "look-settings.json";
  constexpr char const *kFile = "look.json";
  constexpr char const *kMigrated = "look-file-migrated";
  constexpr float kTick = .25f;
  constexpr float kSaveDelay = .5f; // a slider drag writes the file once, after it stops

  constexpr char const *kHelp =
      "Your icon look. Edit a value and save the file, the game picks it up in a moment. "
      "Only values that differ from the defaults are here: delete a line to put it back to its default. "
      "Every setting with its options, range and default: look-settings.json in the mod resources";

  std::vector<settings::Def> s_defs;            // the look file, in order
  std::map<std::string, size_t, std::less<>> s_index; // key -> s_defs
  std::vector<std::string> s_keys;
  std::map<std::string, settings::Def, std::less<>> s_geodeDefs; // made on first use
  std::map<std::string, matjson::Value, std::less<>> s_values;   // only what differs from the defaults

  bool s_dirty = false;
  float s_sinceChange = 0.f;
  std::filesystem::file_time_type s_writeTime{}; // of the file as we wrote or read it last
  unsigned s_revision = 0;

  std::filesystem::path s_path;

  double roundNumber(double value)
  {
    return std::round(value * 1e4) / 1e4;
  }

  bool sameValue(matjson::Value const &a, matjson::Value const &b)
  {
    if (a.isNumber() && b.isNumber())
      return std::abs(a.asDouble().unwrap() - b.asDouble().unwrap()) < 1e-4;
    if (a.isString() && b.isString())
      return utils::string::toLower(a.asString().unwrap()) == utils::string::toLower(b.asString().unwrap());
    return a == b;
  }

  std::optional<ccColor3B> parseColor(matjson::Value const &value)
  {
    if (value.isString())
    {
      auto text = value.asString().unwrap();
      if (!text.empty() && text.front() == '#')
        text.erase(0, 1);
      if (auto parsed = cc3bFromHexString(text, true))
        return parsed.unwrap();
      return std::nullopt;
    }
    if (value.isObject())
    {
      auto channel = [&](char const *name) -> std::optional<GLubyte>
      {
        auto part = value.get(name);
        if (!part || !part.unwrap().isNumber())
          return std::nullopt;
        return static_cast<GLubyte>(std::clamp<int64_t>(part.unwrap().asInt().unwrapOr(0), 0, 255));
      };
      auto r = channel("r"), g = channel("g"), b = channel("b");
      if (r && g && b)
        return ccColor3B{*r, *g, *b};
    }
    return std::nullopt;
  }

  std::string colorText(ccColor3B color)
  {
    return "#" + utils::string::toLower(cc3bToHexString(color));
  }

  // ! --- Definitions --- !

  settings::Type parseType(std::string_view type)
  {
    if (type == "title")
      return settings::Type::Title;
    if (type == "bool")
      return settings::Type::Bool;
    if (type == "int")
      return settings::Type::Int;
    if (type == "float")
      return settings::Type::Float;
    if (type == "string")
      return settings::Type::Choice;
    if (type == "color")
      return settings::Type::Color;
    return settings::Type::Other;
  }

  void readDefinitions()
  {
    auto path = Mod::get()->getResourcesDir() / kDefinitions;
    auto json = file::readJson(path);
    if (!json || !json.unwrap().isObject())
    {
      log::error("Settings: can't read {}: {}", path, json ? "not an object" : json.unwrapErr());
      return;
    }

    for (auto const &entry : json.unwrap())
    {
      settings::Def def;
      def.key = entry.getKey().value_or("");
      def.type = parseType(entry["type"].asString().unwrapOr(""));
      def.name = entry["name"].asString().unwrapOr(def.key);
      def.description = entry["description"].asString().unwrapOr("");
      def.fallback = entry.contains("default") ? entry["default"] : matjson::Value(nullptr);
      def.min = entry["min"].asDouble().unwrapOr(0.0);
      def.max = entry["max"].asDouble().unwrapOr(def.type == settings::Type::Int ? 100.0 : 1.0);
      if (entry.contains("control"))
        def.step = entry["control"]["slider-step"].asDouble().unwrapOr(def.type == settings::Type::Int ? 1.0 : .1);
      else
        def.step = def.type == settings::Type::Int ? 1.0 : .1;
      if (entry.contains("one-of"))
      {
        for (auto const &option : entry["one-of"])
          def.options.push_back(option.asString().unwrapOr(""));
      }
      if (def.key.empty() || def.type == settings::Type::Other)
      {
        log::warn("Settings: skipping the definition '{}'", def.key);
        continue;
      }

      // Defaults in the same form as values
      if (def.type == settings::Type::Color)
        def.fallback = colorText(parseColor(def.fallback).value_or(ccColor3B{255, 255, 255}));
      else if (def.type == settings::Type::Int)
        def.fallback = static_cast<int64_t>(def.fallback.asInt().unwrapOr(0));
      else if (def.type == settings::Type::Float)
        def.fallback = roundNumber(def.fallback.asDouble().unwrapOr(0.0));

      s_index[def.key] = s_defs.size();
      s_keys.push_back(def.key);
      s_defs.push_back(std::move(def));
    }
  }

  settings::Def const *geodeDef(std::string_view key)
  {
    if (auto found = s_geodeDefs.find(key); found != s_geodeDefs.end())
      return &found->second;

    auto setting = Mod::get()->getSetting(key);
    if (!setting)
      return nullptr;

    settings::Def def;
    def.key = std::string(key);
    def.name = setting->getDisplayName();
    def.description = setting->getDescription().value_or("");
    def.inGeode = true;
    if (typeinfo_pointer_cast<TitleSettingV3>(setting))
      def.type = settings::Type::Title;
    else if (auto bool_ = typeinfo_pointer_cast<BoolSettingV3>(setting))
    {
      def.type = settings::Type::Bool;
      def.fallback = bool_->getDefaultValue();
    }
    else if (auto int_ = typeinfo_pointer_cast<IntSettingV3>(setting))
    {
      def.type = settings::Type::Int;
      def.fallback = int_->getDefaultValue();
      def.min = static_cast<double>(int_->getMinValue().value_or(0));
      def.max = static_cast<double>(int_->getMaxValue().value_or(100));
      def.step = static_cast<double>(std::max<int64_t>(int_->getSliderSnap(), 1));
    }
    else if (auto float_ = typeinfo_pointer_cast<FloatSettingV3>(setting))
    {
      def.type = settings::Type::Float;
      def.fallback = float_->getDefaultValue();
      def.min = float_->getMinValue().value_or(0.0);
      def.max = float_->getMaxValue().value_or(1.0);
      def.step = float_->getSliderSnap();
    }
    else if (auto string = typeinfo_pointer_cast<StringSettingV3>(setting))
    {
      def.type = settings::Type::Choice;
      def.fallback = string->getDefaultValue();
      if (auto options = string->getEnumOptions())
        def.options = *options;
    }
    else if (auto color = typeinfo_pointer_cast<Color3BSettingV3>(setting))
    {
      def.type = settings::Type::Color;
      def.fallback = colorText(color->getDefaultValue());
    }
    return &s_geodeDefs.emplace(std::string(key), std::move(def)).first->second;
  }

  // ! --- Geode's settings --- !

  matjson::Value geodeValue(std::string_view key)
  {
    auto setting = Mod::get()->getSetting(key);
    if (auto bool_ = typeinfo_pointer_cast<BoolSettingV3>(setting))
      return bool_->getValue();
    if (auto int_ = typeinfo_pointer_cast<IntSettingV3>(setting))
      return int_->getValue();
    if (auto float_ = typeinfo_pointer_cast<FloatSettingV3>(setting))
      return float_->getValue();
    if (auto string = typeinfo_pointer_cast<StringSettingV3>(setting))
      return string->getValue();
    if (auto color = typeinfo_pointer_cast<Color3BSettingV3>(setting))
      return colorText(color->getValue());
    return nullptr;
  }

  void setGeodeValue(std::string_view key, matjson::Value const &value)
  {
    auto setting = Mod::get()->getSetting(key);
    if (auto bool_ = typeinfo_pointer_cast<BoolSettingV3>(setting))
      bool_->setValue(value.asBool().unwrapOr(false));
    else if (auto int_ = typeinfo_pointer_cast<IntSettingV3>(setting))
      int_->setValue(value.asInt().unwrapOr(0));
    else if (auto float_ = typeinfo_pointer_cast<FloatSettingV3>(setting))
      float_->setValue(value.asDouble().unwrapOr(0.0));
    else if (auto string = typeinfo_pointer_cast<StringSettingV3>(setting))
      string->setValue(value.asString().unwrapOr(""));
    else if (auto color = typeinfo_pointer_cast<Color3BSettingV3>(setting))
      color->setValue(parseColor(value).value_or(ccColor3B{255, 255, 255}));
  }

  // ! --- The file --- !

  std::filesystem::file_time_type writeTimeOf(std::filesystem::path const &path)
  {
    std::error_code error;
    auto time = std::filesystem::last_write_time(path, error);
    return error ? std::filesystem::file_time_type{} : time;
  }

  void save()
  {
    auto json = matjson::Value::object();
    json["$help"] = kHelp;
    for (auto const &key : s_keys)
    {
      if (auto found = s_values.find(key); found != s_values.end())
        json[key] = found->second;
    }

    std::error_code error;
    std::filesystem::create_directories(s_path.parent_path(), error);
    if (auto written = file::writeString(s_path, json.dump(2)); !written)
    {
      log::error("Settings: can't write {}: {}", s_path, written.unwrapErr());
      return;
    }
    s_writeTime = writeTimeOf(s_path);
    s_dirty = false;
  }

  // Values of a look file (or Geode's old settings), what doesn't fit is left out
  std::map<std::string, matjson::Value, std::less<>> readValues(matjson::Value const &json, std::vector<std::string> *problems)
  {
    std::map<std::string, matjson::Value, std::less<>> out;
    for (auto const &entry : json)
    {
      auto key = entry.getKey().value_or("");
      if (key.empty() || key.starts_with('$'))
        continue;
      auto found = s_index.find(key);
      if (found == s_index.end() || s_defs[found->second].type == settings::Type::Title)
      {
        if (problems && found == s_index.end())
          problems->push_back(fmt::format("unknown setting \"{}\"", key));
        continue;
      }
      auto const &def = s_defs[found->second];
      auto value = settings::normalize(def, entry);
      if (!value)
      {
        if (problems)
          problems->push_back(fmt::format("\"{}\" can't be {}", key, entry.dump(matjson::NO_INDENTATION)));
        continue;
      }
      if (!sameValue(*value, def.fallback))
        out[key] = std::move(*value);
    }
    return out;
  }

  // `announce`: tell the player, it was edited by hand while the game runs
  void reload(bool announce)
  {
    s_writeTime = writeTimeOf(s_path);
    auto text = file::readString(s_path);
    if (!text)
      return;
    auto json = matjson::parse(text.unwrap());
    if (!json || !json.unwrap().isObject())
    {
      auto reason = json ? std::string("not a JSON object") : json.unwrapErr().message;
      log::warn("Settings: look.json not applied, {}", reason);
      if (announce)
        Notification::create(fmt::format("look.json: {}", reason), NotificationIcon::Error, 3.f)->show();
      return;
    }

    std::vector<std::string> problems;
    s_values = readValues(json.unwrap(), &problems);
    s_dirty = false;
    ++s_revision;
    HairConfig::bumpVersion();

    for (auto const &problem : problems)
      log::warn("Settings: look.json, {}", problem);
    if (!announce)
      return;
    if (problems.empty())
      Notification::create("look.json applied", NotificationIcon::Success, 1.5f)->show();
    else
      Notification::create(fmt::format("look.json applied, {} skipped: {}", problems.size(), problems.front()),
                           NotificationIcon::Warning, 3.f)
          ->show();
  }

  // The values from the time the look lived in Geode's settings
  void migrate()
  {
    auto old = file::readJson(Mod::get()->getSaveDir() / "settings.json");
    if (old && old.unwrap().isObject())
    {
      s_values = readValues(old.unwrap(), nullptr);
      log::info("Settings: moved {} values from Geode's settings into {}", s_values.size(), kFile);
    }
    Mod::get()->setSavedValue(kMigrated, true);
    save();
  }

  // Saves after changes and reads hand edits, on the global scheduler so it runs in every scene
  class Watcher : public CCObject
  {
  public:
    void tick(float dt)
    {
      auto const time = writeTimeOf(s_path);
      if (time != std::filesystem::file_time_type{} && time != s_writeTime)
      {
        reload(true);
        return;
      }

      if (s_dirty)
      {
        s_sinceChange += dt;
        if (s_sinceChange >= kSaveDelay)
          save();
      }
    }
  };

  void changed()
  {
    s_dirty = true;
    s_sinceChange = 0.f;
    HairConfig::bumpVersion();
  }
}

// ! --- Loading --- !

void settings::load()
{
  s_path = Mod::get()->getConfigDir() / kFile;
  readDefinitions();

  if (std::filesystem::exists(s_path))
    reload(false);
  else if (!Mod::get()->getSavedValue<bool>(kMigrated, false))
    migrate();
  else
    save();

  auto watcher = new Watcher();
  // Lives as long as the game
  CCScheduler::get()->scheduleSelector(schedule_selector(Watcher::tick), watcher, kTick, false);
}

std::filesystem::path settings::filePath()
{
  return s_path;
}

unsigned settings::revision()
{
  return s_revision;
}

void settings::flush()
{
  if (s_dirty)
    save();
}

// ! --- Definitions --- !

settings::Def const *settings::def(std::string_view key)
{
  if (auto found = s_index.find(key); found != s_index.end())
    return &s_defs[found->second];
  return geodeDef(key);
}

std::vector<std::string> const &settings::fileKeys()
{
  return s_keys;
}

// ! --- Values --- !

std::optional<matjson::Value> settings::normalize(Def const &def, matjson::Value const &value)
{
  switch (def.type)
  {
  case Type::Bool:
    if (!value.isBool())
      return std::nullopt;
    return value;
  case Type::Int:
  {
    if (!value.isNumber())
      return std::nullopt;
    auto number = static_cast<int64_t>(std::llround(value.asDouble().unwrap()));
    return std::clamp(number, static_cast<int64_t>(def.min), static_cast<int64_t>(def.max));
  }
  case Type::Float:
    if (!value.isNumber())
      return std::nullopt;
    return roundNumber(std::clamp(value.asDouble().unwrap(), def.min, def.max));
  case Type::Choice:
  {
    if (!value.isString())
      return std::nullopt;
    auto const text = value.asString().unwrap();
    if (def.options.empty())
      return text;
    // Hand edits may get the letter case wrong
    for (auto const &option : def.options)
    {
      if (utils::string::toLower(option) == utils::string::toLower(text))
        return option;
    }
    return std::nullopt;
  }
  case Type::Color:
    if (auto color = parseColor(value))
      return colorText(*color);
    return std::nullopt;
  default:
    return std::nullopt;
  }
}

namespace
{
  // The value of a look setting without copying, nullptr for a Geode one or an unknown key
  matjson::Value const *fileValue(std::string_view key)
  {
    if (auto found = s_values.find(key); found != s_values.end())
      return &found->second;
    if (auto found = s_index.find(key); found != s_index.end())
      return &s_defs[found->second].fallback;
    return nullptr;
  }
}

matjson::Value settings::get(std::string_view key)
{
  if (auto value = fileValue(key))
    return *value;
  if (auto setting = def(key); setting && setting->inGeode)
    return geodeValue(key);
  return nullptr;
}

float settings::number(std::string_view key)
{
  if (auto value = fileValue(key))
    return static_cast<float>(value->asDouble().unwrapOr(0.0));
  return static_cast<float>(get(key).asDouble().unwrapOr(0.0));
}

int settings::integer(std::string_view key)
{
  if (auto value = fileValue(key))
    return static_cast<int>(value->asInt().unwrapOr(0));
  return static_cast<int>(get(key).asInt().unwrapOr(0));
}

bool settings::flag(std::string_view key)
{
  if (auto value = fileValue(key))
    return value->asBool().unwrapOr(false);
  return get(key).asBool().unwrapOr(false);
}

std::string settings::text(std::string_view key)
{
  if (auto value = fileValue(key))
    return value->asString().unwrapOr("");
  return get(key).asString().unwrapOr("");
}

ccColor3B settings::color(std::string_view key)
{
  if (auto value = fileValue(key))
    return parseColor(*value).value_or(ccColor3B{255, 255, 255});
  return parseColor(get(key)).value_or(ccColor3B{255, 255, 255});
}

bool settings::set(std::string_view key, matjson::Value const &value)
{
  auto setting = def(key);
  if (!setting)
    return false;
  auto normal = normalize(*setting, value);
  if (!normal)
    return false;

  if (setting->inGeode)
  {
    // Geode tells listenForAllSettingChanges, which bumps the version
    setGeodeValue(key, *normal);
    return true;
  }

  auto found = s_values.find(key);
  if (sameValue(*normal, setting->fallback))
  {
    if (found == s_values.end())
      return true;
    s_values.erase(found);
  }
  else
  {
    if (found != s_values.end() && sameValue(found->second, *normal))
      return true;
    s_values[std::string(key)] = std::move(*normal);
  }
  changed();
  return true;
}

void settings::reset(std::string_view key)
{
  auto setting = def(key);
  if (!setting || setting->type == Type::Title)
    return;
  if (setting->inGeode)
  {
    if (auto geode = Mod::get()->getSetting(key))
      geode->reset();
    return;
  }
  if (auto found = s_values.find(key); found != s_values.end())
  {
    s_values.erase(found);
    changed();
  }
}

bool settings::isDefault(std::string_view key)
{
  auto setting = def(key);
  if (!setting)
    return true;
  if (setting->inGeode)
  {
    auto geode = Mod::get()->getSetting(key);
    return !geode || geode->isDefaultValue();
  }
  return !s_values.contains(key);
}
