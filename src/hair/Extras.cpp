#include "HairNode.hpp"
#include "HairShared.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

using namespace geode::prelude;
using namespace hair;

// ! --- Extras --- !
// Ponytail / twin tails, ahoge, bows, ears and the scarf. They are locks of the same simulation as
// the hairstyle, built in the same head frame, so they spin with the icon or stay upright with it.

namespace
{
  // A tail is a bundle: a thick main lock and thinner ones from the same tie, slightly fanned out
  struct BundleLock
  {
    float angle;  // degrees from the main lock
    float length; // relative to the tail length
    float width;  // relative to the tail thickness
    float depth;  // shading, the main lock is the lightest
  };

  constexpr std::array kBundle{
      BundleLock{-9.f, .9f, .6f, .55f},
      BundleLock{15.f, .78f, .45f, .4f},
      BundleLock{8.f, .86f, .55f, .7f},
      BundleLock{0.f, 1.f, 1.f, 1.f},
  };

  constexpr float kTailRootDepth = .93f; // part of the way to the edge of the head where the tie sits
  constexpr float kTailDrop = 1.5f;      // how fast a tail turns from sticking out to hanging down
  constexpr float kTailStiffness = 1.1f;
  constexpr float kTailStiffnessPower = 1.6f;
  constexpr float kTieSize = .55f; // scrunchie radius relative to the tail thickness

  // Braids: overlapping ovals along the lock, tilted left and right in turn
  constexpr float kBraidStep = .55f;   // oval spacing relative to the braid width
  constexpr float kBraidTilt = 28.f;   // degrees
  constexpr float kBraidShift = .12f;  // sideways offset relative to the width
  constexpr float kBraidLong = .62f;   // oval radii relative to the width
  constexpr float kBraidShort = .36f;
  constexpr float kBraidTaper = .4f;   // how much narrower the end is
  constexpr float kBraidEnd = .85f;    // where the braid is tied, the rest is a loose tuft
  constexpr int kBraidOvalPoints = 14;

  constexpr float kAhogeRootDepth = .9f;
  constexpr float kAhogeSpread = 10.f; // degrees between two ahoge strands
  constexpr float kAhogeCurl = 230.f;  // degrees a fully curled ahoge turns over its length
  constexpr float kAhogeWidth = .8f;   // relative to the lock width setting
  constexpr float kAhogeStiffness = 1.4f;
  constexpr float kAhogeStiffnessPower = .7f;

  constexpr float kBowSize = 5.5f; // icon units, half the width of a bow at size 1
  constexpr float kBowRootDepth = .98f;
  constexpr float kRibbonWidth = 2.2f; // icon units at bow size 1
  constexpr float kRibbonSpread = .5f; // how much the two ribbon tails part
  constexpr float kRibbonStiffness = .5f;
  constexpr float kBowWobbleMax = 25.f; // degrees
  constexpr float kBowAccel = 3000.f;   // units / s^2 of head acceleration for a full swing
  constexpr float kBowSpring = 160.f;   // 1 / s^2
  constexpr float kBowDamping = 9.f;    // 1 / s

  // Ears, sizes in icon units at ear size 1
  struct EarShape
  {
    float length;
    float width;
    float stiffness;
    float stiffnessPower;
    float flare; // degrees they lean out
  };
  constexpr EarShape kCatEar{9.f, 8.f, 4.f, .3f, 12.f};
  constexpr EarShape kFoxEar{11.f, 9.f, 3.5f, .4f, 16.f};
  constexpr EarShape kBunnyEar{18.f, 5.5f, 1.4f, 1.f, 6.f};
  constexpr float kEarRootDepth = .9f;
  constexpr float kEarFold = 22.f;        // degrees the ears fold back in a full air flow
  constexpr float kEarKick = 260.f;       // degrees / s of a twitch
  constexpr float kEarSpring = 500.f;     // 1 / s^2
  constexpr float kEarDamping = 18.f;     // 1 / s
  constexpr float kEarInner = .5f;        // inner ear width relative to the ear
  constexpr float kFoxTip = .72f;         // where the light tip of a fox ear starts

  // Sleepy: the ahoge droops and the ears fold
  constexpr float kSleepyDroop = 70.f;    // degrees the tip of the ahoge sinks
  constexpr float kSleepyEarFold = 35.f;  // degrees the ears fold out

  // Scarf, in icon units
  constexpr float kScarfOverhang = 1.f;   // the band sticks out of the sides of the head
  constexpr float kScarfSag = .8f;        // the band dips in the middle
  constexpr int kScarfPoints = 7;
  constexpr float kScarfEndWidth = .8f;   // relative to the band
  constexpr float kScarfStiffness = .45f;

  EarShape const &earShape(EarStyle style)
  {
    switch (style)
    {
    case EarStyle::Fox:
      return kFoxEar;
    case EarStyle::Bunny:
      return kBunnyEar;
    default:
      return kCatEar;
    }
  }

  CCPoint fromUp(CCPoint const &up, CCPoint const &across, float degrees)
  {
    float const angle = radians(degrees);
    return normalized(up * std::cos(angle) + across * std::sin(angle), up);
  }
}

// ! --- Locks --- !

bool HairNode::extrasActive() const
{
  return m_config.tails != TailStyle::None || m_config.ahoge > 0 || m_config.headBow || m_config.scarf ||
         m_config.ears != EarStyle::None;
}

void HairNode::addExtraLocks(std::mt19937 &rng)
{
  std::uniform_real_distribution<float> random(0.f, 1.f);

  int const tails = m_config.tails == TailStyle::TwinTails ? 2 : m_config.tails == TailStyle::Ponytail ? 1 : 0;
  for (int group = 0; group < tails; ++group)
  {
    for (auto const &bundle : kBundle)
    {
      // A braid is a single lock, the bundle around it is only for loose tails
      if (m_config.braidTails && bundle.angle != 0.f)
        continue;

      Lock lock;
      lock.kind = LockKind::Tail;
      lock.group = group;
      lock.side = bundle.angle;
      lock.angle = 0.f;
      lock.length = m_config.tailLength * bundle.length * (.95f + .1f * random(rng));
      lock.width = m_config.tailThickness * bundle.width;
      lock.curl = 0.f;
      lock.depth = bundle.depth;
      m_locks.push_back(lock);
    }
  }

  m_ahogeStart = m_locks.size();
  for (int i = 0; i < m_config.ahoge; ++i)
  {
    Lock lock;
    lock.kind = LockKind::Ahoge;
    lock.side = m_config.ahoge == 1 ? 0.f : (i == 0 ? -1.f : 1.f);
    lock.angle = 0.f;
    lock.length = m_config.ahogeLength * (i == 0 ? 1.f : .85f);
    lock.width = m_config.lockWidth * kAhogeWidth;
    lock.curl = 0.f;
    lock.depth = 1.f;
    m_locks.push_back(lock);
  }
}

void HairNode::addEarAndScarfLocks()
{
  m_earsStart = m_locks.size();
  if (m_config.ears != EarStyle::None)
  {
    auto const &shape = earShape(m_config.ears);
    for (float side : {1.f, -1.f})
    {
      Lock lock;
      lock.kind = LockKind::Ear;
      lock.side = side;
      lock.group = side > 0.f ? 0 : 1;
      lock.angle = 0.f;
      lock.length = shape.length * m_config.earSize;
      lock.width = shape.width * m_config.earSize;
      lock.curl = 0.f;
      lock.depth = 1.f;
      m_locks.push_back(lock);
    }
  }

  m_scarfStart = m_locks.size();
  if (m_config.scarf && m_config.scarfLength > 0.f)
  {
    for (int end = 0; end < 2; ++end)
    {
      Lock lock;
      lock.kind = LockKind::ScarfEnd;
      lock.group = end;
      lock.angle = 0.f;
      lock.length = m_config.scarfLength * (end == 0 ? 1.f : .8f);
      lock.width = m_config.scarfWidth * kScarfEndWidth;
      lock.curl = 0.f;
      lock.depth = end == 0 ? 1.f : .8f;
      m_locks.push_back(lock);
    }
  }
}

CCPoint HairNode::scarfKnot(CCPoint const &headCenter) const
{
  // Tied at the back of the neck, in the icon's own frame like the band
  float const half = kHeadRadius * m_simScale;
  float const height = -half + m_config.scarfHeight * m_simScale;
  return headCenter + m_iconUp * height + m_iconBack * (half + kScarfOverhang * m_simScale * .5f);
}

void HairNode::updateEars(float dt)
{
  if (m_config.ears == EarStyle::None || m_needsReset || dt <= 0.f)
  {
    m_earTwitch = {0.f, 0.f};
    m_earTwitchSpeed = {0.f, 0.f};
    return;
  }

  std::uniform_real_distribution<float> random(0.f, 1.f);
  for (int i = 0; i < 2; ++i)
  {
    // Now and then a quick flick that springs back
    m_earTimer[i] -= dt;
    if (m_earTimer[i] <= 0.f)
    {
      // A sleepy head doesn't twitch
      if (m_config.earTwitch > 0.f && m_sleepy < .5f)
        m_earTwitchSpeed[i] += (random(m_random) < .5f ? -1.f : 1.f) * kEarKick;
      m_earTimer[i] = (1.5f + 3.5f * random(m_random)) / (.3f + m_config.earTwitch * 1.4f);
    }

    m_earTwitchSpeed[i] += (-m_earTwitch[i] * kEarSpring - m_earTwitchSpeed[i] * kEarDamping) * dt;
    m_earTwitch[i] += m_earTwitchSpeed[i] * dt;
  }
}

void HairNode::addRibbonLocks()
{
  if (m_config.bowRibbonLength <= 0.f)
    return;

  for (int group = 0; group < 3; ++group)
  {
    if (!this->bowExists(group))
      continue;

    for (float side : {-1.f, 1.f})
    {
      Lock lock;
      lock.kind = LockKind::Ribbon;
      lock.group = group;
      lock.side = side;
      lock.angle = 0.f;
      lock.length = m_config.bowRibbonLength * (side > 0.f ? 1.f : .85f);
      lock.width = kRibbonWidth * m_config.bowSize;
      lock.curl = 0.f;
      lock.depth = .85f;
      m_locks.push_back(lock);
    }
  }
}

// ! --- Shape --- !

float HairNode::headEdge(CCPoint const &dir) const
{
  float const half = kHeadRadius * m_simScale;
  if (!m_isBox)
    return half;

  // The edge of the square along `dir`
  float const x = std::abs(dir.dot(m_headAxisX));
  float const y = std::abs(dir.dot(m_headAxisY));
  float const edge = std::max(x, y);
  return edge > .0001f ? half / edge : half;
}

CCPoint HairNode::tailRoot(int group, CCPoint const &headCenter, CCPoint const &up, CCPoint const &across, CCPoint *outward) const
{
  // The ponytail is tied on the back of the head, the second twin tail mirrored in front
  float const angle = group == 1 ? -m_config.tailPosition : m_config.tailPosition;
  CCPoint const radial = fromUp(up, across, angle);
  if (outward)
    *outward = radial;
  return headCenter + radial * (this->headEdge(radial) * kTailRootDepth);
}

bool HairNode::bowExists(int group) const
{
  if (group == 0)
    return m_config.headBow;
  if (m_config.tie != TieStyle::Bow)
    return false;
  if (group == 1)
    return m_config.tails != TailStyle::None;
  return m_config.tails == TailStyle::TwinTails;
}

CCPoint HairNode::bowAnchor(int group, CCPoint const &headCenter, CCPoint const &up, CCPoint const &across, CCPoint *outward) const
{
  if (group > 0)
    return this->tailRoot(group - 1, headCenter, up, across, outward);

  CCPoint const radial = fromUp(up, across, m_config.bowPosition);
  if (outward)
    *outward = radial;
  return headCenter + radial * (this->headEdge(radial) * kBowRootDepth);
}

void HairNode::buildExtraTarget(Lock const &lock, HairStrandTarget &target, CCPoint const &headCenter, CCPoint const &up,
                                CCPoint const &across)
{
  int const segments = m_config.segments;
  CCPoint const down = up * -1.f;
  target.segmentLength = lock.length * m_simScale / static_cast<float>(segments);

  float stiffness = 0.f;
  if (lock.kind == LockKind::Tail)
  {
    // Sticks out of the tie a little, then hangs down; the bundle fans out a bit
    CCPoint radial;
    target.root = this->tailRoot(lock.group, headCenter, up, across, &radial);
    float const mirror = lock.group == 1 ? -1.f : 1.f;
    for (int k = 0; k < segments; ++k)
    {
      float const t = std::min(1.f, static_cast<float>(k + 1) / static_cast<float>(segments) * kTailDrop);
      CCPoint const dir = normalized(radial * (1.f - t) + down * t, down);
      target.restDirs[k] = rotated(dir, radians(lock.side * mirror));
    }
    target.collide = true;
    target.stiffnessPower = kTailStiffnessPower;
    stiffness = kTailStiffness;
  }
  else if (lock.kind == LockKind::Ahoge)
  {
    // Rises from the top and curls over, one strand towards the front, two to both sides
    CCPoint const radial = fromUp(up, across, lock.side * kAhogeSpread);
    target.root = headCenter + radial * (this->headEdge(radial) * kAhogeRootDepth);
    CCPoint const curlSide = lock.side > 0.f ? across : across * -1.f;
    for (int k = 0; k < segments; ++k)
    {
      float const t = static_cast<float>(k + 1) / static_cast<float>(segments);
      float const angle = radians(kAhogeCurl * m_config.ahogeCurl * std::pow(t, 1.5f) + kSleepyDroop * m_sleepy * t);
      target.restDirs[k] = normalized(radial * std::cos(angle) + curlSide * std::sin(angle), radial);
    }
    target.collide = false;
    target.stiffnessPower = kAhogeStiffnessPower;
    stiffness = kAhogeStiffness;
  }
  else if (lock.kind == LockKind::Ear)
  {
    // Stands on the head, leans out a bit, twitches now and then and folds back in the wind
    auto const &shape = earShape(m_config.ears);
    float const place = lock.side * m_config.earSpread;
    CCPoint const radial = fromUp(up, across, place);
    target.root = headCenter + radial * (this->headEdge(radial) * kEarRootDepth);

    float const twitch = m_earTwitch[lock.group];
    CCPoint const dir = fromUp(up, across, place + lock.side * (shape.flare + twitch + kSleepyEarFold * m_sleepy) + kEarFold * m_motion);
    for (int k = 0; k < segments; ++k)
      target.restDirs[k] = dir;
    target.collide = false;
    target.stiffnessPower = shape.stiffnessPower;
    stiffness = shape.stiffness;
  }
  else if (lock.kind == LockKind::ScarfEnd)
  {
    // Two ends hanging from the knot, the air flow blows them back
    target.root = this->scarfKnot(headCenter);
    CCPoint const back = m_iconBack;
    CCPoint const dir = lock.group == 0 ? normalized(back * .75f + down * .65f, down) : normalized(back * .4f + down * .9f, down);
    for (int k = 0; k < segments; ++k)
      target.restDirs[k] = dir;
    target.collide = false;
    target.stiffnessPower = 1.f;
    stiffness = kScarfStiffness;
  }
  else
  {
    // Ribbon tails hang from the knot of a bow, parting to both sides
    CCPoint radial;
    target.root = this->bowAnchor(lock.group, headCenter, up, across, &radial);
    CCPoint const dir = normalized(down + perpendicular(radial) * (lock.side * kRibbonSpread), down);
    for (int k = 0; k < segments; ++k)
      target.restDirs[k] = dir;
    target.collide = false;
    target.stiffnessPower = 1.f;
    stiffness = kRibbonStiffness;
  }

  target.stiffness = stiffness * kSpringScale * kBaseGravity * m_simScale / target.segmentLength;
}

float HairNode::lockWidthAt(Lock const &lock, float along) const
{
  switch (lock.kind)
  {
  case LockKind::Tail:
  {
    // Gathered at the tie, full in the middle, pointed at the end
    float const bulge = std::sin(kPi * std::min(along * 1.25f, 1.f));
    return (.75f + .45f * bulge) * (1.f - .95f * along * along);
  }
  case LockKind::Ribbon:
  case LockKind::ScarfEnd:
    return 1.f - .3f * along;
  case LockKind::Ear:
    if (m_config.ears == EarStyle::Bunny)
      return (.85f + .3f * std::sin(kPi * along)) * (1.f - std::pow(along, 4.f));
    return 1.f - .95f * std::pow(along, 1.1f);
  default:
    return 1.f - kTipTaper * along;
  }
}

// ! --- Bow wobble --- !

void HairNode::updateBow(float dt)
{
  bool const anyBow = this->bowExists(0) || this->bowExists(1) || this->bowExists(2);
  if (!anyBow || dt <= 0.f || m_needsReset)
  {
    m_bowWobble = 0.f;
    m_bowWobbleSpeed = 0.f;
    return;
  }

  // The loops lag behind when the head speeds up or slows down, then swing back
  CCPoint const accel = (m_headVelocity - m_lastHeadVelocity) / dt;
  float const target = std::clamp(-accel.x / (kBowAccel * m_simScale), -1.f, 1.f) * kBowWobbleMax;
  m_bowWobbleSpeed += ((target - m_bowWobble) * kBowSpring - m_bowWobbleSpeed * kBowDamping) * dt;
  m_bowWobble = std::clamp(m_bowWobble + m_bowWobbleSpeed * dt, -2.f * kBowWobbleMax, 2.f * kBowWobbleMax);
}

// ! --- Drawing --- !

void HairNode::drawTies(CCDrawNode *node)
{
  if (m_config.tails == TailStyle::None || m_config.tie != TieStyle::Scrunchie)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const outline = m_config.outline ? kOutlineWidth * scale : 0.f;
  float const radius = m_config.tailThickness * kTieSize * scale;

  auto const color = m_config.tieColorSource == HairColorSource::Hair ? this->hairColor()
                                                                      : this->sourceColor(m_config.tieColorSource, m_config.tieColor);
  auto const outlineColor = ccColor4F{0.f, 0.f, 0.f, color.a};
  auto const &strands = m_sim.strands();

  // A soft ring at the root of the main lock of every tail
  for (size_t s = m_tailsStart; s < m_ahogeStart && s < strands.size(); ++s)
  {
    if (m_locks[s].side != 0.f)
      continue;

    CCPoint const center = CCPointApplyAffineTransform(strands[s][0], simToNode);
    if (outline > 0.f)
      fillCircle(node, center, radius + outline, outlineColor);
    fillCircle(node, center, radius, color);
    fillCircle(node, center, radius * .45f, shaded(color, .8f));
  }
}

void HairNode::drawBows(CCDrawNode *node)
{
  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const outline = m_config.outline ? kOutlineWidth * scale : 0.f;
  float const size = kBowSize * m_config.bowSize * scale;
  CCPoint const across = normalized(m_frameBack, {-m_frameUp.y, m_frameUp.x});

  for (int group = 0; group < 3; ++group)
  {
    if (!this->bowExists(group))
      continue;

    auto const source = group == 0 ? m_config.bowColorSource : m_config.tieColorSource;
    auto const custom = group == 0 ? m_config.bowColor : m_config.tieColor;
    auto const color = source == HairColorSource::Hair ? this->hairColor() : this->sourceColor(source, custom);

    CCPoint outwardSim;
    CCPoint const anchor = this->bowAnchor(group, m_frameParams.headCenter, m_frameUp, across, &outwardSim);
    CCPoint const knot = CCPointApplyAffineTransform(anchor, simToNode);
    CCPoint const outward = normalized(applyVec(outwardSim, simToNode), {0.f, 1.f});
    drawBowShape(node, knot, outward, m_bowWobble, size, color, outline);
  }
}

void HairNode::drawEarInners(CCDrawNode *node)
{
  if (m_config.ears == EarStyle::None || m_earsStart >= m_scarfStart)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const alpha = m_head->getDisplayedOpacity() / 255.f;
  auto const inner = premultiplied(m_config.earInnerColor, alpha);
  auto const tip = premultiplied({255, 255, 255}, alpha);
  auto const &strands = m_sim.strands();

  // A narrower soft-colored copy inside every ear, a light tip on fox ears
  for (size_t s = m_earsStart; s < m_scarfStart && s < strands.size(); ++s)
  {
    auto const &points = strands[s];
    float const rootRadius = m_locks[s].width * .5f * scale;
    int const last = static_cast<int>(points.size()) - 1;
    for (int k = 1; k <= last; ++k)
    {
      float const along = static_cast<float>(k) / static_cast<float>(last);
      CCPoint const a = CCPointApplyAffineTransform(points[k - 1], simToNode);
      CCPoint const b = CCPointApplyAffineTransform(points[k], simToNode);
      float const radius = rootRadius * this->lockWidthAt(m_locks[s], along);

      if (m_config.ears == EarStyle::Fox && along > kFoxTip)
        node->drawSegment(a, b, radius * .9f, tip);
      else if (along > .12f && along < .8f)
        node->drawSegment(a, b, radius * kEarInner, inner);
    }
  }
}

void HairNode::drawScarfBand(CCDrawNode *node)
{
  if (!m_config.scarf)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const outline = m_config.outline ? kOutlineWidth * scale : 0.f;

  auto const color = m_config.scarfColorSource == HairColorSource::Hair ? this->hairColor()
                                                                        : this->sourceColor(m_config.scarfColorSource, m_config.scarfColor);
  auto const outlineColor = ccColor4F{0.f, 0.f, 0.f, color.a};

  // A band across the bottom of the face in the icon frame, dipping a little in the middle
  float const unit = m_simScale;
  float const half = kHeadRadius * unit;
  CCPoint const center = m_frameParams.headCenter;
  CCPoint const right = m_iconBack * -1.f;
  float const reach = half + kScarfOverhang * unit;
  float const height = -half + m_config.scarfHeight * unit;

  std::array<CCPoint, kScarfPoints> band;
  for (int i = 0; i < kScarfPoints; ++i)
  {
    float const x = static_cast<float>(i) / static_cast<float>(kScarfPoints - 1) * 2.f - 1.f;
    float const sag = kScarfSag * unit * (1.f - x * x);
    band[i] = CCPointApplyAffineTransform(center + right * (x * reach) + m_iconUp * (height - sag), simToNode);
  }

  float const radius = m_config.scarfWidth * .5f * scale;
  CCPoint const upNode = normalized(applyVec(m_iconUp, simToNode), {0.f, 1.f});
  for (int pass = outline > 0.f ? 0 : 1; pass < 2; ++pass)
  {
    for (int i = 1; i < kScarfPoints; ++i)
    {
      if (pass == 0)
        node->drawSegment(band[i - 1], band[i], radius + outline, outlineColor);
      else
        node->drawSegment(band[i - 1], band[i], radius, color);
    }
  }

  // A fold along the band and the knot on the back side
  for (int i = 1; i < kScarfPoints; ++i)
    node->drawSegment(band[i - 1] + upNode * (radius * .3f), band[i] + upNode * (radius * .3f), radius * .12f, shaded(color, .78f));

  CCPoint const knot = CCPointApplyAffineTransform(this->scarfKnot(center), simToNode);
  if (outline > 0.f)
    fillCircle(node, knot, radius * .8f + outline, outlineColor);
  fillCircle(node, knot, radius * .8f, shaded(color, .9f));
}

// ! --- Braids --- !

void HairNode::drawBraids(CCDrawNode *node, size_t from, size_t to)
{
  auto const &strands = m_sim.strands();
  to = std::min(to, strands.size());
  if (from >= to)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const outline = m_config.outline ? kOutlineWidth * scale : 0.f;

  auto const tieColor = m_config.tieColorSource == HairColorSource::Hair ? this->hairColor()
                                                                         : this->sourceColor(m_config.tieColorSource, m_config.tieColor);

  for (size_t s = from; s < to; ++s)
  {
    auto const &lock = m_locks[s];
    auto const base = shaded(this->lockColor(lock), kBackShade + (1.f - kBackShade) * lock.depth);
    auto const outlineColor = ccColor4F{0.f, 0.f, 0.f, base.a};

    buildCurve(strands[s], simToNode, 3, m_curve);
    if (m_curve.size() < 2)
      continue;

    // Arc length along the smoothed curve, to place the ovals evenly
    std::vector<float> lengths(m_curve.size(), 0.f);
    for (size_t i = 1; i < m_curve.size(); ++i)
      lengths[i] = lengths[i - 1] + m_curve[i].getDistance(m_curve[i - 1]);
    float const total = lengths.back();
    if (total <= .0001f)
      continue;

    auto pointAt = [&](float distance, CCPoint &tangent)
    {
      size_t i = 1;
      while (i + 1 < m_curve.size() && lengths[i] < distance)
        ++i;
      float const span = std::max(lengths[i] - lengths[i - 1], .0001f);
      float const t = std::clamp((distance - lengths[i - 1]) / span, 0.f, 1.f);
      tangent = normalized(m_curve[i] - m_curve[i - 1], {0.f, -1.f});
      return m_curve[i - 1] + (m_curve[i] - m_curve[i - 1]) * t;
    };

    float const width = lock.width * scale;
    float const end = total * kBraidEnd;
    float const step = std::max(width * kBraidStep, .5f);

    // From the root to the tie, every oval drawn whole over the previous one: the outlines that
    // show through make it read as plaited
    int index = 0;
    for (float distance = step * .5f; distance < end; distance += step, ++index)
    {
      float const along = distance / total;
      float const taper = 1.f - kBraidTaper * along;
      float const side = index % 2 == 0 ? 1.f : -1.f;

      CCPoint tangent;
      CCPoint const point = pointAt(distance, tangent);
      CCPoint const axis = rotated(tangent, radians(side * kBraidTilt));
      CCPoint const minor = perpendicular(axis);
      CCPoint const center = point + perpendicular(tangent) * (side * width * kBraidShift * taper);

      auto oval = [&](float grow, ccColor4F const &color)
      {
        std::array<CCPoint, kBraidOvalPoints> points;
        for (int i = 0; i < kBraidOvalPoints; ++i)
        {
          float const angle = 2.f * kPi * static_cast<float>(i) / static_cast<float>(kBraidOvalPoints);
          points[i] = center + axis * (std::cos(angle) * (width * kBraidLong * taper + grow)) +
                      minor * (std::sin(angle) * (width * kBraidShort * taper + grow));
        }
        node->drawPolygon(points.data(), kBraidOvalPoints, color, 0.f, color);
      };

      if (outline > 0.f)
        oval(outline, outlineColor);
      oval(0.f, this->lockColorAt(lock, shaded(base, side > 0.f ? 1.f : .88f), along));
    }

    // Tied off, then a short loose tuft
    CCPoint tangent;
    CCPoint const tie = pointAt(end, tangent);
    float const tieRadius = width * .3f * (1.f - kBraidTaper * kBraidEnd);
    float const tuftRadius = width * .28f * (1.f - kBraidTaper);
    size_t first = 0;
    while (first + 1 < m_curve.size() && lengths[first] < end)
      ++first;
    for (int pass = outline > 0.f ? 0 : 1; pass < 2; ++pass)
    {
      CCPoint previous = tie;
      for (size_t i = first; i < m_curve.size(); ++i)
      {
        float const along = lengths[i] / total;
        float const t = (along - kBraidEnd) / (1.f - kBraidEnd);
        float const radius = tuftRadius * (1.f - .85f * t);
        if (pass == 0)
          node->drawSegment(previous, m_curve[i], radius + outline, outlineColor);
        else
          node->drawSegment(previous, m_curve[i], radius, this->lockColorAt(lock, base, along));
        previous = m_curve[i];
      }
    }
    if (outline > 0.f)
      fillCircle(node, tie, tieRadius + outline, outlineColor);
    fillCircle(node, tie, tieRadius, tieColor);
  }
}

