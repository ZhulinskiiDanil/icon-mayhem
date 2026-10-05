#include "HairConfig.hpp"

#include <algorithm>

using namespace geode::prelude;

// ! --- Settings --- !

namespace
{
  unsigned s_version = 0;

  HairColorSource parseColorSource(std::string const &value)
  {
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
  cfg.gravity = static_cast<float>(mod->getSettingValue<double>("gravity"));
  cfg.damping = static_cast<float>(mod->getSettingValue<double>("damping"));
  cfg.colorSource = parseColorSource(mod->getSettingValue<std::string>("color-source"));
  cfg.customColor = mod->getSettingValue<ccColor3B>("custom-color");
  cfg.outline = mod->getSettingValue<bool>("outline");

  cfg.lockCount = std::clamp(cfg.lockCount, 1, 256);
  cfg.segments = std::clamp(cfg.segments, 3, 16);

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
