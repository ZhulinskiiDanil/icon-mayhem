#include "HairNode.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

using namespace geode::prelude;

// ! --- Constants --- !

namespace
{
  constexpr float kPi = std::numbers::pi_v<float>;

  constexpr float kBaseGravity = 900.f; // units / s^2 for gravity = 1
  constexpr float kTeleportDistance = 160.f;
  constexpr float kHeadRadius = 15.f;     // hair units, half of the cube
  constexpr float kRootDepth = .72f;      // roots sit inside the head so the icon hides them
  constexpr float kColliderScale = 1.22f; // hair rests a bit above the head, gives it volume
  constexpr float kSpringScale = 13.f;    // style spring vs gravity, see lockStiffness()
  constexpr float kTurnSpeed = 12.f;      // radians / s the hairstyle rotates after a gravity flip
  constexpr float kFacingSpeed = 6.f;     // facing units / s when turning around
  constexpr float kWindStrength = 300.f;  // units / s^2 in the garage

  constexpr int kCurveSubdiv = 3;      // drawn points per simulated segment
  constexpr float kTipTaper = .92f;    // how much thinner the tip is than the root
  constexpr float kOutlineWidth = .6f; // icon units
  constexpr float kOutlineTaper = .6f; // outline thins towards the tip too, no blobs on the ends
  constexpr float kBackShade = .72f;   // brightness of the deepest lock

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

  CCPoint applyVec(CCPoint const &v, CCAffineTransform const &t)
  {
    return {t.a * v.x + t.c * v.y, t.b * v.x + t.d * v.y};
  }

  CCPoint normalized(CCPoint const &v, CCPoint const &fallback)
  {
    float const len = v.getLength();
    return len > .0001f ? v / len : fallback;
  }

  float radians(float degrees)
  {
    return degrees * kPi / 180.f;
  }

  float approach(float value, float target, float maxDelta)
  {
    return value + std::clamp(target - value, -maxDelta, maxDelta);
  }

  CCPoint catmullRom(CCPoint const &p0, CCPoint const &p1, CCPoint const &p2, CCPoint const &p3, float t)
  {
    float const t2 = t * t;
    float const t3 = t2 * t;
    return (p1 * 2.f + (p2 - p0) * t + (p0 * 2.f - p1 * 5.f + p2 * 4.f - p3) * t2 + (p1 * 3.f - p0 - p2 * 3.f + p3) * t3) * .5f;
  }

  // Smooth curve through the simulated points, already moved into the hair node space
  void buildCurve(std::vector<CCPoint> const &pts, CCAffineTransform const &toHair, std::vector<CCPoint> &out)
  {
    int const n = static_cast<int>(pts.size());
    auto at = [&](int i)
    {
      if (i < 0)
        return pts[0] * 2.f - pts[1];
      if (i >= n)
        return pts[n - 1] * 2.f - pts[n - 2];
      return pts[i];
    };

    out.clear();
    for (int i = 0; i + 1 < n; ++i)
    {
      for (int j = 0; j < kCurveSubdiv; ++j)
      {
        float const t = static_cast<float>(j) / static_cast<float>(kCurveSubdiv);
        out.push_back(CCPointApplyAffineTransform(catmullRom(at(i - 1), at(i), at(i + 1), at(i + 2), t), toHair));
      }
    }
    out.push_back(CCPointApplyAffineTransform(pts[n - 1], toHair));
  }

  ccColor4F premultiplied(ccColor3B const &color, float alpha)
  {
    return {
        color.r / 255.f * alpha,
        color.g / 255.f * alpha,
        color.b / 255.f * alpha,
        alpha,
    };
  }

  ccColor4F shaded(ccColor4F const &color, float shade)
  {
    return {color.r * shade, color.g * shade, color.b * shade, color.a};
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
  m_config = HairConfig::load();
  m_configVersion = HairConfig::version();
  this->generateLocks();

  m_sim.setup(m_config.lockCount, m_config.segments);
  m_targets.assign(m_config.lockCount, HairStrandTarget{});
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

  // Where along the head the top (angle 0) is, the length profile peaks around it
  float const top = -style.frontAngle / (style.backAngle - style.frontAngle);

  m_locks.clear();
  for (int i = 0; i < count; ++i)
  {
    float u = (static_cast<float>(i) + .5f) / static_cast<float>(count);
    u = std::clamp(u + (random(rng) - .5f) * .7f / static_cast<float>(count), 0.f, 1.f);

    float const lengthFactor = u < top
                                   ? style.frontLength + (style.topLength - style.frontLength) * (u / top)
                                   : style.topLength + (style.backLength - style.topLength) * ((u - top) / (1.f - top));

    Lock lock;
    lock.angle = style.frontAngle + (style.backAngle - style.frontAngle) * u;
    lock.length = m_config.length * lengthFactor * (1.f + (random(rng) * 2.f - 1.f) * style.lengthJitter);
    lock.width = m_config.lockWidth * style.width * (.8f + .4f * random(rng));
    lock.curl = style.curl * (.7f + .6f * random(rng));
    lock.depth = random(rng);
    m_locks.push_back(lock);
  }

  // Draw order is the strand order, back layer first
  std::sort(m_locks.begin(), m_locks.end(), [](Lock const &a, Lock const &b)
            { return a.depth < b.depth; });
}

float HairNode::headUnit() const
{
  // Heads come in different sizes (a cube, a wave, a robot head), the hair follows them
  auto const size = m_head->getContentSize();
  float const radius = std::min(size.width, size.height) * .5f;
  return radius > 1.f ? radius / kHeadRadius : 1.f;
}

bool HairNode::isActive() const
{
  if (!m_config.enabled || (m_isGarage && !m_config.showInGarage))
    return false;
  return !m_shouldShow || m_shouldShow();
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

    if (m_configVersion != HairConfig::version())
      this->reloadConfig();

    this->clear();
    if (!this->isActive())
    {
      m_needsReset = true;
      return;
    }

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

void HairNode::buildTargets(CCPoint const &headCenter)
{
  auto const &style = styleShape(m_config.style);
  int const segments = m_config.segments;

  CCPoint const up = {std::cos(m_upAngle), std::sin(m_upAngle)};
  // Back is against the movement, kept perpendicular to up. It shrinks while turning around,
  // which squeezes the hairstyle like a head turning
  CCPoint back = {-m_facing, 0.f};
  back = back - up * back.dot(up);

  float const rootRadius = kHeadRadius * kRootDepth * m_simScale;

  for (size_t s = 0; s < m_locks.size(); ++s)
  {
    auto const &lock = m_locks[s];
    auto &target = m_targets[s];

    float const angle = radians(lock.angle);
    target.root = headCenter + (up * std::cos(angle) + back * std::sin(angle)) * rootRadius;
    target.segmentLength = lock.length * m_simScale / static_cast<float>(segments);

    // Front locks curl forward and down, the rest curl back and down
    float const curlSide = std::clamp((lock.angle + 10.f) / 20.f, -1.f, 1.f);
    for (int k = 0; k < segments; ++k)
    {
      float const dir = radians(lock.angle + curlSide * lock.curl * static_cast<float>(k + 1));
      target.restDirs[k] = normalized(up * std::cos(dir) + back * std::sin(dir), up);
    }

    // Scaled by gravity over segment length so the hair bends by the same angle at any size
    target.stiffness = m_config.volume * style.stiffness * kSpringScale * kBaseGravity * m_simScale / target.segmentLength;
  }
}

void HairNode::simulate(float dt)
{
  auto const headToSim = CCAffineTransformConcat(m_head->nodeToWorldTransform(), m_simSpace->worldToNodeTransform());
  m_simScale = applyVec({1.f, 0.f}, headToSim).getLength() * this->headUnit();

  auto const size = m_head->getContentSize();
  CCPoint const headCenter = CCPointApplyAffineTransform({size.width * .5f, size.height * .5f}, headToSim);

  this->updateFrame(dt, m_needsReset);
  this->buildTargets(headCenter);

  HairSimParams params;
  params.stiffnessPower = styleShape(m_config.style).stiffnessPower;
  params.damping = m_config.damping;
  params.teleportDistance = kTeleportDistance * std::max(m_simScale, 1.f);
  params.headCenter = headCenter;
  params.headRadius = kHeadRadius * kColliderScale * m_simScale;
  params.rootRadius = kHeadRadius * kRootDepth * m_simScale;

  CCPoint const down = m_gravityDir ? m_gravityDir() : CCPoint{0.f, -1.f};
  params.gravity = down * (kBaseGravity * m_config.gravity * m_simScale);

  if (m_idleWind)
  {
    m_time += dt;
    float const gust = std::sin(m_time * 1.3f) + .5f * std::sin(m_time * 3.1f + 1.f);
    params.wind = CCPoint{gust * kWindStrength * m_simScale, 0.f};
  }

  if (m_needsReset)
  {
    m_sim.reset(m_targets, headCenter);
    m_needsReset = false;
  }
  else
  {
    m_sim.step(dt, m_targets, params);
  }
}

// ! --- Drawing --- !

ccColor4F HairNode::hairColor() const
{
  float const alpha = m_head->getDisplayedOpacity() / 255.f;

  switch (m_config.colorSource)
  {
  case HairColorSource::Primary:
    return premultiplied(m_primary->getColor(), alpha);
  case HairColorSource::Secondary:
    return premultiplied(m_secondary ? m_secondary->getColor() : m_primary->getColor(), alpha);
  case HairColorSource::Custom:
    return premultiplied(m_config.customColor, alpha);
  }
  return premultiplied(m_primary->getColor(), alpha);
}

void HairNode::redraw()
{
  auto const simToHair = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), this->worldToNodeTransform());
  auto const headToHair = CCAffineTransformConcat(m_head->nodeToWorldTransform(), this->worldToNodeTransform());
  float const hairScale = applyVec({1.f, 0.f}, headToHair).getLength() * this->headUnit();

  float const outline = kOutlineWidth * hairScale;
  auto const color = this->hairColor();
  auto const outlineColor = ccColor4F{0.f, 0.f, 0.f, color.a};
  auto const &strands = m_sim.strands();

  // All outlines first so overlapping locks merge into one hair shape
  for (int pass = m_config.outline ? 0 : 1; pass < 2; ++pass)
  {
    for (size_t s = 0; s < strands.size() && s < m_locks.size(); ++s)
    {
      auto const &lock = m_locks[s];
      float const rootRadius = lock.width * .5f * hairScale;
      auto const lockColor = shaded(color, kBackShade + (1.f - kBackShade) * lock.depth);

      buildCurve(strands[s], simToHair, m_curve);
      int const last = static_cast<int>(m_curve.size()) - 1;

      for (int k = 1; k <= last; ++k)
      {
        float const along = static_cast<float>(k) / static_cast<float>(last);
        float const radius = rootRadius * (1.f - kTipTaper * along);

        if (pass == 0)
          this->drawSegment(m_curve[k - 1], m_curve[k], radius + outline * (1.f - kOutlineTaper * along), outlineColor);
        else
          this->drawSegment(m_curve[k - 1], m_curve[k], radius, lockColor);
      }
    }
  }
}
