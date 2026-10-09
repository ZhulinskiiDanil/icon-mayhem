#include "LevelQuery.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

// ! --- Level query --- !

namespace
{
  constexpr float kDefaultSection = 100.f; // units per section when the factors look unusual

  // Units per section from GD's factor: either a multiplier (0.01) or a size (100)
  float sectionSize(float factor)
  {
    if (factor > 1.f)
      return factor;
    if (factor > 0.f)
      return 1.f / factor;
    return kDefaultSection;
  }

  bool isHazard(GameObjectType type)
  {
    return type == GameObjectType::Hazard || type == GameObjectType::AnimatedHazard;
  }

  bool isBooster(GameObjectType type)
  {
    switch (type)
    {
    case GameObjectType::YellowJumpPad:
    case GameObjectType::PinkJumpPad:
    case GameObjectType::GravityPad:
    case GameObjectType::RedJumpPad:
    case GameObjectType::SpiderPad:
    case GameObjectType::YellowJumpRing:
    case GameObjectType::PinkJumpRing:
    case GameObjectType::GravityRing:
    case GameObjectType::RedJumpRing:
    case GameObjectType::GreenRing:
    case GameObjectType::DashRing:
    case GameObjectType::GravityDashRing:
    case GameObjectType::DropRing:
    case GameObjectType::CustomRing:
    case GameObjectType::SpiderOrb:
    case GameObjectType::TeleportOrb:
      return true;
    default:
      return false;
    }
  }

  // The closest object of a kind ahead, measured along `ahead`
  std::optional<CCPoint> nearestAhead(CCPoint const &from, CCPoint const &ahead, float range, float reach,
                                      bool (*matches)(GameObjectType))
  {
    CCPoint const side = {-ahead.y, ahead.x};
    CCPoint const end = from + ahead * range;
    CCRect const area{std::min(from.x, end.x) - reach, std::min(from.y, end.y) - reach, std::abs(end.x - from.x) + 2.f * reach,
                      std::abs(end.y - from.y) + 2.f * reach};

    std::optional<CCPoint> best;
    float bestDistance = range;
    level::forEachObjectNear(area, [&](GameObject *object)
                             {
                               if (!matches(object->m_objectType))
                                 return;
                               CCPoint const center = object->getPosition();
                               CCPoint const offset = center - from;
                               float const along = offset.dot(ahead);
                               if (along < 0.f || along > bestDistance || std::abs(offset.dot(side)) > reach)
                                 return;
                               bestDistance = along;
                               best = center;
                             });
    return best;
  }
}

void level::forEachObjectNear(CCRect const &area, std::function<void(GameObject *)> const &fn)
{
  auto layer = PlayLayer::get();
  if (!layer)
    return;

  auto &sections = layer->m_nonEffectObjects;
  auto &sizes = layer->m_nonEffectObjectsSizes;
  float const sizeX = sectionSize(layer->m_sectionXFactor);
  float const sizeY = sectionSize(layer->m_sectionYFactor);

  int const fromX = std::max(0, static_cast<int>(std::floor(area.getMinX() / sizeX)));
  int const toX = std::min(static_cast<int>(sections.size()) - 1, static_cast<int>(std::floor(area.getMaxX() / sizeX)));
  for (int sx = fromX; sx <= toX; ++sx)
  {
    auto column = sections[sx];
    auto columnSizes = sx < static_cast<int>(sizes.size()) ? sizes[sx] : nullptr;
    if (!column || !columnSizes)
      continue;

    int const fromY = std::max(0, static_cast<int>(std::floor(area.getMinY() / sizeY)));
    int const toY = std::min({static_cast<int>(column->size()) - 1, static_cast<int>(columnSizes->size()) - 1,
                              static_cast<int>(std::floor(area.getMaxY() / sizeY))});
    for (int sy = fromY; sy <= toY; ++sy)
    {
      auto objects = (*column)[sy];
      if (!objects)
        continue;
      int const count = std::min(static_cast<int>(objects->size()), (*columnSizes)[sy]);
      for (int i = 0; i < count; ++i)
      {
        auto object = (*objects)[i];
        if (!object || object->m_isDisabled || object->m_isDisabled2 || object->m_isHide || object->m_isNoTouch)
          continue;
        if (!object->getObjectRect().intersectsRect(area))
          continue;
        fn(object);
      }
    }
  }
}

std::optional<CCPoint> level::nearestHazard(CCPoint const &from, CCPoint const &ahead, float range, float reach)
{
  return nearestAhead(from, ahead, range, reach, isHazard);
}

std::optional<CCPoint> level::nearestBooster(CCPoint const &from, CCPoint const &ahead, float range, float reach)
{
  return nearestAhead(from, ahead, range, reach, isBooster);
}

std::optional<float> level::groundBelow(float x, float fromY, float halfWidth, float depth, bool flipped)
{
  // Normal gravity: the highest top below fromY; flipped: the lowest bottom above it
  CCRect const area = flipped ? CCRect{x - halfWidth, fromY, 2.f * halfWidth, depth}
                              : CCRect{x - halfWidth, fromY - depth, 2.f * halfWidth, depth};
  std::optional<float> best;
  forEachObjectNear(area, [&](GameObject *object)
                    {
                      if (object->m_objectType != GameObjectType::Solid && object->m_objectType != GameObjectType::Slope)
                        return;
                      auto const &rect = object->getObjectRect();
                      if (flipped)
                      {
                        float const bottom = rect.getMinY();
                        if (bottom >= fromY - 2.f && (!best || bottom < *best))
                          best = bottom;
                      }
                      else
                      {
                        float const top = rect.getMaxY();
                        if (top <= fromY + 2.f && (!best || top > *best))
                          best = top;
                      }
                    });
  return best;
}
