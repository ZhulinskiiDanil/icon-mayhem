#include "HairNode.hpp"
#include "HairShared.hpp"

#include <array>
#include <cmath>

using namespace geode::prelude;
using namespace hair;

// ! --- Wings --- !
// Angel, fairy or bat wings on the sides of the head, behind the icon. They flap once on every
// takeoff (an angle spring), flutter softly the rest of the time, fold back in the air flow and
// droop when sleepy. Sizes are in icon units.

namespace
{
  constexpr float kWingLength = 13.f; // at wing size 1
  constexpr float kWingRoot = .82f;   // part of the way to the edge of the head where they grow
  constexpr float kWingDrop = .15f;   // roots a bit below the middle, relative to the head half size
  constexpr float kWingSpread = 25.f; // degrees above horizontal at rest
  constexpr float kWingKick = 420.f;  // degrees / s of a flap at wing flap 1
  constexpr float kWingSpring = 220.f; // 1 / s^2
  constexpr float kWingDamping = 9.f;  // 1 / s
  constexpr float kWingFold = 18.f;    // degrees folded down by the full air flow
  constexpr float kWingSleepy = 22.f;  // degrees they droop when sleepy

  struct Flutter
  {
    float speed;  // radians / s
    float amount; // degrees
  };

  Flutter flutterOf(WingStyle style)
  {
    switch (style)
    {
    case WingStyle::Fairy:
      return {14.f, 6.f};
    case WingStyle::Bat:
      return {5.f, 3.f};
    default:
      return {3.f, 2.f};
    }
  }
}

void HairNode::updateWings(float dt, bool tookOff)
{
  if (m_config.wings == WingStyle::None || m_needsReset || dt <= 0.f)
  {
    m_wingAngle = 0.f;
    m_wingSpeed = 0.f;
    return;
  }

  // A flap on every takeoff, then the spring brings them back
  if (tookOff)
    m_wingSpeed += kWingKick * m_config.wingFlap;
  m_wingSpeed += (-m_wingAngle * kWingSpring - m_wingSpeed * kWingDamping) * dt;
  m_wingAngle += m_wingSpeed * dt;
}

void HairNode::drawWings(CCDrawNode *node)
{
  if (m_config.wings == WingStyle::None)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const outline = m_config.outline ? kOutlineWidth * scale : 0.f;

  auto const color = m_config.wingColorSource == HairColorSource::Hair ? this->hairColor()
                                                                       : this->sourceColor(m_config.wingColorSource, m_config.wingColor);
  auto const outlineColor = this->ink(color.a);

  CCPoint const center = m_frameParams.headCenter;
  CCPoint const up = m_frameUp;
  CCPoint const across = normalized(m_frameBack, {-up.y, up.x});
  CCPoint const upNode = normalized(applyVec(up, simToNode), {0.f, 1.f});
  float const length = kWingLength * m_config.wingSize * scale;

  auto const flutter = flutterOf(m_config.wings);
  float const idle = std::sin(m_time * flutter.speed) * flutter.amount;
  float const angle = radians(kWingSpread + m_wingAngle + idle - kWingFold * m_motion - kWingSleepy * m_sleepy);

  for (float side : {-1.f, 1.f})
  {
    // Out from the side of the head, raised by `angle`
    CCPoint const outward = across * side;
    CCPoint const rootSim = center + outward * (this->headEdge(outward) * kWingRoot) - up * (kHeadRadius * kWingDrop * m_simScale);
    CCPoint const dirSim = normalized(outward * std::cos(angle) + up * std::sin(angle), outward);

    CCPoint const root = CCPointApplyAffineTransform(rootSim, simToNode);
    CCPoint const o = normalized(applyVec(dirSim, simToNode), {1.f, 0.f});
    CCPoint const n = normalized(upNode - o * upNode.dot(o), upNode); // the upper side of the wing

    switch (m_config.wings)
    {
    case WingStyle::Angel:
    {
      // Feathers fanning down from the top arm, the back ones first
      for (int i = 3; i >= 0; --i)
      {
        float const fan = radians(static_cast<float>(i) * 14.f - 8.f);
        CCPoint const dir = normalized(o * std::cos(fan) - n * std::sin(fan), o);
        float const along = length * (.55f - .07f * static_cast<float>(i));
        fillEllipse(node, root + dir * (length * (.42f + .04f * static_cast<float>(i))), dir, along, length * .17f, shaded(color, 1.f - .05f * static_cast<float>(i)),
                    outline, outlineColor);
      }
      fillEllipse(node, root + o * (length * .35f) + n * (length * .08f), o, length * .45f, length * .16f, color, outline, outlineColor);
      break;
    }
    case WingStyle::Fairy:
    {
      // Two see-through lobes with light veins
      auto const glass = faded(color, .6f);
      auto const vein = faded(mixedWhite(color, .5f), .8f);
      CCPoint const upper = normalized(o + n * .5f, o);
      CCPoint const lower = normalized(o - n * .6f, o);
      fillEllipse(node, root + upper * (length * .5f), upper, length * .55f, length * .3f, glass, outline * .7f, faded(outlineColor, .8f));
      fillEllipse(node, root + lower * (length * .38f), lower, length * .4f, length * .2f, glass, outline * .7f, faded(outlineColor, .8f));
      node->drawSegment(root, root + upper * (length * .85f), length * .03f, vein);
      node->drawSegment(root, root + lower * (length * .6f), length * .03f, vein);
      break;
    }
    case WingStyle::Bat:
    {
      // Pointed top, scalloped bottom; the edges of the pieces read as the ribs
      CCPoint const tip = root + o * length + n * (length * .18f);
      CCPoint const b1 = root + o * (length * .78f) - n * (length * .2f);
      CCPoint const b2 = root + o * (length * .5f) - n * (length * .06f);
      CCPoint const b3 = root + o * (length * .25f) - n * (length * .18f);
      for (auto const &piece : {std::array<CCPoint, 3>{root, tip, b1}, std::array<CCPoint, 3>{root, b1, b2}, std::array<CCPoint, 3>{root, b2, b3}})
      {
        auto verts = piece;
        node->drawPolygon(verts.data(), 3, color, outline * .8f, outlineColor);
      }
      break;
    }
    case WingStyle::None:
      break;
    }
  }
}
