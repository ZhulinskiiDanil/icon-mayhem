#include "HairNode.hpp"
#include "HairShared.hpp"

#include <array>
#include <cmath>
#include <random>

using namespace geode::prelude;
using namespace hair;

// ! --- Pet --- !
// A tiny cat, ghost or bird floating behind and above the icon on a lazy spring. It bobs, looks
// where you go, blinks now and then and hops when you land. Sizes are in icon units.

namespace
{
  constexpr float kPetBody = 4.2f;    // body radius at pet size 1
  constexpr float kPetHeight = 6.f;   // above the top of the head
  constexpr float kPetSpring = 40.f;  // 1 / s^2
  constexpr float kPetDamping = 7.f;  // 1 / s
  constexpr float kPetBob = 1.2f;     // icon units
  constexpr float kPetBlink = .12f;   // s the eyes stay closed
  constexpr ccColor3B kEye = {43, 33, 64};
  constexpr ccColor3B kPetBlush = {255, 143, 163};
  constexpr ccColor3B kInnerEar = {244, 167, 185};
  constexpr ccColor3B kBeak = {255, 179, 71};
}

CCPoint HairNode::petTarget(CCPoint const &headCenter) const
{
  // Behind the head (against the movement) and above it, in world up
  CCPoint const worldUp = m_gravityDir ? m_gravityDir() * -1.f : CCPoint{0.f, 1.f};
  CCPoint worldUpFrame;
  CCPoint worldBack;
  this->gravityAxes(worldUpFrame, worldBack);
  CCPoint const back = normalized(worldBack, perpendicular(worldUp));
  float const bob = std::sin(m_time * 2.5f) * kPetBob;
  return headCenter + worldUp * ((kHeadRadius + kPetHeight + bob) * m_simScale) +
         back * ((kHeadRadius + m_config.petDistance) * m_simScale);
}

void HairNode::updatePet(float dt, CCPoint const &headCenter)
{
  if (m_config.pet == PetStyle::None)
    return;

  // Snaps back after a respawn or a teleport, otherwise follows lazily
  CCPoint const target = this->petTarget(headCenter);
  if (m_needsReset || target.getDistance(m_petPosition) > kHeadRadius * 10.f * m_simScale)
  {
    m_petPosition = target;
    m_petVelocity = CCPoint{};
  }
  else
  {
    m_petVelocity = m_petVelocity + ((target - m_petPosition) * kPetSpring - m_petVelocity * kPetDamping) * dt;
    m_petPosition = m_petPosition + m_petVelocity * dt;
  }

  // Blink every few seconds
  m_petBlink -= dt;
  if (m_petBlink < -kPetBlink)
    m_petBlink = 2.f + 3.f * std::uniform_real_distribution<float>(0.f, 1.f)(m_random);
}

void HairNode::drawPet(CCDrawNode *node)
{
  if (m_config.pet == PetStyle::None)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const alpha = m_head->getDisplayedOpacity() / 255.f;
  float const outline = m_config.outline ? kOutlineWidth * scale : 0.f;
  auto const outlineColor = ccColor4F{0.f, 0.f, 0.f, alpha};

  auto color = this->sourceColor(m_config.petColorSource, m_config.petColor);
  CCPoint const center = CCPointApplyAffineTransform(m_petPosition, simToNode);
  CCPoint const up = normalized(applyVec(m_gravityDir ? m_gravityDir() * -1.f : CCPoint{0.f, 1.f}, simToNode), {0.f, 1.f});
  CCPoint const right = perpendicular(up) * -1.f;
  float const face = m_facing >= 0.f ? 1.f : -1.f; // looks where you go
  float const r = kPetBody * m_config.petSize * scale;

  auto body = [&](ccColor4F const &fill)
  {
    if (outline > 0.f)
      fillCircle(node, center, r + outline, outlineColor);
    fillCircle(node, center, r, fill);
  };

  switch (m_config.pet)
  {
  case PetStyle::Cat:
  {
    // Swishing tail behind, pointy ears, round body
    CCPoint previous = center - right * (face * r * .8f);
    for (int pass = outline > 0.f ? 0 : 1; pass < 2; ++pass)
    {
      previous = center - right * (face * r * .8f);
      for (int i = 1; i <= 6; ++i)
      {
        float const t = static_cast<float>(i);
        CCPoint const point = center - right * (face * (r * .8f + t * r * .3f)) + up * (std::sin(m_time * 3.f + t * .6f) * r * .25f + t * r * .12f);
        node->drawSegment(previous, point, pass == 0 ? r * .22f + outline : r * .22f, pass == 0 ? outlineColor : color);
        previous = point;
      }
    }
    for (float side : {-1.f, 1.f})
    {
      CCPoint const base = center + up * (r * .55f) + right * (side * r * .5f);
      std::array<CCPoint, 3> ear = {base - right * (r * .32f), base + right * (r * .32f), base + up * (r * .75f) + right * (side * r * .1f)};
      node->drawPolygon(ear.data(), 3, color, outline, outlineColor);
      std::array<CCPoint, 3> inner = {base - right * (r * .15f) + up * (r * .1f), base + right * (r * .15f) + up * (r * .1f),
                                      base + up * (r * .5f) + right * (side * r * .07f)};
      node->drawPolygon(inner.data(), 3, premultiplied(kInnerEar, alpha), 0.f, outlineColor);
    }
    body(color);
    break;
  }
  case PetStyle::Ghost:
  {
    // A dome with a wavy hem, a bit see-through
    color = faded(color, .9f);
    for (int pass = outline > 0.f ? 0 : 1; pass < 2; ++pass)
    {
      float const grow = pass == 0 ? outline : 0.f;
      auto const fill = pass == 0 ? outlineColor : color;
      std::vector<CCPoint> dome;
      for (int i = 0; i <= 12; ++i)
      {
        float const angle = kPi * static_cast<float>(i) / 12.f;
        dome.push_back(center + right * (std::cos(angle) * (r + grow)) + up * (std::sin(angle) * (r + grow)));
      }
      dome.push_back(center - right * (r + grow) - up * (r * .6f));
      dome.push_back(center + right * (r + grow) - up * (r * .6f));
      node->drawPolygon(dome.data(), static_cast<unsigned>(dome.size()), fill, 0.f, fill);
      for (int i = -1; i <= 1; ++i)
        fillCircle(node, center + right * (static_cast<float>(i) * r * .66f) - up * (r * .6f), r * .34f + grow, fill);
    }
    break;
  }
  case PetStyle::Bird:
  {
    // Tail feathers behind, round body, a flapping wing, a beak in front
    std::array<CCPoint, 3> tail = {center - right * (face * r * .6f) + up * (r * .2f), center - right * (face * r * .6f) - up * (r * .3f),
                                   center - right * (face * r * 1.6f) + up * (r * .1f)};
    node->drawPolygon(tail.data(), 3, shaded(color, .85f), outline, outlineColor);
    body(color);
    CCPoint const wing = rotated(right * -face, radians(std::sin(m_time * 12.f) * 25.f));
    fillEllipse(node, center - right * (face * r * .1f) - up * (r * .05f), wing, r * .55f, r * .3f, shaded(color, .85f), outline * .8f, outlineColor);
    std::array<CCPoint, 3> beak = {center + right * (face * r * .85f) + up * (r * .18f), center + right * (face * r * .85f) - up * (r * .12f),
                                   center + right * (face * r * 1.35f) + up * (r * .03f)};
    node->drawPolygon(beak.data(), 3, premultiplied(kBeak, alpha), outline * .8f, outlineColor);
    break;
  }
  case PetStyle::None:
    break;
  }

  // Face: eyes looking where you go (lines while blinking) and blush
  bool const blinking = m_petBlink < 0.f;
  bool const bird = m_config.pet == PetStyle::Bird;
  std::array<float, 2> eyes = {face * r * .45f, face * r * .02f};
  for (size_t i = 0; i < (bird ? 1u : 2u); ++i)
  {
    CCPoint const eye = center + right * eyes[i] + up * (r * .1f);
    if (blinking)
      node->drawSegment(eye - right * (r * .12f), eye + right * (r * .12f), r * .05f, premultiplied(kEye, alpha));
    else
    {
      fillCircle(node, eye, r * .14f, premultiplied(kEye, alpha));
      fillCircle(node, eye + up * (r * .05f) + right * (r * .04f), r * .05f, {alpha, alpha, alpha, alpha});
    }
    fillCircle(node, eye - up * (r * .25f) + right * (face * r * .05f), r * .1f, premultiplied(kPetBlush, alpha * .6f));
  }
}
