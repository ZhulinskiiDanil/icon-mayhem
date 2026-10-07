#pragma once

#include <Geode/Geode.hpp>

#include <array>

// ! --- Hair config --- !

enum class HairColorSource
{
  Hair, // face locks and bangs: same as the rest of the hair
  Primary,
  Secondary,
  Custom,
};

enum class TailStyle
{
  None,
  Ponytail,
  TwinTails,
};

enum class TieStyle
{
  Scrunchie,
  Bow,
  None,
};

enum class ClipStyle
{
  Star,
  Heart,
  XPin,
  Bar,
};

enum class EarStyle
{
  None,
  Cat,
  Bunny,
  Fox,
};

enum class SparkleStyle
{
  None,
  Hearts,
  Sparkles,
  Both,
};

enum class HeadbandDeco
{
  None,
  Bow,
  CatEars,
  BunnyEars,
};

enum class FlowerStyle
{
  None,
  Sakura,
  Daisy,
  Crown,
};

enum class WingStyle
{
  None,
  Angel,
  Fairy,
  Bat,
};

enum class PetStyle
{
  None,
  Cat,
  Ghost,
  Bird,
  Bunny,
  Slime,
};

enum class HatStyle
{
  None,
  Beret,
  Beanie,
  WitchHat,
};

enum class StickerStyle
{
  None,
  BandAid,
  Heart,
  Star,
  Freckles,
};

enum class WeatherStyle
{
  Sakura,
  Snow,
  Leaves,
  Stars,
};

enum class HeadphoneStyle
{
  None,
  Plain,
  CatEars,
};

enum class GlassesStyle
{
  None,
  Round,
  Hearts,
  Stars,
};

enum class EarringStyle
{
  None,
  Drops,
  Hearts,
  Stars,
  Pearls,
};

enum class StreakPlacement
{
  Bangs,
  FaceLocks,
  Front,
  Back,
  Scattered,
};

enum class TrailStyle
{
  None,
  Ribbon,
  Hearts,
  Stars,
  Sparkles,
};

enum class Quality
{
  High,
  Balanced,
  Low,
};

enum class GameMode
{
  Cube,
  Ship,
  Ball,
  Ufo,
  Wave,
  Robot,
  Spider,
  Swing,
  Jetpack,
  Count,
};

enum class HairStyle
{
  Flowing,
  Long,
  Spiky,
};

struct HairConfig
{
  bool enabled = true;
  bool showInGarage = true;
  bool showInMenus = true; // your icons on your profile and the menu
  // Performance mode: caps applied after loading, so a look keeps its own values
  Quality quality = Quality::High;
  size_t maxParticles = 90;
  float simRate = 240.f; // simulation steps per second
  std::array<bool, static_cast<size_t>(GameMode::Count)> modes; // the whole rig is hidden in the modes turned off
  HairStyle style = HairStyle::Flowing;
  bool spinWithIcon = true;
  int lockCount = 24;
  float length = 20.f; // icon units, the cube is ~30
  int segments = 8;
  float lockWidth = 3.f; // icon units, at the root
  float hairTopGap = 0.f; // degrees around the top of the head without hair, for a cap
  float volume = .7f;

  // Hair in front of the face, drawn over the icon. Insets are in icon units
  bool faceLocks = false; // locks framing the face
  bool faceLockLeft = true;
  bool faceLockRight = true;
  float faceLockLength = 24.f;
  float faceLockWidth = 3.6f; // at the root
  bool braidFaceLocks = false;
  float faceLockInsetX = 0.f; // towards the middle of the face
  float faceLockInsetY = 0.f; // down the face
  HairColorSource faceLockColorSource = HairColorSource::Hair;
  cocos2d::ccColor3B faceLockColor = {58, 42, 128};

  bool bangs = false;
  float bangsLength = 9.f;
  int bangsCount = 9;
  float bangsSpread = .8f;     // how far to the sides from the middle, 1 is the whole face
  float bangsArcSize = 0.f;    // how much the ends drop down, for icons with a round top
  float bangsArcSoftness = .5f; // 0 rounds only the ends, 1 makes one smooth arc
  float bangsInsetX = 0.f;     // to the right
  float bangsInsetY = 0.f;     // down the face
  HairColorSource bangsColorSource = HairColorSource::Hair;
  cocos2d::ccColor3B bangsColor = {58, 42, 128};

  // Extras, see Extras.cpp. Positions are degrees from the top of the head towards the back
  TailStyle tails = TailStyle::None;
  float tailPosition = 95.f;
  float tailLength = 30.f;
  float tailThickness = 6.f;
  HairColorSource tailColorSource = HairColorSource::Hair;
  cocos2d::ccColor3B tailColor = {58, 42, 128};
  bool braidTails = false;
  TieStyle tie = TieStyle::Scrunchie;
  HairColorSource tieColorSource = HairColorSource::Custom;
  cocos2d::ccColor3B tieColor = {224, 64, 90};

  int ahoge = 0; // strands
  float ahogeLength = 12.f;
  float ahogeCurl = .6f;

  bool headBow = false;
  float bowPosition = -35.f;
  float bowSize = 1.f;
  float bowRibbonLength = 10.f;
  HairColorSource bowColorSource = HairColorSource::Custom;
  cocos2d::ccColor3B bowColor = {224, 64, 90};

  // Hair clips on the bangs (or the face locks, or the head)
  int clipCount = 0;
  ClipStyle clipStyle = ClipStyle::Star;
  bool clipRight = true;
  float clipSize = 1.f;
  HairColorSource clipColorSource = HairColorSource::Custom;
  cocos2d::ccColor3B clipColor = {255, 216, 74};

  // Scarf around the bottom of the head, ends fluttering behind
  bool scarf = false;
  float scarfHeight = 3.f; // icon units up from the bottom edge
  float scarfWidth = 6.f;
  float scarfLength = 16.f;
  HairColorSource scarfColorSource = HairColorSource::Custom;
  cocos2d::ccColor3B scarfColor = {217, 48, 62};

  // Ears on top of the head
  EarStyle ears = EarStyle::None;
  float earSize = 1.f;
  float earSpread = 38.f; // degrees from the top
  float earTwitch = .5f;  // how often they twitch
  HairColorSource earColorSource = HairColorSource::Hair;
  cocos2d::ccColor3B earColor = {58, 42, 128};
  cocos2d::ccColor3B earInnerColor = {244, 167, 185};

  // Effects
  bool blush = false;
  cocos2d::ccColor3B blushColor = {255, 143, 163};
  float blushOpacity = .45f;
  float blushSize = 1.f;
  float blushSpread = 9.f;  // icon units from the middle of the face
  float blushHeight = -5.f; // icon units from the middle of the face
  bool blushLines = true;
  bool blushPop = true; // brighter for a moment after a landing

  SparkleStyle sparkles = SparkleStyle::None;
  float sparkleRate = .4f;
  bool sparkleOnLanding = true;
  float sparkleSize = 1.f;
  HairColorSource sparkleColorSource = HairColorSource::Custom;
  cocos2d::ccColor3B sparkleColor = {255, 111, 174};

  // Headband over the top of the head
  bool headband = false;
  float headbandInset = 1.5f; // icon units down from the edge of the head
  float headbandWidth = 2.5f;
  HairColorSource headbandColorSource = HairColorSource::Custom;
  cocos2d::ccColor3B headbandColor = {224, 64, 90};
  HeadbandDeco headbandDeco = HeadbandDeco::None;

  // Flowers in the hair
  FlowerStyle flowers = FlowerStyle::None;
  float flowerPosition = 40.f; // degrees from the top
  float flowerSize = 1.f;
  HairColorSource flowerColorSource = HairColorSource::Custom;
  cocos2d::ccColor3B flowerColor = {255, 183, 197};

  // Halo floating above the head
  bool halo = false;
  float haloHeight = 7.f; // icon units above the head
  float haloSize = 1.f;
  cocos2d::ccColor3B haloColor = {255, 230, 128};
  bool haloGlow = true;

  // Falling sakura petals and the sleepy Zzz
  bool petals = false;
  float petalAmount = .5f;
  cocos2d::ccColor3B petalColor = {255, 192, 203};
  bool sleepy = false;
  float sleepyDelay = 6.f; // s standing still before falling asleep

  // Wings on the sides of the head
  WingStyle wings = WingStyle::None;
  float wingSize = 1.f;
  float wingFlap = .6f;
  HairColorSource wingColorSource = HairColorSource::Custom;
  cocos2d::ccColor3B wingColor = {255, 255, 255};

  // Pet floating behind the icon
  PetStyle pet = PetStyle::None;
  float petSize = 1.f;
  float petDistance = 12.f; // icon units behind the head
  HairColorSource petColorSource = HairColorSource::Custom;
  cocos2d::ccColor3B petColor = {245, 215, 181};
  bool petMoods = true; // sleeps, cheers and is sad with the icon

  // Hat on the hairstyle
  HatStyle hat = HatStyle::None;
  float hatSize = 1.f;
  float hatTilt = 8.f;  // degrees
  float hatInset = 2.f; // icon units down onto the head
  HairColorSource hatColorSource = HairColorSource::Custom;
  cocos2d::ccColor3B hatColor = {217, 48, 62};

  // Face sticker, placed like the blush
  StickerStyle sticker = StickerStyle::None;
  bool stickerRight = true;
  float stickerX = 8.f;
  float stickerY = -6.f;
  float stickerSize = 1.f;
  cocos2d::ccColor3B stickerColor = {255, 111, 145};

  // Headphones over the head, the lights glow to the music
  HeadphoneStyle headphones = HeadphoneStyle::None;
  float headphonesSize = 1.f;
  HairColorSource headphonesColorSource = HairColorSource::Custom;
  cocos2d::ccColor3B headphonesColor = {245, 240, 255};
  cocos2d::ccColor3B headphonesLight = {255, 143, 177};
  bool headphonesBeat = true;

  // Glasses, placed like the blush
  GlassesStyle glasses = GlassesStyle::None;
  float glassesX = 6.5f; // icon units from the middle of the face
  float glassesY = 0.f;
  float glassesSize = 1.f;
  cocos2d::ccColor3B glassesColor = {43, 33, 64};
  cocos2d::ccColor3B glassesTint = {190, 230, 255};
  float glassesTintOpacity = .35f;

  // Earrings and a cat bell swinging on pendulums
  EarringStyle earrings = EarringStyle::None;
  float earringSize = 1.f;
  float earringLength = 2.5f; // icon units of chain
  float earringHeight = -4.f; // icon units from the middle of the head side
  HairColorSource earringColorSource = HairColorSource::Custom;
  cocos2d::ccColor3B earringColor = {255, 210, 74};
  bool bell = false;
  float bellSize = 1.f;
  cocos2d::ccColor3B bellColor = {255, 210, 74};
  cocos2d::ccColor3B collarColor = {217, 48, 62};

  // Colored locks in the hair
  int streaks = 0;
  StreakPlacement streakPlacement = StreakPlacement::Bangs;
  cocos2d::ccColor3B streakColor = {255, 143, 177};

  // A ribbon tied at the back of the head, or little hearts / stars / sparkles left behind
  TrailStyle trail = TrailStyle::None;
  float trailLength = 50.f; // icon units of ribbon; for the particles how long they stay
  float trailWidth = 2.5f;
  HairColorSource trailColorSource = HairColorSource::Custom;
  cocos2d::ccColor3B trailColor = {255, 143, 177};

  bool orbReaction = false; // orbs and pads throw the hair up with sparkles
  float orbKick = 1.f;

  bool reactions = false; // sweat drop on death, hearts on level complete, sparkles on checkpoints
  bool cuteDeath = false; // the hair bursts into petals and hearts on death
  WeatherStyle weather = WeatherStyle::Sakura;

  float hitboxMultiplier = 1.f; // size of the head collider
  float windMultiplier = 1.f;  // 0 makes the hair look the same moving and standing still
  bool calmJumps = true;       // the hair turns with a spinning icon instead of whipping around
  float gusts = .4f;           // how much the wind strength changes over time
  float gustSpeed = 1.f;       // how quickly the gusts come and go
  float flutter = .3f;         // small random flicks of every lock
  float breeze = 0.f;          // wind even standing still, 1 combs the hair like running
  float gravity = 1.f;
  float damping = .06f;
  float friction = .3f; // internal friction of the hair, settles swinging
  HairColorSource colorSource = HairColorSource::Secondary;
  cocos2d::ccColor3B customColor = {58, 42, 128};
  bool outline = true;

  // Hair look
  bool hairShine = false;
  float shinePosition = .25f; // along the locks
  float shineStrength = .5f;
  bool dyedTips = false;
  HairColorSource tipsColorSource = HairColorSource::Custom;
  cocos2d::ccColor3B tipsColor = {232, 123, 168};
  float tipsStart = .6f; // along the locks

  bool showsIn(GameMode mode) const { return modes[static_cast<size_t>(mode)]; }

  // The mod settings, or a look saved in a preset (its settings JSON) on top of them
  static HairConfig load(matjson::Value const *look = nullptr);

  // Bumped whenever any mod setting changes, nodes compare it to reload lazily
  static unsigned version();
  static void bumpVersion();
};
