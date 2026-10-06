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

  TailStyle parseTails(std::string const &value)
  {
    if (value == "Ponytail")
      return TailStyle::Ponytail;
    if (value == "Twin tails")
      return TailStyle::TwinTails;
    return TailStyle::None;
  }

  TieStyle parseTie(std::string const &value)
  {
    if (value == "Bow")
      return TieStyle::Bow;
    if (value == "None")
      return TieStyle::None;
    return TieStyle::Scrunchie;
  }

  ClipStyle parseClip(std::string const &value)
  {
    if (value == "Heart")
      return ClipStyle::Heart;
    if (value == "X pin")
      return ClipStyle::XPin;
    if (value == "Bar")
      return ClipStyle::Bar;
    return ClipStyle::Star;
  }

  EarStyle parseEars(std::string const &value)
  {
    if (value == "Cat")
      return EarStyle::Cat;
    if (value == "Bunny")
      return EarStyle::Bunny;
    if (value == "Fox")
      return EarStyle::Fox;
    return EarStyle::None;
  }

  SparkleStyle parseSparkles(std::string const &value)
  {
    if (value == "Hearts")
      return SparkleStyle::Hearts;
    if (value == "Sparkles")
      return SparkleStyle::Sparkles;
    if (value == "Both")
      return SparkleStyle::Both;
    return SparkleStyle::None;
  }

  HeadbandDeco parseHeadbandDeco(std::string const &value)
  {
    if (value == "Bow")
      return HeadbandDeco::Bow;
    if (value == "Cat ears")
      return HeadbandDeco::CatEars;
    if (value == "Bunny ears")
      return HeadbandDeco::BunnyEars;
    return HeadbandDeco::None;
  }

  FlowerStyle parseFlowers(std::string const &value)
  {
    if (value == "Sakura")
      return FlowerStyle::Sakura;
    if (value == "Daisy")
      return FlowerStyle::Daisy;
    if (value == "Crown")
      return FlowerStyle::Crown;
    return FlowerStyle::None;
  }

  WingStyle parseWings(std::string const &value)
  {
    if (value == "Angel")
      return WingStyle::Angel;
    if (value == "Fairy")
      return WingStyle::Fairy;
    if (value == "Bat")
      return WingStyle::Bat;
    return WingStyle::None;
  }

  PetStyle parsePet(std::string const &value)
  {
    if (value == "Cat")
      return PetStyle::Cat;
    if (value == "Ghost")
      return PetStyle::Ghost;
    if (value == "Bird")
      return PetStyle::Bird;
    if (value == "Bunny")
      return PetStyle::Bunny;
    if (value == "Slime")
      return PetStyle::Slime;
    return PetStyle::None;
  }

  HatStyle parseHat(std::string const &value)
  {
    if (value == "Beret")
      return HatStyle::Beret;
    if (value == "Beanie")
      return HatStyle::Beanie;
    if (value == "Witch hat")
      return HatStyle::WitchHat;
    return HatStyle::None;
  }

  StickerStyle parseSticker(std::string const &value)
  {
    if (value == "Band-aid")
      return StickerStyle::BandAid;
    if (value == "Heart")
      return StickerStyle::Heart;
    if (value == "Star")
      return StickerStyle::Star;
    if (value == "Freckles")
      return StickerStyle::Freckles;
    return StickerStyle::None;
  }

  WeatherStyle parseWeather(std::string const &value)
  {
    if (value == "Snow")
      return WeatherStyle::Snow;
    if (value == "Leaves")
      return WeatherStyle::Leaves;
    if (value == "Stars")
      return WeatherStyle::Stars;
    return WeatherStyle::Sakura;
  }

  HeadphoneStyle parseHeadphones(std::string const &value)
  {
    if (value == "Plain")
      return HeadphoneStyle::Plain;
    if (value == "Cat ears")
      return HeadphoneStyle::CatEars;
    return HeadphoneStyle::None;
  }

  GlassesStyle parseGlasses(std::string const &value)
  {
    if (value == "Round")
      return GlassesStyle::Round;
    if (value == "Hearts")
      return GlassesStyle::Hearts;
    if (value == "Stars")
      return GlassesStyle::Stars;
    return GlassesStyle::None;
  }

  EarringStyle parseEarrings(std::string const &value)
  {
    if (value == "Drops")
      return EarringStyle::Drops;
    if (value == "Hearts")
      return EarringStyle::Hearts;
    if (value == "Stars")
      return EarringStyle::Stars;
    if (value == "Pearls")
      return EarringStyle::Pearls;
    return EarringStyle::None;
  }

  StreakPlacement parseStreaks(std::string const &value)
  {
    if (value == "Face locks")
      return StreakPlacement::FaceLocks;
    if (value == "Front")
      return StreakPlacement::Front;
    if (value == "Back")
      return StreakPlacement::Back;
    if (value == "Scattered")
      return StreakPlacement::Scattered;
    return StreakPlacement::Bangs;
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

HairConfig HairConfig::load(matjson::Value const *look)
{
  auto mod = Mod::get();
  HairConfig cfg;

  // A look from a preset: its values win, settings it doesn't have come from the mod settings
  auto lookValue = [&](std::string_view key) -> matjson::Value const *
  {
    if (!look)
      return nullptr;
    auto value = look->get(key);
    return value ? &value.unwrap() : nullptr;
  };
  auto number = [&](std::string_view key)
  {
    if (auto value = lookValue(key); value && value->isNumber())
      return static_cast<float>(value->asDouble().unwrapOr(0.0));
    return static_cast<float>(mod->getSettingValue<double>(key));
  };
  auto integer = [&](std::string_view key)
  {
    if (auto value = lookValue(key); value && value->isNumber())
      return static_cast<int>(value->asInt().unwrapOr(0));
    return static_cast<int>(mod->getSettingValue<int64_t>(key));
  };
  auto flag = [&](std::string_view key)
  {
    if (auto value = lookValue(key); value && value->isBool())
      return value->asBool().unwrapOr(false);
    return mod->getSettingValue<bool>(key);
  };
  auto text = [&](std::string_view key)
  {
    if (auto value = lookValue(key); value && value->isString())
      return value->asString().unwrapOr("");
    return mod->getSettingValue<std::string>(key);
  };
  auto color = [&](std::string_view key)
  {
    if (auto value = lookValue(key); value && value->isString())
    {
      if (auto parsed = cc3bFromHexString(value->asString().unwrapOr(""), true))
        return parsed.unwrap();
    }
    return mod->getSettingValue<ccColor3B>(key);
  };

  // The switches of the mod itself always come from the mod settings

  cfg.enabled = mod->getSettingValue<bool>("enabled");
  cfg.showInGarage = mod->getSettingValue<bool>("show-in-garage");

  static constexpr std::array<char const *, static_cast<size_t>(GameMode::Count)> kModeKeys{
      "mode-cube", "mode-ship", "mode-ball", "mode-ufo", "mode-wave",
      "mode-robot", "mode-spider", "mode-swing", "mode-jetpack"};
  for (size_t i = 0; i < kModeKeys.size(); ++i)
    cfg.modes[i] = mod->getSettingValue<bool>(kModeKeys[i]);

  cfg.style = parseStyle(text("style"));
  cfg.spinWithIcon = flag("spin-with-icon");
  cfg.lockCount = integer("density");
  cfg.length = number("hair-length");
  cfg.segments = integer("segments");
  cfg.lockWidth = number("lock-width");
  cfg.hairTopGap = number("hair-top-gap");
  cfg.volume = number("volume");


  cfg.faceLocks = flag("face-locks");
  cfg.faceLockLeft = flag("face-lock-left");
  cfg.faceLockRight = flag("face-lock-right");
  cfg.faceLockLength = number("face-lock-length");
  cfg.faceLockWidth = number("face-lock-width");
  cfg.braidFaceLocks = flag("braid-face-locks");
  cfg.faceLockInsetX = number("face-lock-inset-x");
  cfg.faceLockInsetY = number("face-lock-inset-y");
  cfg.faceLockColorSource = parseColorSource(text("face-lock-color"));
  cfg.faceLockColor = color("face-lock-custom-color");

  cfg.bangs = flag("bangs");
  cfg.bangsLength = number("bangs-length");
  cfg.bangsCount = integer("bangs-density");
  cfg.bangsSpread = number("bangs-spread");
  cfg.bangsArcSize = number("bangs-arc-size");
  cfg.bangsArcSoftness = number("bangs-arc-softness");
  cfg.bangsInsetX = number("bangs-inset-x");
  cfg.bangsInsetY = number("bangs-inset-y");
  cfg.bangsColorSource = parseColorSource(text("bangs-color"));
  cfg.bangsColor = color("bangs-custom-color");

  cfg.tails = parseTails(text("ponytail"));
  cfg.tailPosition = number("ponytail-position");
  cfg.tailLength = number("ponytail-length");
  cfg.tailThickness = number("ponytail-thickness");
  cfg.tailColorSource = parseColorSource(text("ponytail-color"));
  cfg.tailColor = color("ponytail-custom-color");
  cfg.braidTails = flag("braid-tails");
  cfg.tie = parseTie(text("ponytail-tie"));
  cfg.tieColorSource = parseColorSource(text("tie-color"));
  cfg.tieColor = color("tie-custom-color");

  cfg.ahoge = integer("ahoge");
  cfg.ahogeLength = number("ahoge-length");
  cfg.ahogeCurl = number("ahoge-curl");

  cfg.headBow = flag("head-bow");
  cfg.bowPosition = number("bow-position");
  cfg.bowSize = number("bow-size");
  cfg.bowRibbonLength = number("bow-ribbon-length");
  cfg.bowColorSource = parseColorSource(text("bow-color"));
  cfg.bowColor = color("bow-custom-color");


  cfg.clipCount = integer("clip-count");
  cfg.clipStyle = parseClip(text("clip-style"));
  cfg.clipRight = text("clip-side") != "Left";
  cfg.clipSize = number("clip-size");
  cfg.clipColorSource = parseColorSource(text("clip-color"));
  cfg.clipColor = color("clip-custom-color");

  cfg.scarf = flag("scarf");
  cfg.scarfHeight = number("scarf-height");
  cfg.scarfWidth = number("scarf-width");
  cfg.scarfLength = number("scarf-length");
  cfg.scarfColorSource = parseColorSource(text("scarf-color"));
  cfg.scarfColor = color("scarf-custom-color");

  cfg.ears = parseEars(text("ears"));
  cfg.earSize = number("ear-size");
  cfg.earSpread = number("ear-spread");
  cfg.earTwitch = number("ear-twitch");
  cfg.earColorSource = parseColorSource(text("ear-color"));
  cfg.earColor = color("ear-custom-color");
  cfg.earInnerColor = color("ear-inner-color");

  cfg.blush = flag("blush");
  cfg.blushColor = color("blush-color");
  cfg.blushOpacity = number("blush-opacity");
  cfg.blushSize = number("blush-size");
  cfg.blushSpread = number("blush-spread");
  cfg.blushHeight = number("blush-height");
  cfg.blushLines = flag("blush-lines");
  cfg.blushPop = flag("blush-pop");

  cfg.sparkles = parseSparkles(text("sparkles"));
  cfg.sparkleRate = number("sparkle-rate");
  cfg.sparkleOnLanding = flag("sparkle-on-landing");
  cfg.sparkleSize = number("sparkle-size");
  cfg.sparkleColorSource = parseColorSource(text("sparkle-color"));
  cfg.sparkleColor = color("sparkle-custom-color");

  cfg.headband = flag("headband");
  cfg.headbandInset = number("headband-inset");
  cfg.headbandWidth = number("headband-width");
  cfg.headbandColorSource = parseColorSource(text("headband-color"));
  cfg.headbandColor = color("headband-custom-color");
  cfg.headbandDeco = parseHeadbandDeco(text("headband-deco"));

  cfg.flowers = parseFlowers(text("flowers"));
  cfg.flowerPosition = number("flower-position");
  cfg.flowerSize = number("flower-size");
  cfg.flowerColorSource = parseColorSource(text("flower-color"));
  cfg.flowerColor = color("flower-custom-color");

  cfg.halo = flag("halo");
  cfg.haloHeight = number("halo-height");
  cfg.haloSize = number("halo-size");
  cfg.haloColor = color("halo-color");
  cfg.haloGlow = flag("halo-glow");

  cfg.petals = flag("petals");
  cfg.petalAmount = number("petal-amount");
  cfg.petalColor = color("petal-color");
  cfg.sleepy = flag("sleepy");
  cfg.sleepyDelay = number("sleepy-delay");

  cfg.wings = parseWings(text("wings"));
  cfg.wingSize = number("wing-size");
  cfg.wingFlap = number("wing-flap");
  cfg.wingColorSource = parseColorSource(text("wing-color"));
  cfg.wingColor = color("wing-custom-color");

  cfg.pet = parsePet(text("pet"));
  cfg.petSize = number("pet-size");
  cfg.petDistance = number("pet-distance");
  cfg.petColorSource = parseColorSource(text("pet-color"));
  cfg.petColor = color("pet-custom-color");
  cfg.petMoods = flag("pet-moods");

  cfg.headphones = parseHeadphones(text("headphones"));
  cfg.headphonesSize = number("headphones-size");
  cfg.headphonesColorSource = parseColorSource(text("headphones-color"));
  cfg.headphonesColor = color("headphones-custom-color");
  cfg.headphonesLight = color("headphones-light");
  cfg.headphonesBeat = flag("headphones-beat");

  cfg.glasses = parseGlasses(text("glasses"));
  cfg.glassesX = number("glasses-x");
  cfg.glassesY = number("glasses-y");
  cfg.glassesSize = number("glasses-size");
  cfg.glassesColor = color("glasses-color");
  cfg.glassesTint = color("glasses-tint");
  cfg.glassesTintOpacity = number("glasses-tint-opacity");

  cfg.earrings = parseEarrings(text("earrings"));
  cfg.earringSize = number("earring-size");
  cfg.earringLength = number("earring-length");
  cfg.earringHeight = number("earring-height");
  cfg.earringColorSource = parseColorSource(text("earring-color"));
  cfg.earringColor = color("earring-custom-color");
  cfg.bell = flag("bell");
  cfg.bellSize = number("bell-size");
  cfg.bellColor = color("bell-color");
  cfg.collarColor = color("collar-color");

  cfg.streaks = integer("streaks");
  cfg.streakPlacement = parseStreaks(text("streak-placement"));
  cfg.streakColor = color("streak-color");

  cfg.orbReaction = flag("orb-reaction");
  cfg.orbKick = number("orb-kick");

  cfg.hat = parseHat(text("hat"));
  cfg.hatSize = number("hat-size");
  cfg.hatTilt = number("hat-tilt");
  cfg.hatInset = number("hat-inset");
  cfg.hatColorSource = parseColorSource(text("hat-color"));
  cfg.hatColor = color("hat-custom-color");

  cfg.sticker = parseSticker(text("sticker"));
  cfg.stickerRight = text("sticker-side") != "Left";
  cfg.stickerX = number("sticker-x");
  cfg.stickerY = number("sticker-y");
  cfg.stickerSize = number("sticker-size");
  cfg.stickerColor = color("sticker-color");

  cfg.reactions = flag("reactions");
  cfg.cuteDeath = flag("cute-death");
  cfg.weather = parseWeather(text("petal-style"));

  cfg.hairShine = flag("hair-shine");
  cfg.shinePosition = number("shine-position");
  cfg.shineStrength = number("shine-strength");
  cfg.dyedTips = flag("dyed-tips");
  cfg.tipsColorSource = parseColorSource(text("tips-color"));
  cfg.tipsColor = color("tips-custom-color");
  cfg.tipsStart = number("tips-start");

  cfg.hitboxMultiplier = number("hitbox-multiplier");
  cfg.windMultiplier = number("wind-multiplier");
  cfg.calmJumps = flag("calm-jumps");
  cfg.gusts = number("wind-gusts");
  cfg.gustSpeed = number("wind-gust-speed");
  cfg.flutter = number("wind-flutter");
  cfg.breeze = number("breeze");
  cfg.gravity = number("gravity");
  cfg.damping = number("damping");
  cfg.friction = number("hair-friction");
  cfg.colorSource = parseColorSource(text("color-source"));
  cfg.customColor = color("custom-color");
  cfg.outline = flag("outline");

  cfg.lockCount = std::clamp(cfg.lockCount, 1, 256);
  cfg.segments = std::clamp(cfg.segments, 3, 16);
  cfg.bangsCount = std::clamp(cfg.bangsCount, 1, 32);
  cfg.ahoge = std::clamp(cfg.ahoge, 0, 2);
  cfg.clipCount = std::clamp(cfg.clipCount, 0, 3);
  cfg.streaks = std::clamp(cfg.streaks, 0, 8);

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
