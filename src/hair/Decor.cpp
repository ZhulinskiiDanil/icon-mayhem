#include "HairNode.hpp"
#include "HairShared.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

using namespace geode::prelude;
using namespace hair;

// ! --- Decorations --- !
// Hair shine and dyed tips (colors of the locks), clips riding the bangs, blush and stickers on the
// face, headband, flowers, the halo and hats. Sizes are in icon units (the cube is about 30).

namespace
{
  // Hair look
  constexpr float kShineWidth = .09f; // part of a lock the shine covers on each side

  // Clips
  constexpr float kClipSize = 3.f;
  constexpr float kClipAlongBangs = .35f; // where on a bang the clip sits

  // Blush
  constexpr float kBlushWidth = 3.6f;
  constexpr float kBlushHeight = 2.2f;
  constexpr int kBlushPoints = 20;

  // Headband: an arc over the top of the head
  constexpr float kHeadbandFrom = -52.f; // degrees from the top, the ends go under the hair on the sides
  constexpr int kHeadbandPoints = 17;
  constexpr float kHeadbandEarSize = 4.5f;
  constexpr float kHeadbandBunnySize = 9.f;
  constexpr float kHeadbandBowSize = 4.5f;

  // Flowers
  constexpr float kFlowerSize = 3.2f;
  constexpr float kCrownFrom = -60.f; // degrees, the crown goes over the top
  constexpr int kCrownFlowers = 6;
  constexpr ccColor3B kDaisyCenter = {255, 210, 63};
  constexpr ccColor3B kLeafColor = {109, 190, 90};

  // Stickers
  constexpr ccColor3B kBandAid = {242, 199, 160};
  constexpr ccColor3B kBandAidPad = {233, 180, 138};
  constexpr ccColor3B kFreckle = {181, 112, 74};
  constexpr std::array<std::pair<float, float>, 5> kFreckles = {{{-1.2f, .4f}, {0.f, .9f}, {1.1f, .3f}, {-.5f, -.6f}, {.7f, -.7f}}};

  // Hats, sizes at hat size 1
  constexpr float kHatLandSquash = .14f; // how much a hat squashes right after a landing

  // Halo
  constexpr float kHaloWidth = 7.f;   // icon units, half width at size 1
  constexpr float kHaloHeight = 2.2f; // half height, it is seen from the side
  constexpr float kHaloThickness = .9f;
  constexpr float kHaloSpring = 120.f; // 1 / s^2
  constexpr float kHaloDamping = 12.f; // 1 / s
  constexpr float kHaloBob = .8f;      // icon units of the idle bob
  constexpr int kHaloPoints = 28;

  ccColor4F mixed(ccColor4F const &a, ccColor4F const &b, float t)
  {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
  }

  float smoothstep(float edge0, float edge1, float x)
  {
    float const t = std::clamp((x - edge0) / std::max(edge1 - edge0, .0001f), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
  }


}

// ! --- State --- !

bool HairNode::decorActive() const
{
  return m_config.blush || m_config.sparkles != SparkleStyle::None || m_config.clipCount > 0 || m_config.headband ||
         m_config.flowers != FlowerStyle::None || m_config.halo || m_config.petals || m_config.sleepy ||
         m_config.wings != WingStyle::None || m_config.pet != PetStyle::None || m_config.hat != HatStyle::None ||
         m_config.sticker != StickerStyle::None || m_config.reactions || m_config.cuteDeath || this->charmsActive();
}

// ! --- Hair look --- !

ccColor4F HairNode::lockColorAt(Lock const &lock, ccColor4F const &base, float along) const
{
  bool const isHair = lock.kind == LockKind::Back || lock.kind == LockKind::FaceLock || lock.kind == LockKind::Bang ||
                      lock.kind == LockKind::Tail || lock.kind == LockKind::Ahoge;
  if (!isHair)
    return base;

  ccColor4F color = base;

  // Dyed tips: the lock fades into the second color, shaded like the lock itself
  if (m_config.dyedTips)
  {
    float const shade = kBackShade + (1.f - kBackShade) * lock.depth;
    auto const tips = shaded(this->sourceColor(m_config.tipsColorSource, m_config.tipsColor), shade);
    color = mixed(color, tips, smoothstep(m_config.tipsStart, 1.f, along));
  }

  // Shine: a lighter band at the same place on every lock, together they make a ring
  if (m_config.hairShine)
  {
    float const distance = std::abs(along - m_config.shinePosition);
    if (distance < kShineWidth)
    {
      float const strength = smoothstep(0.f, 1.f, 1.f - distance / kShineWidth) * m_config.shineStrength * .75f;
      color = mixed(color, {color.a, color.a, color.a, color.a}, strength);
    }
  }

  return color;
}

// ! --- Clips --- !

bool HairNode::clipPlacement(int index, CCPoint &position, CCPoint &direction) const
{
  auto const &strands = m_sim.strands();

  // `side` > 0 is the back of the head, which is the left side of the face
  auto onChosenSide = [&](Lock const &lock)
  { return m_config.clipRight ? lock.side < 0.f : lock.side > 0.f; };

  auto pointOn = [&](size_t strand, float along)
  {
    auto const &points = strands[strand];
    float const at = along * static_cast<float>(points.size() - 1);
    size_t const i = std::min(static_cast<size_t>(at), points.size() - 2);
    float const t = at - static_cast<float>(i);
    position = points[i] + (points[i + 1] - points[i]) * t;
    direction = normalized(points[i + 1] - points[i], m_frameUp * -1.f);
  };

  // On the outer bangs of the chosen side
  std::vector<size_t> bangs;
  for (size_t s = m_bangsStart; s < m_ribbonsStart && s < strands.size(); ++s)
  {
    if (onChosenSide(m_locks[s]))
      bangs.push_back(s);
  }
  std::sort(bangs.begin(), bangs.end(), [&](size_t a, size_t b)
            { return std::abs(m_locks[a].side) > std::abs(m_locks[b].side); });
  if (static_cast<size_t>(index) < bangs.size())
  {
    pointOn(bangs[index], kClipAlongBangs);
    return true;
  }

  // Without bangs: down the face lock
  for (size_t s = m_frontStart; s < m_bangsStart && s < strands.size(); ++s)
  {
    if (onChosenSide(m_locks[s]))
    {
      pointOn(s, .18f + .14f * static_cast<float>(index));
      return true;
    }
  }

  // Without both: on the front edge of the head
  if (strands.empty() && m_frameParams.headCenter.equals(CCPoint{}))
    return false;
  CCPoint const across = normalized(m_frameBack, {-m_frameUp.y, m_frameUp.x});
  float const angle = (m_config.clipRight ? -1.f : 1.f) * (48.f + 14.f * static_cast<float>(index));
  CCPoint const radial = normalized(m_frameUp * std::cos(radians(angle)) + across * std::sin(radians(angle)), m_frameUp);
  position = m_frameParams.headCenter + radial * (this->headEdge(radial) * .95f);
  direction = perpendicular(radial);
  return true;
}

void HairNode::drawClips(CCDrawNode *node)
{
  if (m_config.clipCount <= 0)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const outline = m_config.outline ? kOutlineWidth * scale : 0.f;
  float const size = kClipSize * m_config.clipSize * scale;

  auto const color = this->sourceColor(m_config.clipColorSource, m_config.clipColor);
  auto const outlineColor = ccColor4F{0.f, 0.f, 0.f, color.a};
  auto const shine = faded({1.f, 1.f, 1.f, 1.f}, .7f * color.a);

  for (int i = 0; i < m_config.clipCount; ++i)
  {
    CCPoint positionSim;
    CCPoint directionSim;
    if (!this->clipPlacement(i, positionSim, directionSim))
      continue;

    CCPoint const center = CCPointApplyAffineTransform(positionSim, simToNode);
    CCPoint const along = normalized(applyVec(directionSim, simToNode), {0.f, -1.f});
    CCPoint const up = along * -1.f;

    // Outline first: the same shape a bit bigger in black
    switch (m_config.clipStyle)
    {
    case ClipStyle::Star:
      if (outline > 0.f)
        drawStar(node, center, up, size + outline * 1.6f, size * .45f + outline, outlineColor);
      drawStar(node, center, up, size, size * .45f, color);
      fillCircle(node, center + up * (size * .15f) + perpendicular(up) * (size * .12f), size * .12f, shine);
      break;
    case ClipStyle::Heart:
      if (outline > 0.f)
        drawHeart(node, center, up, size + outline * 1.4f, outlineColor);
      drawHeart(node, center, up, size, color);
      fillCircle(node, center + up * (size * .4f) + perpendicular(up) * (size * .5f), size * .14f, shine);
      break;
    case ClipStyle::XPin:
      for (float turn : {45.f, -45.f})
      {
        CCPoint const bar = rotated(along, radians(turn)) * size;
        if (outline > 0.f)
          node->drawSegment(center - bar, center + bar, size * .17f + outline, outlineColor);
      }
      for (float turn : {45.f, -45.f})
      {
        CCPoint const bar = rotated(along, radians(turn)) * size;
        node->drawSegment(center - bar, center + bar, size * .17f, color);
      }
      break;
    case ClipStyle::Bar:
      if (outline > 0.f)
        node->drawSegment(center - along * size, center + along * size, size * .28f + outline, outlineColor);
      node->drawSegment(center - along * size, center + along * size, size * .28f, color);
      fillCircle(node, center - along * (size * .5f), size * .1f, shine);
      break;
    }
  }
}

// ! --- Blush --- !

void HairNode::drawBlush(CCDrawNode *node)
{
  if (!m_config.blush)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const alpha = m_head->getDisplayedOpacity() / 255.f;

  // On the face, so in the icon's own frame: it follows the sprite even when the hair stays upright
  float const pop = m_config.blushPop ? m_blushPop : 0.f;
  float const opacity = std::clamp(m_config.blushOpacity * (1.f + pop * .8f), 0.f, 1.f) * alpha;
  float const size = m_config.blushSize * (1.f + pop * .15f);
  CCPoint const right = normalized(applyVec(m_iconBack * -1.f, simToNode), {1.f, 0.f});
  CCPoint const up = normalized(applyVec(m_iconUp, simToNode), {0.f, 1.f});
  CCPoint const center = CCPointApplyAffineTransform(m_frameParams.headCenter, simToNode);

  for (float side : {-1.f, 1.f})
  {
    CCPoint const cheek = center + right * (side * m_config.blushSpread * scale) + up * (m_config.blushHeight * scale);

    // Soft edge: a few ovals on top of each other, the middle gets the most color
    for (auto const [radius, layer] : {std::pair{1.f, .35f}, std::pair{.75f, .35f}, std::pair{.5f, .3f}})
    {
      std::array<CCPoint, kBlushPoints> oval;
      for (int i = 0; i < kBlushPoints; ++i)
      {
        float const angle = 2.f * kPi * static_cast<float>(i) / static_cast<float>(kBlushPoints);
        oval[i] = cheek + right * (std::cos(angle) * kBlushWidth * size * radius * scale) +
                  up * (std::sin(angle) * kBlushHeight * size * radius * scale);
      }
      auto const color = premultiplied(m_config.blushColor, opacity * layer);
      node->drawPolygon(oval.data(), kBlushPoints, color, 0.f, color);
    }

    // Little anime lines over the cheek
    if (m_config.blushLines)
    {
      auto const line = shaded(premultiplied(m_config.blushColor, std::min(1.f, opacity * 1.6f)), .7f);
      for (int j = -1; j <= 1; ++j)
      {
        float const x = static_cast<float>(j) * 1.3f;
        CCPoint const top = cheek + right * ((x + .5f) * size * scale) + up * (.9f * size * scale);
        CCPoint const bottom = cheek + right * ((x - .3f) * size * scale) - up * (.9f * size * scale);
        node->drawSegment(top, bottom, .22f * scale, line);
      }
    }
  }
}

// ! --- Headband --- !

void HairNode::drawHeadband(CCDrawNode *node)
{
  if (!m_config.headband)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const outline = m_config.outline ? kOutlineWidth * scale : 0.f;

  auto const color = m_config.headbandColorSource == HairColorSource::Hair
                         ? this->hairColor()
                         : this->sourceColor(m_config.headbandColorSource, m_config.headbandColor);
  auto const outlineColor = ccColor4F{0.f, 0.f, 0.f, color.a};

  // Along the edge of the head over the top, from one side to the other
  CCPoint const center = m_frameParams.headCenter;
  CCPoint const across = normalized(m_frameBack, {-m_frameUp.y, m_frameUp.x});
  float const inset = m_config.headbandInset * m_simScale;
  auto arcPoint = [&](float degrees, CCPoint *outward = nullptr)
  {
    CCPoint const radial = normalized(m_frameUp * std::cos(radians(degrees)) + across * std::sin(radians(degrees)), m_frameUp);
    if (outward)
      *outward = normalized(applyVec(radial, simToNode), {0.f, 1.f});
    return CCPointApplyAffineTransform(center + radial * (this->headEdge(radial) - inset), simToNode);
  };

  std::array<CCPoint, kHeadbandPoints> arc;
  for (int i = 0; i < kHeadbandPoints; ++i)
  {
    float const t = static_cast<float>(i) / static_cast<float>(kHeadbandPoints - 1);
    arc[i] = arcPoint(kHeadbandFrom + (-2.f * kHeadbandFrom) * t);
  }

  float const radius = m_config.headbandWidth * .5f * scale;
  for (int pass = outline > 0.f ? 0 : 1; pass < 2; ++pass)
  {
    for (int i = 1; i < kHeadbandPoints; ++i)
      node->drawSegment(arc[i - 1], arc[i], pass == 0 ? radius + outline : radius, pass == 0 ? outlineColor : color);
  }

  // Decoration on top, swaying with the bows
  CCPoint outward;
  switch (m_config.headbandDeco)
  {
  case HeadbandDeco::Bow:
  {
    CCPoint const knot = arcPoint(0.f, &outward);
    drawBowShape(node, knot + outward * radius, outward, m_bowWobble, kHeadbandBowSize * scale, color, outline);
    break;
  }
  case HeadbandDeco::CatEars:
  case HeadbandDeco::BunnyEars:
  {
    bool const bunny = m_config.headbandDeco == HeadbandDeco::BunnyEars;
    for (float side : {-1.f, 1.f})
    {
      CCPoint const base = arcPoint(side * (bunny ? 20.f : 30.f), &outward);
      CCPoint const up = rotated(outward, radians(-side * (bunny ? 10.f : 6.f) + m_bowWobble * .4f));
      if (bunny)
      {
        float const length = kHeadbandBunnySize * scale;
        CCPoint const middle = base + up * (length * .5f);
        fillEllipse(node, middle, up, length * .5f, length * .2f, color, outline, outlineColor);
        fillEllipse(node, middle + up * (length * .05f), up, length * .36f, length * .09f, premultiplied(m_config.earInnerColor, color.a));
      }
      else
      {
        float const size = kHeadbandEarSize * scale;
        CCPoint const left = perpendicular(up) * (size * .55f);
        std::array<CCPoint, 3> ear = {base - left, base + left, base + up * size};
        node->drawPolygon(ear.data(), 3, color, outline, outlineColor);
        std::array<CCPoint, 3> inner = {base - left * .5f + up * (size * .12f), base + left * .5f + up * (size * .12f), base + up * (size * .7f)};
        node->drawPolygon(inner.data(), 3, premultiplied(m_config.earInnerColor, color.a), 0.f, outlineColor);
      }
    }
    break;
  }
  case HeadbandDeco::None:
    break;
  }
}

// ! --- Flowers --- !

namespace
{
  // A flower at `center`, `up` gives its turn. Sakura: five heart-shaped petals pointing in,
  // daisy: ten thin white petals around a yellow middle
  void drawFlower(CCDrawNode *node, CCPoint const &center, CCPoint const &up, float size, bool sakura, ccColor4F const &color,
                  float outline)
  {
    auto const outlineColor = ccColor4F{0.f, 0.f, 0.f, color.a};
    int const petals = sakura ? 5 : 10;

    for (int pass = outline > 0.f ? 0 : 1; pass < 2; ++pass)
    {
      for (int i = 0; i < petals; ++i)
      {
        CCPoint const dir = rotated(up, 2.f * kPi * static_cast<float>(i) / static_cast<float>(petals));
        if (sakura)
          drawHeart(node, center + dir * (size * .55f), dir, size * .5f + (pass == 0 ? outline : 0.f), pass == 0 ? outlineColor : color);
        else
          fillEllipse(node, center + dir * (size * .55f), dir, size * .5f + (pass == 0 ? outline : 0.f),
                      size * .16f + (pass == 0 ? outline : 0.f), pass == 0 ? outlineColor : color);
      }
    }

    auto const middle = sakura ? shaded(color, .78f) : premultiplied(kDaisyCenter, color.a);
    if (outline > 0.f)
      fillCircle(node, center, size * .3f + outline, outlineColor);
    fillCircle(node, center, size * .3f, middle);
  }
}

void HairNode::drawFlowers(CCDrawNode *node)
{
  if (m_config.flowers == FlowerStyle::None)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const outline = m_config.outline ? kOutlineWidth * scale * .8f : 0.f;

  auto const color = m_config.flowerColorSource == HairColorSource::Hair ? this->hairColor()
                                                                         : this->sourceColor(m_config.flowerColorSource, m_config.flowerColor);
  CCPoint const center = m_frameParams.headCenter;
  CCPoint const across = normalized(m_frameBack, {-m_frameUp.y, m_frameUp.x});

  auto onHead = [&](float degrees, CCPoint &outward)
  {
    CCPoint const radial = normalized(m_frameUp * std::cos(radians(degrees)) + across * std::sin(radians(degrees)), m_frameUp);
    outward = normalized(applyVec(radial, simToNode), {0.f, 1.f});
    return CCPointApplyAffineTransform(center + radial * (this->headEdge(radial) * .98f), simToNode);
  };

  if (m_config.flowers != FlowerStyle::Crown)
  {
    CCPoint outward;
    CCPoint const point = onHead(m_config.flowerPosition, outward);
    drawFlower(node, point, rotated(outward, radians(m_bowWobble * .5f)), kFlowerSize * m_config.flowerSize * scale,
               m_config.flowers == FlowerStyle::Sakura, color, outline);
    return;
  }

  // A crown: small sakuras and daisies with leaves between them, over the top of the head
  float const size = kFlowerSize * .6f * m_config.flowerSize * scale;
  auto const leaf = premultiplied(kLeafColor, color.a);
  for (int i = 0; i < kCrownFlowers; ++i)
  {
    float const t = (static_cast<float>(i) + .5f) / static_cast<float>(kCrownFlowers);
    CCPoint outward;
    CCPoint const point = onHead(kCrownFrom + (-2.f * kCrownFrom) * t, outward);
    CCPoint const leafDir = rotated(outward, radians(i % 2 == 0 ? 60.f : -60.f));
    fillEllipse(node, point + leafDir * (size * .9f), leafDir, size * .55f, size * .22f, leaf, outline, {0.f, 0.f, 0.f, color.a});
  }
  for (int i = 0; i < kCrownFlowers; ++i)
  {
    float const t = (static_cast<float>(i) + .5f) / static_cast<float>(kCrownFlowers);
    CCPoint outward;
    CCPoint const point = onHead(kCrownFrom + (-2.f * kCrownFrom) * t, outward);
    bool const sakura = i % 2 == 0;
    auto const flower = sakura ? color : premultiplied({255, 255, 255}, color.a);
    drawFlower(node, point, rotated(outward, radians(m_bowWobble * .5f + 17.f * static_cast<float>(i))), size, sakura, flower, outline);
  }
}

// ! --- Halo --- !

CCPoint HairNode::haloTarget(CCPoint const &headCenter) const
{
  // Floats over the head in world up, it doesn't spin with the cube
  CCPoint const worldUp = m_gravityDir ? m_gravityDir() * -1.f : CCPoint{0.f, 1.f};
  float const bob = std::sin(m_time * 2.f) * kHaloBob;
  return headCenter + worldUp * ((kHeadRadius + m_config.haloHeight + bob) * m_simScale);
}

void HairNode::updateHalo(float dt, CCPoint const &headCenter)
{
  if (!m_config.halo)
    return;

  // Lags behind the head on a soft spring; snaps back after a teleport
  CCPoint const target = this->haloTarget(headCenter);
  if (target.getDistance(m_haloPosition) > kHeadRadius * 8.f * m_simScale)
  {
    m_haloPosition = target;
    m_haloVelocity = CCPoint{};
    return;
  }
  m_haloVelocity = m_haloVelocity + ((target - m_haloPosition) * kHaloSpring - m_haloVelocity * kHaloDamping) * dt;
  m_haloPosition = m_haloPosition + m_haloVelocity * dt;
}

void HairNode::drawHalo(CCDrawNode *node)
{
  if (!m_config.halo)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const alpha = m_head->getDisplayedOpacity() / 255.f;

  CCPoint const worldUp = m_gravityDir ? m_gravityDir() * -1.f : CCPoint{0.f, 1.f};
  CCPoint const up = normalized(applyVec(worldUp, simToNode), {0.f, 1.f});
  CCPoint const right = perpendicular(up) * -1.f;
  CCPoint const center = CCPointApplyAffineTransform(m_haloPosition, simToNode);
  float const width = kHaloWidth * m_config.haloSize * scale;
  float const height = kHaloHeight * m_config.haloSize * scale;
  float const thickness = kHaloThickness * m_config.haloSize * scale;

  std::array<CCPoint, kHaloPoints + 1> ring;
  for (int i = 0; i <= kHaloPoints; ++i)
  {
    float const angle = 2.f * kPi * static_cast<float>(i) / static_cast<float>(kHaloPoints);
    ring[i] = center + right * (std::cos(angle) * width) + up * (std::sin(angle) * height);
  }

  auto const color = premultiplied(m_config.haloColor, alpha);
  auto ringPass = [&](float radius, ccColor4F const &passColor)
  {
    for (int i = 1; i <= kHaloPoints; ++i)
      node->drawSegment(ring[i - 1], ring[i], radius, passColor);
  };

  // A soft glow, a darker rim instead of the black outline, the ring, a light glint on top
  if (m_config.haloGlow)
    ringPass(thickness * 2.6f, premultiplied(m_config.haloColor, alpha * .25f));
  if (m_config.outline)
    ringPass(thickness + kOutlineWidth * scale * .6f, shaded(color, .55f));
  ringPass(thickness, color);
  for (int i = kHaloPoints / 4 - 3; i <= kHaloPoints / 4 + 3; ++i)
    node->drawSegment(ring[i - 1], ring[i], thickness * .4f, premultiplied({255, 255, 255}, alpha * .8f));
}

// ! --- Face sticker --- !

void HairNode::drawSticker(CCDrawNode *node)
{
  if (m_config.sticker == StickerStyle::None)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const alpha = m_head->getDisplayedOpacity() / 255.f;
  float const outline = m_config.outline ? kOutlineWidth * scale * .8f : 0.f;
  auto const outlineColor = ccColor4F{0.f, 0.f, 0.f, alpha};

  // On the face like the blush, in the icon's own frame
  CCPoint const right = normalized(applyVec(m_iconBack * -1.f, simToNode), {1.f, 0.f});
  CCPoint const up = normalized(applyVec(m_iconUp, simToNode), {0.f, 1.f});
  CCPoint const center = CCPointApplyAffineTransform(m_frameParams.headCenter, simToNode);
  float const side = m_config.stickerRight ? 1.f : -1.f;
  float const size = m_config.stickerSize * scale;
  CCPoint const spot = center + right * (side * m_config.stickerX * scale) + up * (m_config.stickerY * scale);
  auto const color = premultiplied(m_config.stickerColor, alpha);

  switch (m_config.sticker)
  {
  case StickerStyle::BandAid:
  {
    // A tilted capsule with a pad in the middle and little holes on the ends
    CCPoint const axis = rotated(right, radians(side * 20.f));
    CCPoint const a = spot - axis * (size * 2.2f);
    CCPoint const b = spot + axis * (size * 2.2f);
    if (outline > 0.f)
      node->drawSegment(a, b, size * .9f + outline, outlineColor);
    node->drawSegment(a, b, size * .9f, premultiplied(kBandAid, alpha));
    node->drawSegment(spot - axis * (size * .6f), spot + axis * (size * .6f), size * .55f, premultiplied(kBandAidPad, alpha));
    for (float end : {-1.f, 1.f})
    {
      for (float dot : {-.3f, .3f})
        fillCircle(node, spot + axis * (end * size * 1.6f) + perpendicular(axis) * (dot * size), size * .12f, premultiplied(kBandAidPad, alpha));
    }
    break;
  }
  case StickerStyle::Heart:
    if (outline > 0.f)
      drawHeart(node, spot, up, size * 1.5f + outline, outlineColor);
    drawHeart(node, spot, up, size * 1.5f, color);
    break;
  case StickerStyle::Star:
    if (outline > 0.f)
      drawStar(node, spot, up, size * 1.6f + outline * 1.6f, size * .72f + outline, outlineColor);
    drawStar(node, spot, up, size * 1.6f, size * .72f, color);
    break;
  case StickerStyle::Freckles:
    // A few freckles on both cheeks, mirrored
    for (float cheek : {-1.f, 1.f})
    {
      CCPoint const middle = center + right * (cheek * m_config.stickerX * scale) + up * (m_config.stickerY * scale);
      for (auto const &[x, y] : kFreckles)
        fillCircle(node, middle + right * (cheek * x * size * 1.6f) + up * (y * size * 1.6f), size * .28f, premultiplied(kFreckle, alpha * .8f));
    }
    break;
  case StickerStyle::None:
    break;
  }
}

// ! --- Hat --- !

void HairNode::drawHat(CCDrawNode *node)
{
  if (m_config.hat == HatStyle::None)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const outline = m_config.outline ? kOutlineWidth * scale : 0.f;

  auto const color = m_config.hatColorSource == HairColorSource::Hair ? this->hairColor()
                                                                      : this->sourceColor(m_config.hatColorSource, m_config.hatColor);
  auto const outlineColor = ccColor4F{0.f, 0.f, 0.f, color.a};

  // Sits on top of the head in the hair frame, tilted, swaying with the bows, squashed after a landing
  CCPoint const center = m_frameParams.headCenter;
  CCPoint const topSim = center + m_frameUp * (this->headEdge(m_frameUp) - m_config.hatInset * m_simScale);
  CCPoint const base = CCPointApplyAffineTransform(topSim, simToNode);
  CCPoint const frameUp = normalized(applyVec(m_frameUp, simToNode), {0.f, 1.f});
  CCPoint const up = rotated(frameUp, radians(-m_config.hatTilt - m_bowWobble * .3f));
  CCPoint const right = perpendicular(up) * -1.f;
  float const size = m_config.hatSize * scale;
  float const tall = size * (1.f - kHatLandSquash * m_landPop);

  auto shape = [&](std::initializer_list<CCPoint> points, ccColor4F const &fill)
  {
    std::vector<CCPoint> verts(points);
    node->drawPolygon(verts.data(), static_cast<unsigned>(verts.size()), fill, outline, outlineColor);
  };

  switch (m_config.hat)
  {
  case HatStyle::Beret:
  {
    // A flat soft disc leaning over, with a little stem on top
    CCPoint const middle = base + up * (tall * 2.5f) + right * (size * 1.5f);
    fillEllipse(node, middle, rotated(right, radians(-6.f)), size * 11.f, tall * 4.2f, color, outline, outlineColor);
    fillEllipse(node, middle - up * (tall * 2.2f), right, size * 9.f, tall * 1.1f, shaded(color, .85f));
    CCPoint const stem = middle + up * (tall * 4.f);
    if (outline > 0.f)
      fillCircle(node, stem, size * .9f + outline, outlineColor);
    fillCircle(node, stem, size * .9f, shaded(color, .8f));
    break;
  }
  case HatStyle::Beanie:
  {
    // A knitted dome, a folded band and a pom-pom that lags behind on a spring
    std::vector<CCPoint> dome;
    for (int i = 0; i <= 12; ++i)
    {
      float const angle = kPi * static_cast<float>(i) / 12.f;
      dome.push_back(base + up * (tall * .5f) + right * (std::cos(angle) * size * 10.5f) + up * (std::sin(angle) * tall * 9.f));
    }
    node->drawPolygon(dome.data(), static_cast<unsigned>(dome.size()), color, outline, outlineColor);
    for (int i = -2; i <= 2; ++i)
    {
      CCPoint const rib = base + right * (static_cast<float>(i) * size * 3.5f);
      node->drawSegment(rib + up * (tall * 2.f), rib + up * (tall * 7.f), size * .25f, shaded(color, .85f));
    }
    CCPoint const bandLeft = base - right * (size * 10.8f) + up * (tall * .8f);
    CCPoint const bandRight = base + right * (size * 10.8f) + up * (tall * .8f);
    if (outline > 0.f)
      node->drawSegment(bandLeft, bandRight, size * 2.2f + outline, outlineColor);
    node->drawSegment(bandLeft, bandRight, size * 2.2f, shaded(color, .82f));
    CCPoint const pom = base + rotated(up, radians(m_bowWobble * 1.5f)) * (tall * 9.5f + size * 2.f);
    if (outline > 0.f)
      fillCircle(node, pom, size * 2.6f + outline, outlineColor);
    fillCircle(node, pom, size * 2.6f, mixedWhite(color, .55f));
    break;
  }
  case HatStyle::WitchHat:
  {
    // A wide brim, a cone whose tip flops over on a spring, a band
    fillEllipse(node, base + up * (tall * .8f), right, size * 14.f, tall * 3.f, color, outline, outlineColor);
    CCPoint const middle = base + up * (tall * 9.f);
    float const bend = radians(25.f + m_bowWobble * 1.5f);
    CCPoint const tip = middle + rotated(up, -bend) * (tall * 8.f);
    shape({base - right * (size * 6.5f) + up * tall, base + right * (size * 6.5f) + up * tall, middle + right * (size * 2.9f),
           middle - right * (size * 2.9f)},
          color);
    shape({middle - right * (size * 2.9f), middle + right * (size * 2.9f), tip}, color);
    CCPoint const bandLeft = base - right * (size * 5.8f) + up * (tall * 2.6f);
    CCPoint const bandRight = base + right * (size * 5.8f) + up * (tall * 2.6f);
    node->drawSegment(bandLeft, bandRight, size * 1.3f, shaded(color, .55f));
    break;
  }
  case HatStyle::None:
    break;
  }
}

