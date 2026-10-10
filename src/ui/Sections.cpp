#include "Sections.hpp"

#include "../settings/Settings.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <array>

using namespace geode::prelude;

// ! --- Sections --- !

std::vector<CustomizerSection> const &customizerSections()
{
  static std::vector<CustomizerSection> const list{
      {"General",
       {
           "customization",
           "show-in-garage",
           "show-in-menus",
           "quality",
           "focus-title",
           "focus-mode",
           "focus-opacity",
           "focus-hair",
           "modes-title",
           "mode-cube",
           "mode-ship",
           "mode-ball",
           "mode-ufo",
           "mode-wave",
           "mode-robot",
           "mode-spider",
           "mode-swing",
           "mode-jetpack",
           "@looks",
           "@linker",
           "@gallery",
           "@look-file",
           "online-title",
           "share-icons",
           "show-icons",
           "emotes-title",
           "@emote-keys",
       }},
      {"Hair",
       {
           "enabled",
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
           "outline-color",
           "hair-shine",
           "shine-position",
           "shine-strength",
           "dyed-tips",
           "tips-color",
           "tips-custom-color",
           "tips-start",
           "streaks",
           "streak-placement",
           "streak-color",
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
           "face-lock-shift-x",
           "face-lock-tilt",
           "face-lock-color",
           "face-lock-custom-color",
           "bangs-title",
           "bangs",
           "bangs-style",
           "bangs-clumps",
           "bangs-fill",
           "bangs-wisps",
           "bangs-side",
           "bangs-transition",
           "bangs-length",
           "bangs-density",
           "bangs-spread",
           "bangs-width",
           "bangs-fan",
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
           "cape-title",
           "cape",
           "cape-length",
           "cape-width",
           "cape-color",
           "cape-custom-color",
           "cape-lining",
           "cape-pattern",
           "pet-title",
           "pet",
           "pet-size",
           "pet-distance",
           "pet-color",
           "pet-custom-color",
           "pet-moods",
           "pet-behavior",
           "hat-title",
           "hat",
           "hat-size",
           "hat-tilt",
           "hat-inset",
           "hat-color",
           "hat-custom-color",
           "headphones-title",
           "headphones",
           "headphones-size",
           "headphones-color",
           "headphones-custom-color",
           "headphones-light",
           "headphones-beat",
           "glasses-title",
           "glasses",
           "glasses-x",
           "glasses-y",
           "glasses-size",
           "glasses-color",
           "glasses-tint",
           "glasses-tint-opacity",
           "earrings-title",
           "earrings",
           "earring-size",
           "earring-length",
           "earring-height",
           "earring-inset-x",
           "earring-color",
           "earring-custom-color",
           "bell-title",
           "bell",
           "bell-size",
           "bell-color",
           "collar-color",
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
           "orb-reaction",
           "orb-kick",
           "trail-title",
           "trail",
           "trail-length",
           "trail-width",
           "trail-color",
           "trail-custom-color",
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

// ! --- Groups --- !

namespace
{
  // First settings of a block that are not its switch
  constexpr std::array<std::string_view, 3> kNotSwitches{"customization", "mode-cube", "reactions"};

  // Fitting a part to the icon and fine detail, behind "More"
  constexpr std::array<std::string_view, 37> kFineTuning{
      "segments", "lock-width", "hair-top-gap", "spin-with-icon", "shine-position", "tips-start",
      "face-lock-inset-x", "face-lock-inset-y", "face-lock-shift-x", "face-lock-tilt",
      "bangs-arc-size", "bangs-arc-softness", "bangs-inset-x", "bangs-inset-y",
      "ponytail-position", "bow-position", "ear-spread", "headband-inset", "flower-position",
      "halo-height", "hat-tilt", "hat-inset", "glasses-x", "glasses-y", "earring-height", "earring-inset-x",
      "blush-spread", "blush-height", "sticker-x", "sticker-y", "orb-kick",
      "wind-gust-speed", "wind-flutter", "hair-friction", "damping", "pet-distance", "clip-side"};

  bool isSwitch(std::string_view key)
  {
    if (key.starts_with('@') || std::find(kNotSwitches.begin(), kNotSwitches.end(), key) != kNotSwitches.end())
      return false;
    auto def = settings::def(key);
    if (!def)
      return false;
    if (def->type == settings::Type::Bool)
      return true;
    if (def->type == settings::Type::Choice)
      return std::find(def->options.begin(), def->options.end(), "None") != def->options.end() ||
             std::find(def->options.begin(), def->options.end(), "Off") != def->options.end();
    if (def->type == settings::Type::Int)
      return def->min == 0.0;
    return false;
  }
}

std::vector<CustomizerGroup> const &customizerGroups()
{
  static std::vector<CustomizerGroup> const groups = []
  {
    std::vector<CustomizerGroup> out;
    auto const &sections = customizerSections();
    for (size_t s = 0; s < sections.size(); ++s)
    {
      CustomizerGroup group;
      group.id = fmt::format("{}-top", sections[s].name);
      group.name = sections[s].name;
      group.section = s;
      bool fresh = true; // nothing in the block yet: the next setting may be its switch

      for (char const *key : sections[s].keys)
      {
        auto def = settings::def(key);
        if (def && def->type == settings::Type::Title)
        {
          if (group.master || !group.keys.empty())
            out.push_back(std::move(group));
          group = CustomizerGroup{};
          group.id = key;
          group.name = def->name;
          group.section = s;
          fresh = true;
          continue;
        }
        if (fresh && isSwitch(key))
        {
          group.master = key;
          group.hidesWhenOff = def->type != settings::Type::Int;
        }
        else
          group.keys.push_back(key);
        fresh = false;
      }
      if (group.master || !group.keys.empty())
        out.push_back(std::move(group));
    }
    return out;
  }();
  return groups;
}

bool groupIsOn(CustomizerGroup const &group)
{
  if (!group.master)
    return true;
  auto def = settings::def(group.master);
  if (!def)
    return true;
  auto const value = settings::get(group.master);
  if (def->type == settings::Type::Bool)
    return value.asBool().unwrapOr(true);
  if (def->type == settings::Type::Choice)
    return value.asString().unwrapOr("") != "None" && value.asString().unwrapOr("") != "Off";
  if (def->type == settings::Type::Int)
    return value.asInt().unwrapOr(0) != 0;
  return true;
}

bool isFineTuning(std::string_view key)
{
  return std::find(kFineTuning.begin(), kFineTuning.end(), key) != kFineTuning.end();
}

// ! --- Look settings --- !

std::vector<std::string_view> const &lookSettingKeys()
{
  static std::vector<std::string_view> const keys = []
  {
    // Turning the mod on and off, also per game mode, isn't part of a look. Whether the hair is on is
    // (an empty look has none)
    static constexpr std::array kNotLook{
        std::string_view("customization"), std::string_view("show-in-garage"), std::string_view("show-in-menus"), std::string_view("quality"),
        std::string_view("focus-mode"), std::string_view("focus-opacity"), std::string_view("focus-hair"), std::string_view("mode-cube"),
        std::string_view("mode-ship"), std::string_view("mode-ball"), std::string_view("mode-ufo"),
        std::string_view("mode-wave"), std::string_view("mode-robot"), std::string_view("mode-spider"),
        std::string_view("mode-swing"), std::string_view("mode-jetpack"), std::string_view("share-icons"), std::string_view("show-icons")};

    std::vector<std::string_view> list;
    for (auto const &section : customizerSections())
    {
      for (std::string_view key : section.keys)
      {
        if (std::find(kNotLook.begin(), kNotLook.end(), key) != kNotLook.end())
          continue;
        // Special rows like "@looks" have no setting
        auto def = settings::def(key);
        if (!def || def->type == settings::Type::Title)
          continue;
        list.push_back(key);
      }
    }
    return list;
  }();
  return keys;
}
