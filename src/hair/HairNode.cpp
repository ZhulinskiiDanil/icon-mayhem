#include "HairNode.hpp"
#include "../presets/Looks.hpp"
#include "HairShared.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

using namespace geode::prelude;
using namespace hair;

// ! --- Constants --- !

namespace
{
  constexpr float kFocusIn = 7.f;   // 1 / s, focus comes in this fast
  constexpr float kFocusOut = 1.f;  // 1 / s, and goes back this slowly
  constexpr float kFocusHold = 1.5f; // s of calm before it goes back
  constexpr float kTeleportDistance = 160.f;
  constexpr float kRootDepth = .72f;      // roots sit inside the head so the icon hides them
  constexpr float kColliderScale = 1.1f;  // round heads: hair rests a bit above them
  constexpr float kBoxScale = 1.32f;      // cube heads: hair rests this far out on the faces...
  constexpr float kBoxExponent = 3.5f;    // ...and goes around the corners in a wide arc (squircle, 2 is a circle)
  constexpr float kPressedScale = 1.1f;   // the windward side while moving: hair lies right on the face
  constexpr float kPressedExponent = 8.f;
  constexpr float kPressedRoundScale = 1.03f;
  // Base under the locks: barely peeks out of the head so it never shows as a cushion
  // when the locks move away, and is much rounder than the collider
  constexpr float kCapScale = 1.05f;   // half size of the base relative to the head
  constexpr float kCapExponent = 3.f;  // squircle exponent of the base on cubes, 2 is a circle
  constexpr int kCapPoints = 24;
  constexpr float kCapEdgeFade = .15f;    // part of the base arc on each end that sinks into the head
  constexpr float kTurnSpeed = 12.f;      // radians / s the hairstyle rotates after a gravity flip
  constexpr float kFacingSpeed = 6.f;     // facing units / s when turning around
  constexpr float kWindStrength = 120.f;  // units / s^2, a light breeze in previews

  // Combing of the hair glued to a spinning icon, see buildTargets()
  constexpr float kWindwardComb = .7f;     // locks facing the movement turn back right at the root
  constexpr float kCombAlong = .6f;        // every lock turns back along its length
  constexpr float kDrapeAlong = .9f;       // standing still every lock falls down along its length
  constexpr float kDrapeBackBias = .6f;    // moves the parting forward, 0 parts the hair right on top
  constexpr float kCombDown = .35f;        // combed hair points back and a bit down
  constexpr float kCurlReference = 9.f;    // style curl that gives kCombAlong as is
  constexpr float kFullCombSpeed = 250.f;  // units / s of horizontal speed that comb the hair fully
  constexpr float kMotionResponse = 4.f;   // how fast the combing follows the speed, 1 / s

  // Gusty wind, see HairNode::updateGust()
  constexpr float kGustFrequency = .7f;    // noise cells / s at gust speed 1
  constexpr float kFlutterAccel = 1400.f;  // units / s^2 at flutter 1 and full air flow

  // Calm jumps, see HairSimParams::calm
  constexpr float kCalmStrength = .5f;     // per 1/60 s at full spin
  constexpr float kFullCalmSpin = 6.f;     // radians / s, a cube jump turns about 7.5
  constexpr float kCalmFade = .4f;         // s, stays on a bit after the spin so the landing doesn't shake

  // Hair in front of the face, see buildFrontTarget()
  constexpr float kFaceLockRootX = .78f;      // across the face, relative to the head half size
  constexpr float kFaceLockRootY = .9f;       // up from the center, same units
  constexpr float kFaceLockFlare = 22.f;      // degrees the locks start out away from the face
  constexpr float kFaceLockWaves = 1.5f;      // waves along a lock
  constexpr float kFaceLockWaveAmp = 16.f;    // degrees
  constexpr float kFaceLockStiffness = .6f;
  constexpr float kBangRootY = 1.f;           // bangs grow right at the hairline
  constexpr float kBangSpread = .8f;          // part of the face width the bangs cover
  constexpr float kBangFan = 16.f;            // degrees the side bangs turn out
  constexpr float kBangWidth = 1.5f;          // wide locks overlap into one fringe
  constexpr float kBangStiffness = 1.6f;      // short and neat, they keep their shape
  constexpr float kBangPartGap = .14f;        // parted bangs: the bare part in the middle, of the face half
  constexpr float kBangMinLength = .25f;      // one side bangs: the shortest lock towards the bare side

  float smoothstep(float from, float to, float x)
  {
    float const t = std::clamp((x - from) / std::max(to - from, .0001f), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
  }
  constexpr float kArcSharpExponent = 8.f;    // arc softness 0: flat middle, only the ends drop
  constexpr float kArcSoftExponent = 1.5f;    // arc softness 1: one smooth arc
  constexpr float kMaxArcTurn = 45.f;         // degrees the bangs on a steep arc turn out at most
  constexpr float kFrontStiffnessPower = 1.f; // front locks keep their shape further down

  // Hitbox helpers of the customizer, see drawDebug()
  constexpr ccColor4F kDebugCollider = {0.f, 1.f, 1.f, 1.f};
  constexpr ccColor4F kDebugFloor = {1.f, .55f, 0.f, 1.f};
  constexpr ccColor4F kDebugRoots = {1.f, 1.f, 0.f, 1.f};
  constexpr ccColor4F kDebugBangs = {1.f, .3f, 1.f, 1.f};
  constexpr ccColor4F kDebugFaceLocks = {.3f, 1.f, .3f, 1.f};
  constexpr float kDebugLine = .35f; // icon units
  constexpr float kDebugDot = .9f;
  constexpr int kDebugColliderPoints = 64;
  constexpr int kDebugArcPoints = 24;

  constexpr int kCurveSubdiv = 3;      // drawn points per simulated segment
  constexpr int kCurveSubdivDense = 2; // fewer for dense hair, the thin locks don't need it
  constexpr int kDenseLockCount = 64;

  // ! --- Styles --- !

  struct StyleShape
  {
    float frontAngle; // degrees from "up", negative is the front
    float backAngle;
    float curl; // degrees per segment
    float stiffnessPower;
    float stiffness;
    float width;
    float lengthJitter;
    float frontLength; // length multipliers along the head
    float topLength;
    float backLength;
  };

  StyleShape const &styleShape(HairStyle style)
  {
    static constexpr StyleShape flowing{-60.f, 118.f, 9.f, 2.f, 1.f, 1.f, .15f, .45f, .75f, 1.f};
    static constexpr StyleShape longHair{-72.f, 140.f, 10.f, 2.6f, .8f, .9f, .1f, .6f, .9f, 1.f};
    static constexpr StyleShape spiky{-55.f, 95.f, 5.f, 1.f, 1.8f, 1.5f, .35f, .6f, .8f, .75f};

    switch (style)
    {
    case HairStyle::Long:
      return longHair;
    case HairStyle::Spiky:
      return spiky;
    case HairStyle::Flowing:
      break;
    }
    return flowing;
  }

  // ! --- Math --- !

  // Signed angle from `from` to `to`. Nearly opposite directions turn through `via`
  // so the hair goes over the top of the head instead of through it
  float turnAngle(CCPoint const &from, CCPoint const &to, CCPoint const &via)
  {
    float delta = std::atan2(from.cross(to), from.dot(to));
    if (std::abs(delta) > kPi * .75f && (from.cross(via) > 0.f) != (delta > 0.f))
      delta += delta > 0.f ? -2.f * kPi : 2.f * kPi;
    return delta;
  }

}

// ! --- Creation --- !

HairNode *HairNode::attach(CCSprite *head, CCNode *behind, CCSprite *primary, CCSprite *secondary, CCNode *simSpace)
{
  if (!head || !behind || !simSpace)
    return nullptr;

  // A draw node can't live inside a batch node (robot and spider parts are batched),
  // so step out of every batch node and go behind the outermost one
  for (auto node = behind->getParent(); node; node = node->getParent())
  {
    if (typeinfo_cast<CCSpriteBatchNode *>(node))
      behind = node;
  }

  auto parent = behind->getParent();
  if (!parent)
  {
    log::warn("Can't attach hair: the node it should go behind has no parent");
    return nullptr;
  }

  auto node = new HairNode();
  if (!node->init(head, primary ? primary : head, secondary, simSpace))
  {
    delete node;
    return nullptr;
  }
  node->autorelease();

  parent->addChild(node, behind->getZOrder() - 1);

  // The eyes sit on the icon under the front locks: same z, added first
  node->m_eyes = CCNode::create();
  node->m_eyes->setID("hair-eyes"_spr);
  parent->addChild(node->m_eyes, behind->getZOrder() + 1);

  // Face locks and bangs go over the icon
  node->m_front = CCDrawNode::create();
  node->m_front->setID("hair-front"_spr);
  parent->addChild(node->m_front, behind->getZOrder() + 1);

  // Particles live next to the player, over it
  node->m_effects = HairEffectsNode::create();
  node->m_effects->setID("hair-effects"_spr);
  node->m_effects->m_owner = node;
  simSpace->addChild(node->m_effects, 1000);

  return node;
}

bool HairNode::init(CCSprite *head, CCSprite *primary, CCSprite *secondary, CCNode *simSpace)
{
  if (!CCDrawNode::init())
    return false;

  m_head = head;
  m_primary = primary;
  m_secondary = secondary;
  m_simSpace = simSpace;
  m_upAngle = kPi * .5f;

  this->setID("hair"_spr);
  this->reloadConfig();

  return true;
}

void HairNode::reloadConfig()
{
  size_t const oldCount = m_locks.size();
  int const oldSegments = m_config.segments;
  bool const firstLoad = m_locks.empty();

  m_config = m_lookFn ? looks::configFor(m_lookName) : HairConfig::load();
  m_configVersion = HairConfig::version();
  m_sim.setStepRate(m_config.simRate);
  this->generateLocks();

  // Same amount of strands: keep simulating, the hair smoothly moves into its new shape.
  // That's what makes dragging a slider in the customizer look alive
  if (!firstLoad && oldCount == m_locks.size() && oldSegments == m_config.segments)
    return;

  m_sim.setup(static_cast<int>(m_locks.size()), m_config.segments);
  m_sim.setStepRate(m_config.simRate);
  m_targets.assign(m_locks.size(), HairStrandTarget{});
  for (auto &target : m_targets)
    target.restDirs.resize(m_config.segments);
  m_needsReset = true;
}

void HairNode::generateLocks()
{
  auto const &style = styleShape(m_config.style);
  int const count = m_config.lockCount;

  // Fixed seed: the same settings always give the same hairstyle
  std::mt19937 rng(1337);
  std::uniform_real_distribution<float> random(0.f, 1.f);

  // Hair glued to a spinning icon can end up with any side in front, so the hairstyle is
  // symmetric and only the combing in buildTargets() knows where the wind comes from
  bool const symmetric = m_config.spinWithIcon;
  float const halfSpan = (style.backAngle - style.frontAngle) * .5f;
  float const frontAngle = symmetric ? -halfSpan : style.frontAngle;
  float const backAngle = symmetric ? halfSpan : style.backAngle;

  // Where along the head the top (angle 0) is, the length profile peaks around it
  float const top = -style.frontAngle / (style.backAngle - style.frontAngle);

  m_capFrom = frontAngle;
  m_capTo = backAngle;

  // Cap gap: no hair around the top of the head, it comes out from under a cap on the sides.
  // The locks spread over what is left of the arc, in front of the gap and behind it
  float const gap = m_config.hairTopGap;
  float const frontPart = std::max(0.f, -gap - frontAngle);
  float const backPart = std::max(0.f, backAngle - gap);
  int const backCount = m_config.enabled && frontPart + backPart > 0.f ? count : 0;

  m_locks.clear();
  for (int i = 0; i < backCount; ++i)
  {
    float u = (static_cast<float>(i) + .5f) / static_cast<float>(count);
    u = std::clamp(u + (random(rng) - .5f) * .7f / static_cast<float>(count), 0.f, 1.f);

    Lock lock;
    float const along = u * (frontPart + backPart);
    lock.angle = along < frontPart ? frontAngle + along : gap + (along - frontPart);
    // The length profile follows the place on the whole arc, as without a gap
    u = (lock.angle - frontAngle) / (backAngle - frontAngle);

    float lengthFactor;
    if (symmetric)
      lengthFactor = style.topLength + (style.backLength - style.topLength) * std::abs(lock.angle) / halfSpan;
    else if (u < top)
      lengthFactor = style.frontLength + (style.topLength - style.frontLength) * (u / top);
    else
      lengthFactor = style.topLength + (style.backLength - style.topLength) * ((u - top) / (1.f - top));

    lock.length = m_config.length * lengthFactor * (1.f + (random(rng) * 2.f - 1.f) * style.lengthJitter);
    lock.width = m_config.lockWidth * style.width * (.8f + .4f * random(rng));
    lock.curl = style.curl * (.7f + .6f * random(rng));
    lock.depth = random(rng);
    m_locks.push_back(lock);
  }

  // Draw order is the strand order, back layer first
  std::sort(m_locks.begin(), m_locks.end(), [](Lock const &a, Lock const &b)
            { return a.depth < b.depth; });

  m_tailsStart = m_locks.size();
  this->addExtraLocks(rng); // tails, then ahoge, sets m_ahogeStart
  this->addEarAndScarfLocks(); // sets m_earsStart and m_scarfStart

  m_frontStart = m_locks.size();
  m_bangsStart = m_locks.size();
  if (m_config.enabled)
    this->addFrontLocks(rng);

  m_ribbonsStart = m_locks.size();
  this->addRibbonLocks();
  this->markStreaks();
}

void HairNode::markStreaks()
{
  int const count = m_config.streaks;
  if (count <= 0)
    return;

  // Candidates in the order they get picked
  std::vector<Lock *> candidates;
  auto collect = [&](size_t from, size_t to, LockKind kind)
  {
    for (size_t i = from; i < to; ++i)
    {
      if (m_locks[i].kind == kind)
        candidates.push_back(&m_locks[i]);
    }
  };

  switch (m_config.streakPlacement)
  {
  case StreakPlacement::Bangs:
  {
    collect(m_bangsStart, m_ribbonsStart, LockKind::Bang);
    // A group a little off the middle, like a dyed lock in the fringe
    std::sort(candidates.begin(), candidates.end(), [](Lock *a, Lock *b)
              { return std::abs(a->side - .3f) < std::abs(b->side - .3f); });
    break;
  }
  case StreakPlacement::FaceLocks:
    collect(m_frontStart, m_bangsStart, LockKind::FaceLock);
    break;
  case StreakPlacement::Front:
  case StreakPlacement::Back:
  case StreakPlacement::Scattered:
  {
    // Locks of the front layer show the color best
    for (size_t i = 0; i < m_tailsStart; ++i)
    {
      if (m_locks[i].depth >= .4f)
        candidates.push_back(&m_locks[i]);
    }
    if (m_config.streakPlacement == StreakPlacement::Scattered)
    {
      std::mt19937 rng(77);
      std::shuffle(candidates.begin(), candidates.end(), rng);
    }
    else
    {
      float const sign = m_config.streakPlacement == StreakPlacement::Front ? 1.f : -1.f;
      std::sort(candidates.begin(), candidates.end(), [sign](Lock *a, Lock *b)
                { return a->angle * sign < b->angle * sign; });
    }
    break;
  }
  }

  for (size_t i = 0; i < candidates.size() && i < static_cast<size_t>(count); ++i)
    candidates[i]->streak = true;
}

void HairNode::addFrontLocks(std::mt19937 &rng)
{
  std::uniform_real_distribution<float> random(0.f, 1.f);

  if (m_config.faceLocks)
  {
    // +1 is the left side of the face: `across` points to the back of the icon
    for (float side : {1.f, -1.f})
    {
      if ((side > 0.f && !m_config.faceLockLeft) || (side < 0.f && !m_config.faceLockRight))
        continue;

      Lock lock;
      lock.kind = LockKind::FaceLock;
      lock.side = side;
      lock.length = m_config.faceLockLength;
      lock.width = m_config.faceLockWidth;
      lock.curl = 0.f;
      lock.depth = 1.f;
      m_locks.push_back(lock);
    }
  }

  // Bangs last, they cover the face locks
  m_bangsStart = m_locks.size();
  if (m_config.bangs)
  {
    int const count = m_config.bangsCount;
    // `side` -1..1 across the face, -1 is the right (front) side. `toward` is the side Side swept
    // combs to and One side covers, in the same units
    float const toward = m_config.bangsRight ? -1.f : 1.f;
    for (int i = 0; i < count; ++i)
    {
      float const u = count == 1 ? 0.f : (static_cast<float>(i) + .5f) / static_cast<float>(count) * 2.f - 1.f;
      float side = u;
      float lengthScale = 1.f;
      switch (m_config.bangsStyle)
      {
      case BangsStyle::Parted:
        // A bare part in the middle; the locks by the sides are longer, like curtain bangs
        side = (u < 0.f ? -1.f : 1.f) * (kBangPartGap + (1.f - kBangPartGap) * std::abs(u));
        lengthScale = .8f + .5f * std::abs(side);
        break;
      case BangsStyle::SideSwept:
        // Longer towards the side they are combed to
        lengthScale = .7f + .5f * (u * toward + 1.f) * .5f;
        break;
      case BangsStyle::OneSide:
      {
        // All the locks on the covered half (and the edge), shorter towards the bare side
        float const edge = m_config.bangsTransition;
        float const t = count == 1 ? 1.f : (static_cast<float>(i) + .5f) / static_cast<float>(count);
        float const x = -edge + (1.f + edge) * t; // -edge..1, 1 is the covered side
        side = x * toward;
        lengthScale = edge < .02f ? 1.f : std::max(smoothstep(-edge, edge, x), kBangMinLength);
        break;
      }
      default:
        break;
      }

      Lock lock;
      lock.kind = LockKind::Bang;
      lock.side = side;
      lock.length = m_config.bangsLength * lengthScale * (.85f + .3f * random(rng));
      lock.width = m_config.lockWidth * kBangWidth * (.85f + .3f * random(rng));
      lock.curl = 0.f;
      lock.depth = .5f + .5f * random(rng);
      m_locks.push_back(lock);
    }
  }
}

float HairNode::headUnit() const
{
  // Heads come in different sizes (a cube, a wave, a robot head), the hair follows them
  auto const size = m_head->getContentSize();
  float const radius = std::min(size.width, size.height) * .5f;
  return radius > 1.f ? radius / kHeadRadius : 1.f;
}

cocos2d::ccColor4F HairNode::ink(float alpha) const
{
  return premultiplied(m_config.outlineColor, alpha);
}

float HairNode::drawAlpha() const
{
  return m_head->getDisplayedOpacity() / 255.f * m_fadeAlpha;
}

void HairNode::updateFocus(float dt)
{
  float target = 0.f;
  if (m_focusFn && !m_needsReset)
  {
    if (m_config.focusMode == FocusMode::Always)
      target = 1.f;
    else if (m_config.focusMode == FocusMode::Auto)
      target = std::clamp(m_focusFn(), 0.f, 1.f);
  }

  // Comes in fast, goes back slowly after a calm moment, so nothing blinks in and out
  if (target >= m_focus)
  {
    m_focus = approach(m_focus, target, kFocusIn * dt);
    m_focusCalm = 0.f;
  }
  else
  {
    m_focusCalm += dt;
    if (m_focusCalm > kFocusHold)
      m_focus = approach(m_focus, target, kFocusOut * dt);
  }
}

bool HairNode::isActive() const
{
  if (!m_config.customization)
    return false;
  if (!(m_config.enabled || this->extrasActive() || this->decorActive()) || (m_isGarage && !m_config.showInGarage) ||
      (m_isMenu && !m_config.showInMenus))
    return false;
  if (m_gameModeFn && !m_config.showsIn(m_gameModeFn()))
    return false;
  return !m_shouldShow || m_shouldShow();
}

// ! --- Scene --- !

void HairNode::onEnter()
{
  CCDrawNode::onEnter();
  if (m_effects && !m_effects->getParent() && m_simSpace)
  {
    m_effects->m_owner = this;
    m_simSpace->addChild(m_effects, 1000);
  }
}

void HairNode::onExit()
{
  // The effects node lives in another parent: take it along so it never points at a dead node
  if (m_effects)
  {
    m_effects->m_owner = nullptr;
    m_effects->removeFromParent();
  }
  CCDrawNode::onExit();
}

HairEffectsNode *HairEffectsNode::create()
{
  auto node = new HairEffectsNode();
  if (node->init())
  {
    node->autorelease();
    return node;
  }
  delete node;
  return nullptr;
}

void HairEffectsNode::visit()
{
  if (m_owner)
    m_owner->visitEffects(this);
  CCDrawNode::visit();
}

// ! --- Frame --- !

void HairNode::visit()
{
  auto director = CCDirector::sharedDirector();
  unsigned const frame = director->getTotalFrames();

  // visit() can run more than once per frame (render textures), simulate only once
  if (frame != m_lastFrame)
  {
    // Skipped frames mean we were hidden, don't continue from a stale pose
    if (frame != m_lastFrame + 1)
      m_needsReset = true;
    m_lastFrame = frame;

    // Another game mode or player 2 can wear another look
    if (m_lookFn)
    {
      auto look = m_lookFn();
      if (look != m_lookName)
      {
        m_lookName = std::move(look);
        m_configVersion = HairConfig::version() - 1;
      }
    }
    if (m_configVersion != HairConfig::version())
      this->reloadConfig();

    this->clear();
    if (m_front)
      m_front->clear();
    if (!this->isActive())
    {
      m_needsReset = true;
      this->hideEyes();
      return;
    }
    m_wasDead = false;
    m_aliveFrame = frame;

    this->simulate(director->getDeltaTime());
    this->redraw();
  }

  CCDrawNode::visit();
}

void HairNode::updateFrame(float dt, bool snap)
{
  CCPoint const down = m_gravityDir ? m_gravityDir() : CCPoint{0.f, -1.f};
  float const targetAngle = std::atan2(-down.y, -down.x);
  float const targetFacing = m_facingFn ? m_facingFn() : 1.f;

  if (snap)
  {
    m_upAngle = targetAngle;
    m_facing = targetFacing;
    return;
  }

  // Shortest way round, a full flip swings the hair over the back of the head
  float delta = std::remainder(targetAngle - m_upAngle, 2.f * kPi);
  if (std::abs(delta) > kPi * .99f)
    delta = m_facing >= 0.f ? kPi : -kPi;

  m_upAngle += std::clamp(delta, -kTurnSpeed * dt, kTurnSpeed * dt);
  m_facing = approach(m_facing, targetFacing, kFacingSpeed * dt);
}

void HairNode::updateMotion(float dt, CCPoint const &headCenter, bool snap)
{
  CCPoint const velocity = dt > 0.f ? (headCenter - m_lastHeadCenter) / dt : CCPoint{};
  m_lastHeadCenter = headCenter;
  m_lastHeadVelocity = snap ? CCPoint{} : m_headVelocity;
  m_headVelocity = snap ? CCPoint{} : velocity;
  if (snap)
  {
    m_motion = 0.f;
    return;
  }

  // Only the speed along the ground counts, jumping up and down doesn't comb the hair
  CCPoint const down = m_gravityDir ? m_gravityDir() : CCPoint{0.f, -1.f};
  float const speed = std::abs(velocity.cross(down));
  // The air flow combing the hair: the movement scaled by the wind, plus the breeze, all gusty
  float const flow = speed / (kFullCombSpeed * m_simScale) * m_config.windMultiplier + m_config.breeze;
  float const target = std::clamp(flow * m_gust, 0.f, 1.f);
  m_motion = approach(m_motion, target, kMotionResponse * dt);
}

void HairNode::updateGust(float dt)
{
  // Real wind never blows the same: two layers of smooth noise, slow swells and quicker gusts
  m_time += dt;
  float const t = m_time * kGustFrequency * m_config.gustSpeed;
  float const noise = .65f * hairNoise(t) + .35f * hairNoise(t * 2.3f + 17.f);
  m_gust = std::max(0.f, 1.f + m_config.gusts * noise * 1.5f);
}

void HairNode::gravityAxes(CCPoint &up, CCPoint &back) const
{
  up = CCPoint{std::cos(m_upAngle), std::sin(m_upAngle)};
  // Back is against the movement, kept perpendicular to up. It shrinks while turning around,
  // which squeezes the hairstyle like a head turning
  back = CCPoint{-m_facing, 0.f};
  back = back - up * back.dot(up);
}

void HairNode::iconAxes(CCAffineTransform const &headToSim, CCPoint &up, CCPoint &back) const
{
  // The hair is glued to the top of the head and spins with it. Mirroring done through the
  // transform carries over by itself, mirroring done by flipping the texture is applied here
  up = normalized(applyVec({0.f, m_head->isFlipY() ? -1.f : 1.f}, headToSim), {0.f, 1.f});
  back = applyVec({m_head->isFlipX() ? 1.f : -1.f, 0.f}, headToSim);
  back = normalized(back - up * back.dot(up), {-up.y, up.x});
}

void HairNode::buildTargets(CCPoint const &headCenter, CCPoint const &up, CCPoint const &back)
{
  auto const &style = styleShape(m_config.style);
  int const segments = m_config.segments;

  float const rootRadius = kHeadRadius * kRootDepth * m_simScale;

  // Hair glued to a spinning icon drapes down over the head while standing still and gets
  // combed away from the movement while moving, both in world space.
  // While turning around `worldBack` shrinks, so does the combing of the windward side
  CCPoint worldUp;
  CCPoint worldBack;
  this->gravityAxes(worldUp, worldBack);
  CCPoint const worldDown = worldUp * -1.f;
  CCPoint const horizontal = {-worldUp.y, worldUp.x};
  CCPoint const comb = normalized(worldBack - worldUp * kCombDown, worldDown);

  // Across the face, the front locks spread along it
  CCPoint const across = normalized(back, {-up.y, up.x});

  for (size_t s = 0; s < m_locks.size(); ++s)
  {
    auto const &lock = m_locks[s];
    auto &target = m_targets[s];

    if (lock.kind == LockKind::FaceLock || lock.kind == LockKind::Bang)
    {
      this->buildFrontTarget(lock, target, headCenter, up, across);
      continue;
    }
    if (lock.kind != LockKind::Back)
    {
      this->buildExtraTarget(lock, target, headCenter, up, across);
      continue;
    }

    target.collide = true;
    target.stiffnessPower = style.stiffnessPower;

    float const angle = radians(lock.angle);
    CCPoint const radial = up * std::cos(angle) + back * std::sin(angle);
    target.root = headCenter + radial * rootRadius;
    target.segmentLength = lock.length * m_simScale / static_cast<float>(segments);

    if (m_config.spinWithIcon)
    {
      // Standing still a lock falls down on the side of the head it grows from
      // The parting sits in front of the top, so most of the hair goes back and the front makes a fringe
      float const sideness = radial.dot(horizontal) + kDrapeBackBias * worldBack.dot(horizontal);
      CCPoint const side = horizontal * (sideness >= 0.f ? 1.f : -1.f);
      float const drapeTurn = turnAngle(radial, worldDown, side);

      // Moving, locks get combed back over the top. The ones facing the wind turn
      // right at the root, otherwise they would stick out forward like a sword
      float const combTurn = turnAngle(radial, comb, worldUp);
      float const windward = std::max(0.f, -radial.dot(worldBack)) * m_motion;

      float const turn = drapeTurn + (combTurn - drapeTurn) * m_motion;
      float const along = (kDrapeAlong + (kCombAlong - kDrapeAlong) * m_motion) * lock.curl / kCurlReference;

      for (int k = 0; k < segments; ++k)
      {
        float const t = windward * kWindwardComb + along * static_cast<float>(k + 1) / static_cast<float>(segments);
        target.restDirs[k] = rotated(radial, turn * std::min(t, 1.f));
      }
    }
    else
    {
      // Front locks curl forward and down, the rest curl back and down
      float const curlSide = std::clamp((lock.angle + 10.f) / 20.f, -1.f, 1.f);
      for (int k = 0; k < segments; ++k)
      {
        float const dir = radians(lock.angle + curlSide * lock.curl * static_cast<float>(k + 1));
        target.restDirs[k] = normalized(up * std::cos(dir) + back * std::sin(dir), up);
      }
    }

    // Scaled by gravity over segment length so the hair bends by the same angle at any size
    target.stiffness = m_config.volume * style.stiffness * kSpringScale * kBaseGravity * m_simScale / target.segmentLength;
  }
}

void HairNode::buildFrontTarget(Lock const &lock, HairStrandTarget &target, CCPoint const &headCenter,
                                CCPoint const &up, CCPoint const &across)
{
  // Front locks lie on the face, in front of the head, so they never collide with it.
  // Their shape is glued to the head frame like the rest of the hairstyle
  int const segments = m_config.segments;
  float const unit = m_simScale; // icon units to sim units
  float const half = kHeadRadius * unit;
  CCPoint const down = up * -1.f;

  target.collide = false;
  target.stiffnessPower = kFrontStiffnessPower;
  target.segmentLength = lock.length * m_simScale / static_cast<float>(segments);

  float stiffness;
  if (lock.kind == LockKind::FaceLock)
  {
    // From the top corners of the face, out a bit and then down in soft waves.
    // Inset X moves both locks to the middle, inset Y down the face, shift both to the front, and
    // tilt raises the front lock (side -1, `across` points back) and lowers the back one
    float const x = half * kFaceLockRootX - m_config.faceLockInsetX * unit;
    float const y = half * kFaceLockRootY - m_config.faceLockInsetY * unit - lock.side * m_config.faceLockTilt * .5f * unit;
    target.root = headCenter + up * y + across * (lock.side * x - m_config.faceLockShiftX * unit);
    for (int k = 0; k < segments; ++k)
    {
      float const t = (static_cast<float>(k) + .5f) / static_cast<float>(segments);
      float const wave = std::sin(t * kFaceLockWaves * 2.f * kPi) * kFaceLockWaveAmp;
      float const angle = radians(kFaceLockFlare * (1.f - t) + wave);
      target.restDirs[k] = down * std::cos(angle) + across * (lock.side * std::sin(angle));
    }
    stiffness = kFaceLockStiffness;
  }
  else
  {
    // A row along the hairline, falling down and fanning out a little to the sides
    float const spread = std::max(m_config.bangsSpread, .05f);
    float const t = std::abs(lock.side);
    float const exponent = this->bangArcExponent();
    target.root = this->bangRoot(lock.side, headCenter, up, across);

    // On a steep arc the bangs fall away from it, like hair along a round hairline
    float const slope = t > 0.f ? m_config.bangsArcSize * exponent * std::pow(t, exponent - 1.f) / (spread * kHeadRadius) : 0.f;
    float const arcTurn = std::min(std::atan(slope) * 180.f / kPi, kMaxArcTurn);
    float const arc = lock.side < 0.f ? -arcTurn : arcTurn;

    // Where the lock points at the root and at the tip, degrees from down towards `across`:
    // combed bangs leave the hairline sideways and fall down towards the tips
    float rootAngle = lock.side * kBangFan;
    float tipAngle = rootAngle;
    float const toward = m_config.bangsRight ? -1.f : 1.f;
    switch (m_config.bangsStyle)
    {
    case BangsStyle::Parted:
    {
      float const away = lock.side < 0.f ? -1.f : 1.f;
      rootAngle = away * (52.f - 20.f * t);
      tipAngle = away * (4.f + 4.f * t);
      break;
    }
    case BangsStyle::SideSwept:
      rootAngle = toward * 56.f;
      tipAngle = toward * 8.f;
      break;
    case BangsStyle::OneSide:
      rootAngle = toward * 18.f;
      tipAngle = toward * 4.f;
      break;
    default:
      break;
    }
    for (int k = 0; k < segments; ++k)
    {
      // Eases out: most of the turn happens near the root
      float const along = (static_cast<float>(k) + .5f) / static_cast<float>(segments);
      float const turn = 1.f - (1.f - along) * (1.f - along);
      float const angle = radians(rootAngle + (tipAngle - rootAngle) * turn + arc);
      target.restDirs[k] = down * std::cos(angle) + across * std::sin(angle);
    }
    stiffness = kBangStiffness;
  }

  target.stiffness = m_config.volume * stiffness * kSpringScale * kBaseGravity * m_simScale / target.segmentLength;
}

float HairNode::bangArcExponent() const
{
  return kArcSharpExponent + (kArcSoftExponent - kArcSharpExponent) * m_config.bangsArcSoftness;
}

CCPoint HairNode::bangRoot(float side, CCPoint const &headCenter, CCPoint const &up, CCPoint const &across) const
{
  // The hairline is an arc for icons with a round top: |x|^n drops the ends by the arc size,
  // a high n keeps the middle flat, a low one makes a smooth arc
  float const unit = m_simScale;
  float const spread = std::max(m_config.bangsSpread, .05f);
  float const drop = m_config.bangsArcSize * std::pow(std::abs(side), this->bangArcExponent());

  float const x = side * spread * kHeadRadius;
  float const y = kHeadRadius * kBangRootY - drop - m_config.bangsInsetY;
  CCPoint const right = across * -1.f;
  return headCenter + up * (y * unit) + across * (x * unit) + right * (m_config.bangsInsetX * unit);
}

void HairNode::simulate(float dt)
{
  auto const headToSim = CCAffineTransformConcat(m_head->nodeToWorldTransform(), m_simSpace->worldToNodeTransform());
  m_simScale = applyVec({1.f, 0.f}, headToSim).getLength() * this->headUnit();

  auto const size = m_head->getContentSize();
  CCPoint const headCenter = CCPointApplyAffineTransform({size.width * .5f, size.height * .5f}, headToSim);

  // Keep the gravity frame up to date in both modes, switching the setting then doesn't jump
  this->updateFrame(dt, m_needsReset);
  this->updateGust(dt);
  this->updateMotion(dt, headCenter, m_needsReset);

  this->updateBow(dt);

  m_isBox = m_boxHead && m_boxHead();
  m_headAxisX = normalized(applyVec({1.f, 0.f}, headToSim), {1.f, 0.f});
  m_headAxisY = normalized(applyVec({0.f, 1.f}, headToSim), {0.f, 1.f});

  this->iconAxes(headToSim, m_iconUp, m_iconBack);
  this->updateEars(dt);

  CCPoint up;
  CCPoint back;
  if (m_config.spinWithIcon)
  {
    up = m_iconUp;
    back = m_iconBack;
  }
  else
    this->gravityAxes(up, back);
  this->buildTargets(headCenter, up, back);

  // Landings make the blush pop and the hearts burst
  CCPoint const across = normalized(back, {-up.y, up.x});
  bool const onGround = m_onGround && m_onGround();
  if (onGround && !m_wasOnGround && !m_needsReset)
    this->onLanded(headCenter, up, across);
  this->updateFace(dt, headCenter, !onGround && m_wasOnGround && !m_needsReset);
  this->updateWings(dt, !onGround && m_wasOnGround && !m_needsReset);
  m_wasOnGround = onGround;
  this->updatePet(dt, headCenter);
  this->updateCharms(dt);
  this->updateFocus(dt);
  this->updateDecor(dt, headCenter, up, across);

  // How fast the hairstyle frame turns: a spinning cube in a jump, a gravity flip
  float spinSpeed = 0.f;
  if (!m_needsReset && dt > 0.f)
    spinSpeed = std::atan2(m_lastUp.cross(up), m_lastUp.dot(up)) / dt;
  m_lastUp = up;

  HairSimParams params;
  params.damping = m_config.damping;
  params.friction = m_config.friction;
  params.windMultiplier = m_config.windMultiplier * m_gust;
  params.spinSpeed = spinSpeed;

  // Follows the spin right away, fades out slowly after it so the hair settles after the landing
  float const calmTarget = m_config.calmJumps ? kCalmStrength * std::clamp(std::abs(spinSpeed) / kFullCalmSpin, 0.f, 1.f) : 0.f;
  m_calm = m_needsReset ? 0.f : std::max(calmTarget, m_calm - kCalmStrength * dt / kCalmFade);
  params.calm = m_calm;
  params.teleportDistance = kTeleportDistance * std::max(m_simScale, 1.f);
  params.headCenter = headCenter;
  // Everything the hair lies on scales with the hitbox setting, the roots and the floor don't
  float const hitbox = kHeadRadius * m_config.hitboxMultiplier * m_simScale;
  params.headRadius = hitbox * kColliderScale;
  params.rootRadius = kHeadRadius * kRootDepth * m_simScale;

  // A cube is a box, the hair lies right on its faces and slides around the corners
  if (m_boxHead && m_boxHead())
  {
    params.headBox = true;
    params.headAxisX = normalized(applyVec({1.f, 0.f}, headToSim), {1.f, 0.f});
    params.headAxisY = normalized(applyVec({0.f, 1.f}, headToSim), {0.f, 1.f});
    params.headHalfSize = hitbox * kBoxScale;
    params.headExponent = kBoxExponent;
  }

  CCPoint const down = m_gravityDir ? m_gravityDir() : CCPoint{0.f, -1.f};
  params.gravity = down * (kBaseGravity * m_config.gravity * m_simScale);

  // Oncoming air presses the hair to the front of the head instead of letting it bulge like a shield.
  // `worldBack` shrinks while turning around, so does the pressing
  CCPoint worldUp;
  CCPoint worldBack;
  this->gravityAxes(worldUp, worldBack);
  params.windDir = worldBack * -1.f;
  params.windPress = m_motion;

  // The breeze blows from the front like the air of the movement, flutter follows the whole flow
  params.breeze = worldBack * (m_config.breeze * m_gust * kFullCombSpeed * m_simScale);
  params.flutter = m_config.flutter * m_motion * kFlutterAccel * m_simScale;
  params.flutterDir = normalized(worldBack, {-1.f, 0.f});
  params.time = m_time;
  params.tightRadius = hitbox * kPressedRoundScale;
  params.tightHalfSize = hitbox * kPressedScale;
  params.tightExponent = kPressedExponent;

  // Standing on the ground: hair under the head spreads along the floor instead of going through it
  if (m_onGround && m_onGround())
  {
    params.floorDown = down;
    params.floorDistance = kHeadRadius * m_simScale;
  }

  if (m_idleWind)
  {
    float const gust = std::sin(m_time * 1.3f) + .5f * std::sin(m_time * 3.1f + 1.f);
    params.wind = CCPoint{gust * kWindStrength * m_config.windMultiplier * m_simScale, 0.f};
  }

  m_frameParams = params;
  m_frameUp = up;
  m_frameBack = back;

  if (m_needsReset)
  {
    m_sim.reset(m_targets, headCenter);
    m_needsReset = false;
  }
  else
  {
    m_sim.step(dt, m_targets, params);
  }
  this->updateCape(dt, headCenter);
}

// ! --- Drawing --- !

ccColor4F HairNode::hairColor() const
{
  return this->sourceColor(m_config.colorSource, m_config.customColor);
}

ccColor4F HairNode::lockColor(Lock const &lock) const
{
  auto const kind = lock.kind;
  if (lock.streak)
    return this->sourceColor(HairColorSource::Custom, m_config.streakColor);
  // Bow ribbons: the head bow has its own color, the bows on the tails use the tie color
  if (kind == LockKind::Ribbon)
  {
    if (lock.group == 0)
      return m_config.bowColorSource == HairColorSource::Hair ? this->hairColor() : this->sourceColor(m_config.bowColorSource, m_config.bowColor);
    return m_config.tieColorSource == HairColorSource::Hair ? this->hairColor() : this->sourceColor(m_config.tieColorSource, m_config.tieColor);
  }
  if (kind == LockKind::Ear && m_config.earColorSource != HairColorSource::Hair)
    return this->sourceColor(m_config.earColorSource, m_config.earColor);
  if (kind == LockKind::Trail)
    return m_config.trailColorSource == HairColorSource::Hair ? this->hairColor() : this->sourceColor(m_config.trailColorSource, m_config.trailColor);
  if (kind == LockKind::ScarfEnd)
    return m_config.scarfColorSource == HairColorSource::Hair ? this->hairColor() : this->sourceColor(m_config.scarfColorSource, m_config.scarfColor);
  if (kind == LockKind::Tail && m_config.tailColorSource != HairColorSource::Hair)
    return this->sourceColor(m_config.tailColorSource, m_config.tailColor);
  if (kind == LockKind::FaceLock && m_config.faceLockColorSource != HairColorSource::Hair)
    return this->sourceColor(m_config.faceLockColorSource, m_config.faceLockColor);
  if (kind == LockKind::Bang && m_config.bangsColorSource != HairColorSource::Hair)
    return this->sourceColor(m_config.bangsColorSource, m_config.bangsColor);
  return this->hairColor();
}

ccColor4F HairNode::sourceColor(HairColorSource source, ccColor3B const &custom) const
{
  float const alpha = this->drawAlpha();
  auto const secondary = m_secondary ? m_secondary->getColor() : m_primary->getColor();

  switch (source)
  {
  case HairColorSource::Primary:
    return premultiplied(m_primary->getColor(), alpha);
  case HairColorSource::Custom:
    return premultiplied(custom, alpha);
  case HairColorSource::Hair:
  case HairColorSource::Secondary:
    break;
  }
  return premultiplied(secondary, alpha);
}

void HairNode::drawCap(CCAffineTransform const &simToHair, ccColor4F const &color, float outline, ccColor4F const &outlineColor)
{
  // With a cap gap the base is two pieces, in front of the gap and behind it
  float const gap = m_config.hairTopGap;
  if (gap <= 0.f)
  {
    this->drawCapArc(simToHair, m_capFrom, m_capTo, color, outline, outlineColor);
    return;
  }
  if (-gap > m_capFrom)
    this->drawCapArc(simToHair, m_capFrom, -gap, color, outline, outlineColor);
  if (gap < m_capTo)
    this->drawCapArc(simToHair, gap, m_capTo, color, outline, outlineColor);
}

void HairNode::drawCapArc(CCAffineTransform const &simToHair, float from, float to, ccColor4F const &color, float outline,
                          ccColor4F const &outlineColor)
{
  // Base of the hairstyle along the collider surface, fills the gaps between the locks
  // (a parting on top, the corners of a cube) so no background shows through the hair
  std::array<CCPoint, kCapPoints + 1> verts;
  CCPoint const center = m_frameParams.headCenter;
  float const capSize = kHeadRadius * kCapScale * m_simScale;

  verts[0] = CCPointApplyAffineTransform(center, simToHair);
  for (int i = 0; i < kCapPoints; ++i)
  {
    float const t = static_cast<float>(i) / static_cast<float>(kCapPoints - 1);
    float const angle = radians(from + (to - from) * t);
    CCPoint const dir = normalized(m_frameUp * std::cos(angle) + m_frameBack * std::sin(angle), m_frameUp);
    // The ends sink into the head, otherwise they'd stick out as ledges when the locks move away
    float const edge = std::clamp(std::min(t, 1.f - t) / kCapEdgeFade, 0.f, 1.f);
    float surface = capSize;
    if (m_frameParams.headBox)
    {
      float const x = std::abs(dir.dot(m_frameParams.headAxisX));
      float const y = std::abs(dir.dot(m_frameParams.headAxisY));
      surface /= std::pow(std::pow(x, kCapExponent) + std::pow(y, kCapExponent), 1.f / kCapExponent);
    }
    CCPoint point = center + dir * (m_frameParams.rootRadius + (surface - m_frameParams.rootRadius) * edge);

    if (m_frameParams.floorDistance > 0.f)
    {
      float const below = (point - center).dot(m_frameParams.floorDown) - m_frameParams.floorDistance;
      if (below > 0.f)
        point = point - m_frameParams.floorDown * below;
    }

    verts[i + 1] = CCPointApplyAffineTransform(point, simToHair);
  }

  this->drawPolygon(verts.data(), static_cast<unsigned>(verts.size()), color, outline, outlineColor);
}

void HairNode::redraw()
{
  // Focus: what floats around fades or hides, the hair only with "Fade the hair too"
  float const faded = 1.f - m_focus * (1.f - m_config.focusOpacity);
  float const hidden = 1.f - m_focus;
  float const hair = m_config.focusHair ? faded : 1.f;
  m_fadeAlpha = hair;

  // The cape behind everything, it fades in focus like the other things around the icon
  m_fadeAlpha = faded;
  this->drawCape(this);
  m_fadeAlpha = hair;

  // Behind the icon: the hairstyle with its base, the tails with their ties, the ahoge, the ears,
  // the ends of the scarf
  this->drawLocks(this, 0, m_tailsStart, true);
  if (m_config.braidTails)
    this->drawBraids(this, m_tailsStart, m_ahogeStart);
  else
    this->drawLocks(this, m_tailsStart, m_ahogeStart, false);
  this->drawTies(this);
  this->drawLocks(this, m_ahogeStart, m_earsStart, false);
  this->drawLocks(this, m_earsStart, m_scarfStart, false);
  this->drawEarInners(this);
  this->drawLocks(this, m_scarfStart, m_trailStart, false);
  m_fadeAlpha = hidden;
  this->drawRibbon(this);
  m_fadeAlpha = faded;
  this->drawWings(this);
  m_fadeAlpha = hair;
  if (!m_front)
  {
    m_fadeAlpha = 1.f;
    return;
  }

  // In front of it: the blush on the cheeks, the scarf band, then the hair over them. Each group
  // gets its own outline so the bangs clearly lie over the face locks; the clips, bows and the
  // hearts go over everything
  this->drawBlush(m_front);
  this->drawSticker(m_front);
  this->drawFace(m_front);
  this->drawScarfBand(m_front);
  this->drawCollarAndBell(m_front);
  this->drawEarrings(m_front);
  if (m_config.braidFaceLocks)
    this->drawBraids(m_front, m_frontStart, m_bangsStart);
  else
    this->drawLocks(m_front, m_frontStart, m_bangsStart, false);
  this->drawGlasses(m_front);
  this->drawLocks(m_front, m_bangsStart, m_ribbonsStart, false);
  this->drawHeadband(m_front);
  this->drawFlowers(m_front);
  this->drawHeadphones(m_front);
  this->drawHat(m_front);
  this->drawClips(m_front);
  this->drawLocks(m_front, m_ribbonsStart, m_locks.size(), false);
  this->drawBows(m_front);
  m_fadeAlpha = faded;
  this->drawHalo(m_front);
  m_fadeAlpha = 1.f;

  if (m_debugDraw)
    this->drawDebug();
}

void HairNode::drawLocks(CCDrawNode *node, size_t from, size_t to, bool withCap)
{
  if (from >= to)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const hairScale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();

  float const outline = kOutlineWidth * hairScale;
  auto const hair = this->hairColor();
  auto const outlineColor = this->ink(hair.a);
  auto const &strands = m_sim.strands();
  int const subdiv = m_config.lockCount > kDenseLockCount ? kCurveSubdivDense : kCurveSubdiv;
  to = std::min(to, strands.size());

  for (int pass = m_config.outline ? 0 : 1; pass < 2; ++pass)
  {
    if (pass == 1 && withCap)
      this->drawCap(simToNode, shaded(hair, kBackShade), m_config.outline ? outline : 0.f, outlineColor);

    for (size_t s = from; s < to; ++s)
    {
      auto const &lock = m_locks[s];
      float const rootRadius = lock.width * .5f * hairScale;
      auto const color = shaded(this->lockColor(lock), kBackShade + (1.f - kBackShade) * lock.depth);

      buildCurve(strands[s], simToNode, subdiv, m_curve);
      int const last = static_cast<int>(m_curve.size()) - 1;

      for (int k = 1; k <= last; ++k)
      {
        float const along = static_cast<float>(k) / static_cast<float>(last);
        float const radius = rootRadius * this->lockWidthAt(lock, along);

        if (pass == 0)
          node->drawSegment(m_curve[k - 1], m_curve[k], radius + outline * (1.f - kOutlineTaper * along), outlineColor);
        else
          node->drawSegment(m_curve[k - 1], m_curve[k], radius, this->lockColorAt(lock, color, along));
      }
    }
  }
}

// ! --- Hitbox helpers --- !

void HairNode::drawDebug()
{
  auto const simToFront = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), m_front->worldToNodeTransform());
  auto const headToFront = CCAffineTransformConcat(m_head->nodeToWorldTransform(), m_front->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToFront).getLength() * this->headUnit();
  float const line = kDebugLine * scale;
  float const dot = kDebugDot * scale;

  auto const &params = m_frameParams;
  CCPoint const center = params.headCenter;
  auto toFront = [&](CCPoint const &point)
  { return CCPointApplyAffineTransform(point, simToFront); };

  // Collider the hair lies on, with the windward side pressed while moving
  CCPoint previous;
  for (int i = 0; i <= kDebugColliderPoints; ++i)
  {
    float const angle = 2.f * kPi * static_cast<float>(i) / static_cast<float>(kDebugColliderPoints);
    CCPoint const dir = {std::cos(angle), std::sin(angle)};
    CCPoint const point = toFront(center + dir * params.headSurface(dir));
    if (i > 0)
      m_front->drawSegment(previous, point, line, kDebugCollider);
    previous = point;
  }

  // Floor while standing on the ground
  if (params.floorDistance > 0.f)
  {
    CCPoint const floor = center + params.floorDown * params.floorDistance;
    CCPoint const along = CCPoint{-params.floorDown.y, params.floorDown.x} * (params.floorDistance * 2.f);
    m_front->drawSegment(toFront(floor - along), toFront(floor + along), line, kDebugFloor);
  }

  // Roots of every lock
  for (size_t s = 0; s < m_locks.size() && s < m_targets.size(); ++s)
  {
    auto const kind = m_locks[s].kind;
    auto const color = kind == LockKind::FaceLock ? kDebugFaceLocks
                       : kind == LockKind::Bang   ? kDebugBangs
                                                  : kDebugRoots;
    m_front->drawDot(toFront(m_targets[s].root), kind == LockKind::Back ? dot * .6f : dot, color);
  }

  // Clip anchors and the scarf knot
  for (int i = 0; i < m_config.clipCount; ++i)
  {
    CCPoint position;
    CCPoint direction;
    if (this->clipPlacement(i, position, direction))
      m_front->drawDot(toFront(position), dot * 1.3f, kDebugBangs);
  }
  if (m_config.scarf)
    m_front->drawDot(toFront(this->scarfKnot(center)), dot * 1.3f, kDebugRoots);
  if (m_config.halo)
    m_front->drawDot(toFront(this->haloTarget(center)), dot * 1.3f, kDebugRoots);
  if (m_config.pet != PetStyle::None)
    m_front->drawDot(toFront(this->petTarget(center)), dot * 1.3f, kDebugRoots);

  // Bow knots
  CCPoint const across = normalized(m_frameBack, {-m_frameUp.y, m_frameUp.x});
  for (int group = 0; group < 3; ++group)
  {
    if (this->bowExists(group))
      m_front->drawDot(toFront(this->bowAnchor(group, center, m_frameUp, across)), dot * 1.3f, kDebugBangs);
  }

  // The whole bangs hairline, so the arc settings are visible even with a few locks
  if (m_config.bangs)
  {
    for (int i = 0; i <= kDebugArcPoints; ++i)
    {
      float const side = static_cast<float>(i) / static_cast<float>(kDebugArcPoints) * 2.f - 1.f;
      CCPoint const point = toFront(this->bangRoot(side, center, m_frameUp, across));
      if (i > 0)
        m_front->drawSegment(previous, point, line, kDebugBangs);
      previous = point;
    }
  }
}
