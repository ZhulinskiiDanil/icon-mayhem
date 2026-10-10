#pragma once

#include <Geode/Geode.hpp>

#include <functional>
#include <optional>

// ! --- Level query --- !
// Gameplay objects of the level around a point, for the pet that runs on it. Reads the sections GD
// keeps for collisions (gameplay objects only, no decoration). Coordinates are those of the object layer, the sim space of the rigs in a level.
// Outside a level nothing is ever near.

namespace level
{
  // Every enabled, touchable gameplay object whose rect overlaps `area`
  void forEachObjectNear(cocos2d::CCRect const &area, std::function<void(GameObject *)> const &fn);

  // The closest spike or saw ahead of `from` along `ahead` (unit), up to `range` units away and
  // within `reach` units to the sides. Gives its center
  std::optional<cocos2d::CCPoint> nearestHazard(cocos2d::CCPoint const &from, cocos2d::CCPoint const &ahead, float range,
                                                float reach);

  // The top of the highest solid block under `x` (± halfWidth) that is below `fromY`, along
  // `down` (0, -1) or flipped (0, 1) for reversed gravity
  std::optional<float> groundBelow(float x, float fromY, float halfWidth, float depth, bool flipped);
}
