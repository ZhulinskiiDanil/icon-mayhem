#include "Sections.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <array>

using namespace geode::prelude;

// ! --- Sections --- !

std::vector<CustomizerSection> const &customizerSections()
{
  static std::vector<CustomizerSection> const list{
      {"Hair",
       {
           "enabled",
           "show-in-garage",
           "style",
           "spin-with-icon",
           "density",
           "hair-length",
           "segments",
           "lock-width",
           "physics-title",
           "volume",
           "hitbox-multiplier",
           "wind-multiplier",
           "gravity",
           "damping",
           "look-title",
           "color-source",
           "custom-color",
           "outline",
       }},
      {"Front",
       {
           "face-locks-title",
           "face-locks",
           "face-lock-left",
           "face-lock-right",
           "face-lock-length",
           "face-lock-width",
           "face-lock-inset-x",
           "face-lock-inset-y",
           "face-lock-color",
           "face-lock-custom-color",
           "bangs-title",
           "bangs",
           "bangs-length",
           "bangs-density",
           "bangs-spread",
           "bangs-arc-size",
           "bangs-arc-softness",
           "bangs-inset-x",
           "bangs-inset-y",
           "bangs-color",
           "bangs-custom-color",
       }},
  };
  return list;
}

// ! --- Look settings --- !

std::vector<std::string_view> const &lookSettingKeys()
{
  static std::vector<std::string_view> const keys = []
  {
    // Turning the mod on and off isn't part of a look
    static constexpr std::array kNotLook{std::string_view("enabled"), std::string_view("show-in-garage")};

    std::vector<std::string_view> list;
    for (auto const &section : customizerSections())
    {
      for (std::string_view key : section.keys)
      {
        if (std::find(kNotLook.begin(), kNotLook.end(), key) != kNotLook.end())
          continue;
        if (typeinfo_pointer_cast<TitleSettingV3>(Mod::get()->getSetting(key)))
          continue;
        list.push_back(key);
      }
    }
    return list;
  }();
  return keys;
}
