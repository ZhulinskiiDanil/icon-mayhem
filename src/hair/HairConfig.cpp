#include "HairConfig.hpp"

#include <algorithm>

using namespace geode::prelude;

// ! --- Settings --- !

namespace
{
  unsigned s_version = 0;

  HairColorSource parseColorSource(std::string const &value)
  {
    if (value == "Hair")
      return HairColorSource::Hair;
    if (value == "Primary")
      return HairColorSource::Primary;
    if (value == "Custom")
      return HairColorSource::Custom;
    return HairColorSource::Secondary;
  }

  HairStyle parseStyle(std::string const &value)
  {
    if (value == "Long")
      return HairStyle::Long;
    if (value == "Spiky")
      return HairStyle::Spiky;
    return HairStyle::Flowing;
  }
}

HairConfig HairConfig::load()
{
  auto mod = Mod::get();
  HairConfig cfg;

  cfg.enabled = mod->getSettingValue<bool>("enabled");
  cfg.showInGarage = mod->getSettingValue<bool>("show-in-garage");
  cfg.style = parseStyle(mod->getSettingValue<std::string>("style"));
  cfg.spinWithIcon = mod->getSettingValue<bool>("spin-with-icon");
  cfg.lockCount = static_cast<int>(mod->getSettingValue<int64_t>("density"));
  cfg.length = static_cast<float>(mod->getSettingValue<double>("hair-length"));
  cfg.segments = static_cast<int>(mod->getSettingValue<int64_t>("segments"));
  cfg.lockWidth = static_cast<float>(mod->getSettingValue<double>("lock-width"));
  cfg.volume = static_cast<float>(mod->getSettingValue<double>("volume"));

  auto number = [&](std::string_view key)
  { return static_cast<float>(mod->getSettingValue<double>(key)); };

  cfg.faceLocks = mod->getSettingValue<bool>("face-locks");
  cfg.faceLockLeft = mod->getSettingValue<bool>("face-lock-left");
  cfg.faceLockRight = mod->getSettingValue<bool>("face-lock-right");
  cfg.faceLockLength = number("face-lock-length");
  cfg.faceLockWidth = number("face-lock-width");
  cfg.faceLockInsetX = number("face-lock-inset-x");
  cfg.faceLockInsetY = number("face-lock-inset-y");
  cfg.faceLockColorSource = parseColorSource(mod->getSettingValue<std::string>("face-lock-color"));
  cfg.faceLockColor = mod->getSettingValue<ccColor3B>("face-lock-custom-color");

  cfg.bangs = mod->getSettingValue<bool>("bangs");
  cfg.bangsLength = number("bangs-length");
  cfg.bangsCount = static_cast<int>(mod->getSettingValue<int64_t>("bangs-density"));
  cfg.bangsSpread = number("bangs-spread");
  cfg.bangsArcSize = number("bangs-arc-size");
  cfg.bangsArcSoftness = number("bangs-arc-softness");
  cfg.bangsInsetX = number("bangs-inset-x");
  cfg.bangsInsetY = number("bangs-inset-y");
  cfg.bangsColorSource = parseColorSource(mod->getSettingValue<std::string>("bangs-color"));
  cfg.bangsColor = mod->getSettingValue<ccColor3B>("bangs-custom-color");

  cfg.hitboxMultiplier = static_cast<float>(mod->getSettingValue<double>("hitbox-multiplier"));
  cfg.windMultiplier = static_cast<float>(mod->getSettingValue<double>("wind-multiplier"));
  cfg.gravity = static_cast<float>(mod->getSettingValue<double>("gravity"));
  cfg.damping = static_cast<float>(mod->getSettingValue<double>("damping"));
  cfg.colorSource = parseColorSource(mod->getSettingValue<std::string>("color-source"));
  cfg.customColor = mod->getSettingValue<ccColor3B>("custom-color");
  cfg.outline = mod->getSettingValue<bool>("outline");

  cfg.lockCount = std::clamp(cfg.lockCount, 1, 256);
  cfg.segments = std::clamp(cfg.segments, 3, 16);
  cfg.bangsCount = std::clamp(cfg.bangsCount, 1, 32);

  return cfg;
}

unsigned HairConfig::version()
{
  return s_version;
}

void HairConfig::bumpVersion()
{
  ++s_version;
}
