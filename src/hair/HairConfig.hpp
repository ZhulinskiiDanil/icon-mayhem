#pragma once

#include <Geode/Geode.hpp>

// ! --- Hair config --- !

enum class HairColorSource
{
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
  float hitboxMultiplier = 1.f; // size of the head collider
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
