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
  cfg.hairTopGap = static_cast<float>(mod->getSettingValue<double>("hair-top-gap"));
  cfg.volume = static_cast<float>(mod->getSettingValue<double>("volume"));

  auto number = [&](std::string_view key)
  { return static_cast<float>(mod->getSettingValue<double>(key)); };

  cfg.faceLocks = mod->getSettingValue<bool>("face-locks");
  cfg.faceLockLeft = mod->getSettingValue<bool>("face-lock-left");
  cfg.faceLockRight = mod->getSettingValue<bool>("face-lock-right");
  cfg.faceLockLength = number("face-lock-length");
  cfg.faceLockWidth = number("face-lock-width");
  cfg.braidFaceLocks = mod->getSettingValue<bool>("braid-face-locks");
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

  cfg.tails = parseTails(mod->getSettingValue<std::string>("ponytail"));
  cfg.tailPosition = number("ponytail-position");
  cfg.tailLength = number("ponytail-length");
  cfg.tailThickness = number("ponytail-thickness");
  cfg.tailColorSource = parseColorSource(mod->getSettingValue<std::string>("ponytail-color"));
  cfg.tailColor = mod->getSettingValue<ccColor3B>("ponytail-custom-color");
  cfg.braidTails = mod->getSettingValue<bool>("braid-tails");
  cfg.tie = parseTie(mod->getSettingValue<std::string>("ponytail-tie"));
  cfg.tieColorSource = parseColorSource(mod->getSettingValue<std::string>("tie-color"));
  cfg.tieColor = mod->getSettingValue<ccColor3B>("tie-custom-color");

  cfg.ahoge = static_cast<int>(mod->getSettingValue<int64_t>("ahoge"));
  cfg.ahogeLength = number("ahoge-length");
  cfg.ahogeCurl = number("ahoge-curl");

  cfg.headBow = mod->getSettingValue<bool>("head-bow");
  cfg.bowPosition = number("bow-position");
  cfg.bowSize = number("bow-size");
  cfg.bowRibbonLength = number("bow-ribbon-length");
  cfg.bowColorSource = parseColorSource(mod->getSettingValue<std::string>("bow-color"));
  cfg.bowColor = mod->getSettingValue<ccColor3B>("bow-custom-color");

  auto text = [&](std::string_view key)
  { return mod->getSettingValue<std::string>(key); };
  auto color = [&](std::string_view key)
  { return mod->getSettingValue<ccColor3B>(key); };
  auto flag = [&](std::string_view key)
  { return mod->getSettingValue<bool>(key); };

  cfg.clipCount = static_cast<int>(mod->getSettingValue<int64_t>("clip-count"));
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

  cfg.hitboxMultiplier = static_cast<float>(mod->getSettingValue<double>("hitbox-multiplier"));
  cfg.windMultiplier = static_cast<float>(mod->getSettingValue<double>("wind-multiplier"));
  cfg.calmJumps = mod->getSettingValue<bool>("calm-jumps");
  cfg.gusts = static_cast<float>(mod->getSettingValue<double>("wind-gusts"));
  cfg.gustSpeed = static_cast<float>(mod->getSettingValue<double>("wind-gust-speed"));
  cfg.flutter = static_cast<float>(mod->getSettingValue<double>("wind-flutter"));
  cfg.breeze = static_cast<float>(mod->getSettingValue<double>("breeze"));
  cfg.gravity = static_cast<float>(mod->getSettingValue<double>("gravity"));
  cfg.damping = static_cast<float>(mod->getSettingValue<double>("damping"));
  cfg.friction = static_cast<float>(mod->getSettingValue<double>("hair-friction"));
  cfg.colorSource = parseColorSource(mod->getSettingValue<std::string>("color-source"));
  cfg.customColor = mod->getSettingValue<ccColor3B>("custom-color");
  cfg.outline = mod->getSettingValue<bool>("outline");

  cfg.lockCount = std::clamp(cfg.lockCount, 1, 256);
  cfg.segments = std::clamp(cfg.segments, 3, 16);
  cfg.bangsCount = std::clamp(cfg.bangsCount, 1, 32);
  cfg.ahoge = std::clamp(cfg.ahoge, 0, 2);
  cfg.clipCount = std::clamp(cfg.clipCount, 0, 3);

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
