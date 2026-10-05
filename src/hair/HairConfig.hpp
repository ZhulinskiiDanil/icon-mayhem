#pragma once

#include <Geode/Geode.hpp>

// ! --- Hair config --- !

enum class HairColorSource
{
  Hair, // face locks and bangs: same as the rest of the hair
  Primary,
  Secondary,
  Custom,
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
  HairStyle style = HairStyle::Flowing;
  bool spinWithIcon = true;
  int lockCount = 24;
  float length = 20.f; // icon units, the cube is ~30
  int segments = 8;
  float lockWidth = 3.f; // icon units, at the root
  float volume = .7f;

  // Hair in front of the face, drawn over the icon. Insets are in icon units
  bool faceLocks = false; // locks framing the face
  bool faceLockLeft = true;
  bool faceLockRight = true;
  float faceLockLength = 24.f;
  float faceLockWidth = 3.6f; // at the root
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

  float hitboxMultiplier = 1.f; // size of the head collider
  float windMultiplier = 1.f;  // 0 makes the hair look the same moving and standing still
  float gravity = 1.f;
  float damping = .06f;
  HairColorSource colorSource = HairColorSource::Secondary;
  cocos2d::ccColor3B customColor = {58, 42, 128};
  bool outline = true;

  static HairConfig load();

  // Bumped whenever any mod setting changes, nodes compare it to reload lazily
  static unsigned version();
  static void bumpVersion();
};
