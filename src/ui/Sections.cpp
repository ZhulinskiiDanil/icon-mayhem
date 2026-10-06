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
           "hair-top-gap",
           "look-title",
           "color-source",
           "custom-color",
           "outline",
           "hair-shine",
           "shine-position",
           "shine-strength",
           "dyed-tips",
           "tips-color",
           "tips-custom-color",
           "tips-start",
       }},
      {"Front",
       {
           "face-locks-title",
           "face-locks",
           "face-lock-left",
           "face-lock-right",
           "face-lock-length",
           "face-lock-width",
           "braid-face-locks",
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
           "clips-title",
           "clip-count",
           "clip-style",
           "clip-side",
           "clip-size",
           "clip-color",
           "clip-custom-color",
       }},
      {"Extras",
       {
           "tails-title",
           "ponytail",
           "ponytail-position",
           "ponytail-length",
           "ponytail-thickness",
           "braid-tails",
           "ponytail-color",
           "ponytail-custom-color",
           "ponytail-tie",
           "tie-color",
           "tie-custom-color",
           "ahoge-title",
           "ahoge",
           "ahoge-length",
           "ahoge-curl",
           "bow-title",
           "head-bow",
           "bow-position",
           "bow-size",
           "bow-ribbon-length",
           "bow-color",
           "bow-custom-color",
           "scarf-title",
           "scarf",
           "scarf-height",
           "scarf-width",
           "scarf-length",
           "scarf-color",
           "scarf-custom-color",
           "ears-title",
           "ears",
           "ear-size",
           "ear-spread",
           "ear-twitch",
           "ear-color",
           "ear-custom-color",
           "ear-inner-color",
           "headband-title",
           "headband",
           "headband-inset",
           "headband-width",
           "headband-color",
           "headband-custom-color",
           "headband-deco",
           "flowers-title",
           "flowers",
           "flower-position",
           "flower-size",
           "flower-color",
           "flower-custom-color",
           "halo-title",
           "halo",
           "halo-height",
           "halo-size",
           "halo-color",
           "halo-glow",
           "wings-title",
           "wings",
           "wing-size",
           "wing-flap",
           "wing-color",
           "wing-custom-color",
           "pet-title",
           "pet",
           "pet-size",
           "pet-distance",
           "pet-color",
           "pet-custom-color",
           "hat-title",
           "hat",
           "hat-size",
           "hat-tilt",
           "hat-inset",
           "hat-color",
           "hat-custom-color",
       }},
      {"Effects",
       {
           "blush-title",
           "blush",
           "blush-color",
           "blush-opacity",
           "blush-size",
           "blush-spread",
           "blush-height",
           "blush-lines",
           "blush-pop",
           "sticker-title",
           "sticker",
           "sticker-side",
           "sticker-x",
           "sticker-y",
           "sticker-size",
           "sticker-color",
           "reactions-title",
           "reactions",
           "cute-death",
           "sparkles-title",
           "sparkles",
           "sparkle-rate",
           "sparkle-on-landing",
           "sparkle-size",
           "sparkle-color",
           "sparkle-custom-color",
           "petals-title",
           "petals",
           "petal-style",
           "petal-amount",
           "petal-color",
           "sleepy-title",
           "sleepy",
           "sleepy-delay",
       }},
      {"Physics",
       {
           "volume",
           "hitbox-multiplier",
           "gravity",
           "damping",
           "hair-friction",
           "calm-jumps",
           "wind-title",
           "wind-multiplier",
           "wind-gusts",
           "wind-gust-speed",
           "wind-flutter",
           "breeze",
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
