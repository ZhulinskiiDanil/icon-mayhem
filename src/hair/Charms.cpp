#include "HairNode.hpp"
#include "HairShared.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

using namespace geode::prelude;
using namespace hair;

// ! --- Charms --- !
// Headphones over the head that glow to the music, glasses on the face, earrings and a cat bell
// swinging on little pendulums. Sizes are in icon units.

namespace
{
  // Headphones, in the hairstyle frame like the headband
  constexpr float kBandFrom = -78.f;  // degrees from the top, the band reaches down to the cups
  constexpr int kBandPoints = 19;
  constexpr float kBandLift = 1.4f;   // above the edge of the head, over the hair
  constexpr float kBandWidth = 2.2f;
  constexpr float kBandRound = 1.18f; // how far the arch reaches into the corners of a cube, of the half size
  constexpr float kCupLength = 5.4f;  // along the side of the head
  constexpr float kCupDepth = 2.8f;   // sticking out of the side
  constexpr float kCupBump = .7f;     // how far the cups move out on a strong beat
  constexpr float kCatEarAngle = 32.f;
  constexpr float kCatEarSize = 4.f;

  // Glasses, in the icon frame like the stickers
  constexpr float kLensRadius = 3.4f;
  constexpr float kFrameWidth = .5f;
  constexpr int kLensPoints = 28;

  // Earrings and the bell
  constexpr float kCharmGravity = 1.f;  // of the hair gravity
  constexpr float kCharmAccel = .35f;   // how much the head's acceleration throws them
  constexpr float kCharmMaxAccel = 2.5f; // of the gravity, a teleport or a respawn doesn't spin them around
  constexpr float kCharmDamping = 2.5f; // 1 / s
  constexpr float kEarringRadius = 1.35f;
  constexpr float kEarringScale = 1.25f; // the charms are drawn this much bigger than the size setting
  constexpr float kBellRadius = 2.1f;
  constexpr float kBellChain = .5f;
  constexpr float kCollarHeight = 1.6f; // up from the bottom edge of the head
  constexpr float kCollarWidth = 1.8f;
  constexpr float kCollarSag = .6f;
  constexpr int kCollarPoints = 7;
  constexpr float kScarfSag = .8f; // as the scarf band, see Extras.cpp

  // Outline of a lens around its center, `up` and `right` are unit vectors in node space
  std::vector<CCPoint> lensShape(GlassesStyle style, CCPoint const &center, CCPoint const &up, CCPoint const &right, float radius)
  {
    std::vector<CCPoint> points;
    points.reserve(kLensPoints);
    switch (style)
    {
    case GlassesStyle::Hearts:
      // The classic heart curve, scaled to about the size of the round lens
      for (int i = 0; i < kLensPoints; ++i)
      {
        float const t = 2.f * kPi * static_cast<float>(i) / static_cast<float>(kLensPoints);
        float const x = std::pow(std::sin(t), 3.f);
        float const y = (13.f * std::cos(t) - 5.f * std::cos(2.f * t) - 2.f * std::cos(3.f * t) - std::cos(4.f * t)) / 16.f;
        points.push_back(center + right * (x * radius * 1.12f) + up * ((y + .12f) * radius * 1.12f));
      }
      break;
    case GlassesStyle::Stars:
      for (int i = 0; i < 10; ++i)
      {
        float const r = i % 2 == 0 ? radius * 1.3f : radius * .68f;
        points.push_back(center + rotated(up, radians(-36.f * static_cast<float>(i))) * r);
      }
      break;
    default:
      for (int i = 0; i < kLensPoints; ++i)
      {
        float const t = 2.f * kPi * static_cast<float>(i) / static_cast<float>(kLensPoints);
        points.push_back(center + right * (std::cos(t) * radius) + up * (std::sin(t) * radius));
      }
      break;
    }
    return points;
  }

  // Fills a shape that every ray from `center` crosses once (a heart, a star) with triangles,
  // drawPolygon only handles convex shapes
  void fillFan(CCDrawNode *node, CCPoint const &center, std::vector<CCPoint> const &points, ccColor4F const &color)
  {
    for (size_t i = 0; i < points.size(); ++i)
    {
      std::array<CCPoint, 3> piece = {center, points[i], points[(i + 1) % points.size()]};
      node->drawPolygon(piece.data(), 3, color, 0.f, color);
    }
  }

  void drawRing(CCDrawNode *node, std::vector<CCPoint> const &points, float radius, ccColor4F const &color)
  {
    for (size_t i = 0; i < points.size(); ++i)
      node->drawSegment(points[i], points[(i + 1) % points.size()], radius, color);
  }
}

bool HairNode::charmsActive() const
{
  return m_config.headphones != HeadphoneStyle::None || m_config.glasses != GlassesStyle::None ||
         m_config.earrings != EarringStyle::None || m_config.bell;
}

// ! --- Update --- !

void HairNode::updateCharms(float dt)
{
  // The music pulse: jumps up on a beat, fades slowly. Without level music a slow breathing glow
  if (m_config.headphones != HeadphoneStyle::None)
  {
    if (m_musicDriven && m_config.headphonesBeat)
    {
      float const pulse = std::clamp(FMODAudioEngine::sharedEngine()->m_pulse1, 0.f, 1.f);
      m_beat = pulse > m_beat ? pulse : m_beat * std::exp(-6.f * dt);
    }
    else
      m_beat = .35f + .25f * std::sin(m_time * 2.f);
  }

  if (m_config.earrings == EarringStyle::None && !m_config.bell)
    return;

  if (m_needsReset || dt <= 0.f)
  {
    for (auto &swing : m_earringSwing)
      swing.reset();
    m_bellSwing.reset();
    return;
  }

  CCPoint const down = m_gravityDir ? m_gravityDir() : CCPoint{0.f, -1.f};
  float const gravity = kBaseGravity * kCharmGravity * m_simScale;
  CCPoint accel = (m_headVelocity - m_lastHeadVelocity) / dt * kCharmAccel;
  float const maxAccel = gravity * kCharmMaxAccel;
  if (accel.getLength() > maxAccel)
    accel = accel * (maxAccel / accel.getLength());

  if (m_config.earrings != EarringStyle::None)
  {
    float const length = (m_config.earringLength + kEarringRadius * m_config.earringSize) * m_simScale;
    for (auto &swing : m_earringSwing)
      swing.update(dt, down, accel, gravity, std::max(length, .5f * m_simScale), kCharmDamping);
  }
  if (m_config.bell)
  {
    float const length = (kBellChain + kBellRadius * m_config.bellSize) * m_simScale;
    m_bellSwing.update(dt, down, accel, gravity, length, kCharmDamping);
  }
}

// ! --- Headphones --- !

void HairNode::drawHeadphones(CCDrawNode *node)
{
  if (m_config.headphones == HeadphoneStyle::None)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const outline = m_config.outline ? kOutlineWidth * scale : 0.f;
  float const size = m_config.headphonesSize;

  auto const color = m_config.headphonesColorSource == HairColorSource::Hair
                         ? this->hairColor()
                         : this->sourceColor(m_config.headphonesColorSource, m_config.headphonesColor);
  auto const outlineColor = ccColor4F{0.f, 0.f, 0.f, color.a};
  auto const light = premultiplied(m_config.headphonesLight, color.a);

  // Around the head in the hairstyle frame, over the hair
  CCPoint const center = m_frameParams.headCenter;
  CCPoint const across = normalized(m_frameBack, {-m_frameUp.y, m_frameUp.x});
  float const lift = kBandLift * m_simScale;
  float const bump = kCupBump * m_beat * m_beat * m_simScale;
  // A round arch: on a cube it cuts the corners instead of following them like a frame
  float const roundEdge = this->headEdge(m_frameUp) * kBandRound;
  auto arcPoint = [&](float degrees, float extra, CCPoint *outward = nullptr)
  {
    CCPoint const radial = normalized(m_frameUp * std::cos(radians(degrees)) + across * std::sin(radians(degrees)), m_frameUp);
    if (outward)
      *outward = normalized(applyVec(radial, simToNode), {0.f, 1.f});
    return CCPointApplyAffineTransform(center + radial * (std::min(this->headEdge(radial), roundEdge) + extra), simToNode);
  };

  // The band, rising a little with the beat
  std::array<CCPoint, kBandPoints> band;
  for (int i = 0; i < kBandPoints; ++i)
  {
    float const t = static_cast<float>(i) / static_cast<float>(kBandPoints - 1);
    band[i] = arcPoint(kBandFrom + (-2.f * kBandFrom) * t, lift + bump * .5f);
  }
  float const radius = kBandWidth * .5f * size * scale;
  for (int pass = outline > 0.f ? 0 : 1; pass < 2; ++pass)
  {
    for (int i = 1; i < kBandPoints; ++i)
      node->drawSegment(band[i - 1], band[i], pass == 0 ? radius + outline : radius, pass == 0 ? outlineColor : color);
  }
  // A soft shine along the top of the band
  for (int i = kBandPoints / 3 + 1; i < kBandPoints * 2 / 3; ++i)
  {
    CCPoint outward;
    arcPoint(0.f, 0.f, &outward);
    node->drawSegment(band[i - 1] + outward * (radius * .35f), band[i] + outward * (radius * .35f), radius * .25f,
                      mixedWhite(color, .45f));
  }

  // Little cat ears standing on the band
  if (m_config.headphones == HeadphoneStyle::CatEars)
  {
    for (float side : {-1.f, 1.f})
    {
      CCPoint outward;
      CCPoint const base = arcPoint(side * kCatEarAngle, lift + bump * .5f, &outward);
      CCPoint const along = perpendicular(outward) * -1.f;
      float const ear = kCatEarSize * size * scale;
      std::array<CCPoint, 3> shape = {base - along * (ear * .5f), base + along * (ear * .5f),
                                      base + outward * ear + along * (side * ear * .12f)};
      node->drawPolygon(shape.data(), 3, color, outline, outlineColor);
      std::array<CCPoint, 3> inner = {base - along * (ear * .25f) + outward * (ear * .15f), base + along * (ear * .25f) + outward * (ear * .15f),
                                      base + outward * (ear * .7f) + along * (side * ear * .08f)};
      node->drawPolygon(inner.data(), 3, faded(light, .9f), 0.f, light);
    }
  }

  // Ear cups on both sides, the lights glow with the beat
  float const pop = 1.f + .06f * m_beat;
  for (float side : {-1.f, 1.f})
  {
    CCPoint outward;
    CCPoint const cup = arcPoint(side * 90.f, kCupDepth * .35f * size * m_simScale + bump, &outward);
    CCPoint const along = perpendicular(outward);
    float const length = kCupLength * size * scale * pop;
    float const depth = kCupDepth * size * scale * pop;

    fillEllipse(node, cup, along, length * 1.3f, depth * 1.7f, faded(light, .3f * m_beat));
    fillEllipse(node, cup, along, length, depth, color, outline, outlineColor);
    fillEllipse(node, cup, along, length * .66f, depth * .58f, faded(light, .45f + .55f * m_beat));
    fillEllipse(node, cup, along, length * .4f, depth * .3f, shaded(color, .9f));
    // Where the cup holds on to the band, from its end nearer to the band
    CCPoint const bandEnd = band[side < 0.f ? 0 : kBandPoints - 1];
    CCPoint const top = (cup + along * length).getDistance(bandEnd) < (cup - along * length).getDistance(bandEnd)
                            ? cup + along * (length * .8f)
                            : cup - along * (length * .8f);
    node->drawSegment(top, bandEnd, radius * .8f, shaded(color, .85f));
  }
}

// ! --- Glasses --- !

void HairNode::drawGlasses(CCDrawNode *node)
{
  if (m_config.glasses == GlassesStyle::None)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const alpha = m_head->getDisplayedOpacity() / 255.f;
  float const outline = m_config.outline ? kOutlineWidth * scale * .7f : 0.f;
  auto const outlineColor = ccColor4F{0.f, 0.f, 0.f, alpha};
  auto const frame = premultiplied(m_config.glassesColor, alpha);
  auto const tint = premultiplied(m_config.glassesTint, alpha * m_config.glassesTintOpacity);
  auto const glint = ccColor4F{.8f * alpha, .8f * alpha, .8f * alpha, .8f * alpha};

  // On the face like the blush, in the icon's own frame
  CCPoint const right = normalized(applyVec(m_iconBack * -1.f, simToNode), {1.f, 0.f});
  CCPoint const up = normalized(applyVec(m_iconUp, simToNode), {0.f, 1.f});
  CCPoint const center = CCPointApplyAffineTransform(m_frameParams.headCenter, simToNode) + up * (m_config.glassesY * scale);
  float const radius = kLensRadius * m_config.glassesSize * scale;
  float const width = kFrameWidth * m_config.glassesSize * scale;
  float const spread = m_config.glassesX * scale;

  std::array<std::vector<CCPoint>, 2> lenses;
  std::array<CCPoint, 2> centers;
  for (int i = 0; i < 2; ++i)
  {
    float const side = i == 0 ? -1.f : 1.f;
    centers[i] = center + right * (side * spread);
    lenses[i] = lensShape(m_config.glasses, centers[i], up, right, radius);
  }

  // The bridge over the nose and the arms to the sides of the head
  CCPoint const bridgeLeft = centers[0] + right * (radius * .85f) + up * (radius * .15f);
  CCPoint const bridgeRight = centers[1] - right * (radius * .85f) + up * (radius * .15f);
  CCPoint const bridgeTop = (bridgeLeft + bridgeRight) * .5f + up * (radius * .3f);
  std::vector<std::pair<CCPoint, CCPoint>> bars;
  if (spread > radius * .9f)
  {
    bars.emplace_back(bridgeLeft, bridgeTop);
    bars.emplace_back(bridgeTop, bridgeRight);
  }
  for (int i = 0; i < 2; ++i)
  {
    float const side = i == 0 ? -1.f : 1.f;
    CCPoint const dir = m_iconBack * -side;
    CCPoint const edge = CCPointApplyAffineTransform(m_frameParams.headCenter + dir * this->headEdge(dir), simToNode) +
                         up * (m_config.glassesY * scale + radius * .3f);
    CCPoint const start = centers[i] + right * (side * radius * .95f) + up * (radius * .3f);
    if ((edge - center).dot(right * side) > (start - center).dot(right * side))
      bars.emplace_back(start, edge);
  }

  for (int pass = outline > 0.f ? 0 : 1; pass < 2; ++pass)
  {
    float const grow = pass == 0 ? outline : 0.f;
    auto const color = pass == 0 ? outlineColor : frame;
    for (auto const &[from, to] : bars)
      node->drawSegment(from, to, width * .8f + grow, color);
    for (auto const &lens : lenses)
      drawRing(node, lens, width + grow, color);
  }

  // Tinted glass inside the frames and a little glint
  for (int i = 0; i < 2; ++i)
  {
    if (tint.a > .001f)
    {
      if (m_config.glasses == GlassesStyle::Round)
        node->drawPolygon(lenses[i].data(), static_cast<unsigned>(lenses[i].size()), tint, 0.f, tint);
      else
        fillFan(node, centers[i], lenses[i], tint);
    }
    node->drawSegment(centers[i] - right * (radius * .45f) + up * (radius * .2f), centers[i] - right * (radius * .15f) + up * (radius * .5f),
                      width * .45f, glint);
  }
}

// ! --- Earrings --- !

void HairNode::drawEarrings(CCDrawNode *node)
{
  if (m_config.earrings == EarringStyle::None)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const outline = m_config.outline ? kOutlineWidth * scale * .8f : 0.f;

  auto const color = m_config.earringColorSource == HairColorSource::Hair
                         ? this->hairColor()
                         : this->sourceColor(m_config.earringColorSource, m_config.earringColor);
  auto const outlineColor = ccColor4F{0.f, 0.f, 0.f, color.a};
  auto const shine = ccColor4F{.85f * color.a, .85f * color.a, .85f * color.a, .85f * color.a};
  CCPoint const down = m_gravityDir ? m_gravityDir() : CCPoint{0.f, -1.f};
  float const size = m_config.earringSize * kEarringScale * scale;

  for (int i = 0; i < 2; ++i)
  {
    // Hooked on the side of the head in the icon frame, hanging along gravity
    float const side = i == 0 ? -1.f : 1.f;
    CCPoint const dir = m_iconBack * -side;
    CCPoint const anchorSim = m_frameParams.headCenter + dir * (this->headEdge(dir) - .3f * m_simScale) +
                              m_iconUp * (m_config.earringHeight * m_simScale);
    CCPoint const hang = m_earringSwing[i].direction(down);
    CCPoint const chainEndSim = anchorSim + hang * (m_config.earringLength * m_simScale);

    CCPoint const anchor = CCPointApplyAffineTransform(anchorSim, simToNode);
    CCPoint const chainEnd = CCPointApplyAffineTransform(chainEndSim, simToNode);
    CCPoint const hangNode = normalized(applyVec(hang, simToNode), {0.f, -1.f});
    CCPoint const up = hangNode * -1.f;
    CCPoint const across = perpendicular(up);
    CCPoint const charm = chainEnd + hangNode * (kEarringRadius / kEarringScale * size);

    if (m_config.earringLength > .05f)
    {
      if (outline > 0.f)
        node->drawSegment(anchor, chainEnd, .22f * scale + outline * .6f, outlineColor);
      node->drawSegment(anchor, chainEnd, .22f * scale, shaded(color, .8f));
    }
    if (outline > 0.f)
      fillCircle(node, anchor, .5f * scale + outline * .6f, outlineColor);
    fillCircle(node, anchor, .5f * scale, color);

    switch (m_config.earrings)
    {
    case EarringStyle::Drops:
    {
      CCPoint const ball = charm - up * (size * .3f);
      for (int pass = outline > 0.f ? 0 : 1; pass < 2; ++pass)
      {
        float const grow = pass == 0 ? outline : 0.f;
        auto const fill = pass == 0 ? outlineColor : color;
        fillCircle(node, ball, size * 1.05f + grow, fill);
        std::array<CCPoint, 3> tip = {ball - across * (size * 1.03f + grow), ball + across * (size * 1.03f + grow),
                                      charm + up * (size * 1.25f + grow)};
        node->drawPolygon(tip.data(), 3, fill, 0.f, fill);
      }
      fillCircle(node, ball - across * (size * .35f) + up * (size * .3f), size * .25f, shine);
      break;
    }
    case EarringStyle::Hearts:
      if (outline > 0.f)
        drawHeart(node, charm, up, size * 1.3f + outline * 1.4f, outlineColor);
      drawHeart(node, charm, up, size * 1.3f, color);
      fillCircle(node, charm - across * (size * .55f) + up * (size * .45f), size * .2f, shine);
      break;
    case EarringStyle::Stars:
      if (outline > 0.f)
        drawStar(node, charm, up, size * 1.7f + outline * 1.6f, size * .8f + outline, outlineColor);
      drawStar(node, charm, up, size * 1.7f, size * .8f, color);
      break;
    case EarringStyle::Pearls:
      if (outline > 0.f)
        fillCircle(node, charm, size * 1.05f + outline, outlineColor);
      fillCircle(node, charm, size * 1.05f, color);
      fillCircle(node, charm - across * (size * .3f) + up * (size * .35f), size * .32f, shine);
      break;
    default:
      break;
    }
  }
}

// ! --- Collar and bell --- !

void HairNode::drawCollarAndBell(CCDrawNode *node)
{
  if (!m_config.bell)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const alpha = m_head->getDisplayedOpacity() / 255.f;
  float const outline = m_config.outline ? kOutlineWidth * scale : 0.f;
  auto const outlineColor = ccColor4F{0.f, 0.f, 0.f, alpha};

  // Across the bottom of the face in the icon frame, like the scarf
  float const unit = m_simScale;
  float const half = kHeadRadius * unit;
  CCPoint const center = m_frameParams.headCenter;
  CCPoint const right = m_iconBack * -1.f;
  CCPoint anchorSim;

  if (m_config.scarf)
  {
    // The bell hangs from the middle of the scarf
    float const height = -half + m_config.scarfHeight * unit;
    anchorSim = center + m_iconUp * (height - kScarfSag * unit - m_config.scarfWidth * .45f * unit);
  }
  else
  {
    float const height = -half + kCollarHeight * unit;
    float const reach = half + .4f * unit;
    std::array<CCPoint, kCollarPoints> band;
    for (int i = 0; i < kCollarPoints; ++i)
    {
      float const x = static_cast<float>(i) / static_cast<float>(kCollarPoints - 1) * 2.f - 1.f;
      float const sag = kCollarSag * unit * (1.f - x * x);
      band[i] = CCPointApplyAffineTransform(center + right * (x * reach) + m_iconUp * (height - sag), simToNode);
    }
    float const radius = kCollarWidth * .5f * scale;
    auto const collar = premultiplied(m_config.collarColor, alpha);
    for (int pass = outline > 0.f ? 0 : 1; pass < 2; ++pass)
    {
      for (int i = 1; i < kCollarPoints; ++i)
        node->drawSegment(band[i - 1], band[i], pass == 0 ? radius + outline : radius, pass == 0 ? outlineColor : collar);
    }
    anchorSim = center + m_iconUp * (height - kCollarSag * unit - kCollarWidth * .4f * unit);
  }

  // The bell on a tiny ring, swinging along gravity
  CCPoint const down = m_gravityDir ? m_gravityDir() : CCPoint{0.f, -1.f};
  CCPoint const hang = m_bellSwing.direction(down);
  float const size = m_config.bellSize * scale;
  float const radius = kBellRadius * size;
  CCPoint const anchor = CCPointApplyAffineTransform(anchorSim, simToNode);
  CCPoint const hangNode = normalized(applyVec(hang, simToNode), {0.f, -1.f});
  CCPoint const up = hangNode * -1.f;
  CCPoint const across = perpendicular(up);
  CCPoint const bell = anchor + hangNode * (kBellChain * size + radius);

  auto const color = premultiplied(m_config.bellColor, alpha);
  auto const dark = shaded(color, .45f);
  if (outline > 0.f)
  {
    fillCircle(node, anchor, .6f * size + outline, outlineColor);
    fillCircle(node, bell, radius + outline, outlineColor);
  }
  fillCircle(node, anchor, .6f * size, shaded(color, .8f));
  fillCircle(node, bell, radius, color);
  // A ridge around the middle, the slit with the ball at the bottom and a shine
  node->drawSegment(bell - across * (radius * .9f) + up * (radius * .15f), bell + across * (radius * .9f) + up * (radius * .15f),
                    .22f * size, shaded(color, .78f));
  node->drawSegment(bell - up * (radius * .45f) - across * (radius * .35f), bell - up * (radius * .45f) + across * (radius * .35f),
                    .17f * size, dark);
  fillCircle(node, bell - up * (radius * .62f), .3f * size, dark);
  fillCircle(node, bell + up * (radius * .42f) - across * (radius * .4f), .38f * size, ccColor4F{.8f * alpha, .8f * alpha, .8f * alpha, .8f * alpha});
}
