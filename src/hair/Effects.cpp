#include "HairNode.hpp"
#include "HairShared.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

using namespace geode::prelude;
using namespace hair;

// ! --- Effects --- !
// Hearts and sparkles, weather (sakura petals, snow, autumn leaves, stars), the sleepy Zzz and the
// reactions (sweat drop on death, the cute death burst, hearts on level complete, sparkles on
// checkpoints). Particles live in sim space and are drawn by a separate node in the sim space, so
// they stay where they appeared while the icon moves on, and keep playing when the player is
// hidden after a death. Sizes are in icon units.

namespace
{
  constexpr float kBlushPopTime = .6f; // s the landing blush takes to fade
  constexpr float kLandPopTime = .35f; // s hats squash and the pet hops after a landing

  // Hearts and sparkles
  constexpr float kParticlesPerSecond = 3.f; // at amount 1
  constexpr int kBurst = 5;
  constexpr float kParticleSize = 2.2f;
  constexpr float kParticleRise = 25.f;  // icon units / s up
  constexpr float kParticleWobble = 8.f; // icon units / s sideways
  constexpr float kParticleDrag = 1.2f;  // 1 / s

  // Weather
  constexpr float kWeatherPerSecond = 4.f; // at amount 1 while moving, a quarter of it standing still
  constexpr float kWeatherFall = 11.f;     // icon units / s down
  constexpr float kWeatherSway = 14.f;     // icon units / s sideways
  constexpr float kWeatherSpread = 24.f;   // icon units around the head where they appear
  constexpr float kPetalSize = 1.9f;
  constexpr float kTrailPerSecond = 22.f; // at full speed
  constexpr float kTrailFullSpeed = 250.f; // icon units / s
  constexpr float kTrailSize = 1.6f;
  constexpr std::array<ccColor3B, 4> kAutumn = {{{232, 128, 58}, {217, 79, 48}, {242, 182, 64}, {184, 105, 46}}};

  // Sleepy
  constexpr float kStillSpeed = 6.f; // icon units / s, slower counts as standing still
  constexpr float kFallAsleep = .7f; // 1 / s
  constexpr float kWakeUp = 3.f;     // 1 / s
  constexpr float kZzzEvery = 1.1f;  // s
  constexpr float kZzzRise = 12.f;   // icon units / s
  constexpr float kZzzSize = 2.4f;
  constexpr float kZzzGrow = 1.6f; // how much bigger a Z gets over its life

  // Reactions
  constexpr ccColor3B kDropColor = {120, 200, 255};
  constexpr float kDropGravity = 70.f; // icon units / s^2
  constexpr int kCelebrateHearts = 10;
  constexpr float kEmoteTime = 2.f;   // s
  constexpr float kEmotePop = .25f;   // s to pop in
  constexpr float kEmoteRadius = 6.f; // icon units
  constexpr float kEmoteHeight = 9.f; // above the head
  constexpr float kEmoteRise = 3.f;   // icon units / s
  constexpr ccColor3B kEmoteHeart = {255, 92, 138};
  constexpr ccColor3B kEmoteInk = {43, 33, 64};
  constexpr ccColor3B kEmoteExclaim = {224, 64, 90};
  constexpr ccColor3B kEmoteQuestion = {79, 163, 224};
  constexpr float kOrbKick = 180.f; // icon units / s the hair is thrown with at kick 1
  constexpr float kOrbPuff = 60.f;
  constexpr int kOrbSparkles = 6;
  constexpr int kCheckpointSparkles = 6;
  constexpr int kDeathPetals = 18;
  constexpr int kDeathHearts = 6;

  // Four pointed sparkle: two thin crossed diamonds
  void drawSparkle(CCDrawNode *node, CCPoint const &center, float size, ccColor4F const &color)
  {
    for (CCPoint const axis : {CCPoint{0.f, 1.f}, CCPoint{1.f, 0.f}})
    {
      CCPoint const side = perpendicular(axis);
      std::array<CCPoint, 4> diamond = {
          center + axis * size,
          center + side * (size * .25f),
          center - axis * size,
          center - side * (size * .25f),
      };
      node->drawPolygon(diamond.data(), 4, color, 0.f, color);
    }
  }

  // A "Z" from three strokes, outlined like the rest of the icon
  void drawZ(CCDrawNode *node, CCPoint const &center, CCPoint const &up, float size, ccColor4F const &color, float outline,
             ccColor4F const &outlineColor)
  {
    CCPoint const right = perpendicular(up) * -1.f;
    CCPoint const topLeft = center + (up - right) * (size * .5f);
    CCPoint const topRight = center + (up + right) * (size * .5f);
    CCPoint const bottomLeft = center - (up + right) * (size * .5f);
    CCPoint const bottomRight = center + (right - up) * (size * .5f);
    float const thickness = size * .13f;
    for (int pass = outline > 0.f ? 0 : 1; pass < 2; ++pass)
    {
      float const radius = pass == 0 ? thickness + outline : thickness;
      auto const passColor = pass == 0 ? outlineColor : color;
      node->drawSegment(topLeft, topRight, radius, passColor);
      node->drawSegment(topRight, bottomLeft, radius, passColor);
      node->drawSegment(bottomLeft, bottomRight, radius, passColor);
    }
  }

  // Snowflake: three strokes through the middle with little branches
  void drawSnowflake(CCDrawNode *node, CCPoint const &center, CCPoint const &up, float size, ccColor4F const &color)
  {
    for (int i = 0; i < 3; ++i)
    {
      CCPoint const arm = rotated(up, radians(60.f * static_cast<float>(i))) * size;
      node->drawSegment(center - arm, center + arm, size * .1f, color);
      for (float end : {-1.f, 1.f})
      {
        CCPoint const tip = center + arm * (.6f * end);
        CCPoint const branch = rotated(arm * end, radians(40.f)) * .3f;
        CCPoint const branch2 = rotated(arm * end, radians(-40.f)) * .3f;
        node->drawSegment(tip, tip + branch, size * .07f, color);
        node->drawSegment(tip, tip + branch2, size * .07f, color);
      }
    }
  }

  // Drop of water, round at the bottom and pointed at the top
  void drawDrop(CCDrawNode *node, CCPoint const &center, CCPoint const &up, float size, ccColor4F const &color, float outline,
                ccColor4F const &outlineColor)
  {
    CCPoint const side = perpendicular(up);
    for (int pass = outline > 0.f ? 0 : 1; pass < 2; ++pass)
    {
      float const grow = pass == 0 ? outline : 0.f;
      auto const passColor = pass == 0 ? outlineColor : color;
      fillCircle(node, center, size * .55f + grow, passColor);
      std::array<CCPoint, 3> tip = {
          center + side * (size * .53f + grow),
          center - side * (size * .53f + grow),
          center + up * (size * 1.25f + grow * 1.5f),
      };
      node->drawPolygon(tip.data(), 3, passColor, 0.f, passColor);
    }
    fillCircle(node, center + up * (size * .1f) - side * (size * .2f), size * .14f, faded({1.f, 1.f, 1.f, 1.f}, .8f * color.a));
  }
}

// ! --- Update --- !

void HairNode::updateDecor(float dt, CCPoint const &headCenter, CCPoint const &up, CCPoint const &across)
{
  if (m_needsReset)
  {
    m_particles.clear();
    m_sparkleTimer = 0.f;
    m_petalTimer = 0.f;
    m_zzzTimer = 0.f;
    m_idleTime = 0.f;
    m_sleepy = 0.f;
    m_blushPop = 0.f;
    m_landPop = 0.f;
    m_haloPosition = this->haloTarget(headCenter);
    m_haloVelocity = CCPoint{};
    return;
  }

  m_blushPop = std::max(0.f, m_blushPop - dt / kBlushPopTime);
  m_landPop = std::max(0.f, m_landPop - dt / kLandPopTime);
  this->updateHalo(dt, headCenter);

  // Falls asleep after standing still for a while, wakes up as soon as it moves
  float const speed = m_headVelocity.getLength() / std::max(m_simScale, .0001f);
  m_idleTime = speed < kStillSpeed ? m_idleTime + dt : 0.f;
  bool const asleep = m_config.sleepy && m_idleTime > m_config.sleepyDelay;
  m_sleepy = approach(m_sleepy, asleep ? 1.f : 0.f, (asleep ? kFallAsleep : kWakeUp) * dt);

  this->updateParticles(dt);

  if (m_config.sparkles != SparkleStyle::None && m_config.sparkleRate > 0.f)
  {
    m_sparkleTimer += dt * m_config.sparkleRate * kParticlesPerSecond;
    for (; m_sparkleTimer >= 1.f; m_sparkleTimer -= 1.f)
      this->spawnParticle(ParticleKind::Heart, headCenter, false);
  }

  // A trail of little hearts, stars or sparkles left behind, more of them the faster you go
  if (m_config.trail != TrailStyle::None && m_config.trail != TrailStyle::Ribbon)
  {
    float const pace = std::clamp(speed / kTrailFullSpeed, 0.f, 1.f);
    m_trailTimer += dt * kTrailPerSecond * pace;
    for (; m_trailTimer >= 1.f; m_trailTimer -= 1.f)
      this->spawnParticle(ParticleKind::Trail, headCenter, false);
  }

  if (m_config.petals && m_config.petalAmount > 0.f)
  {
    auto const kind = m_config.weather == WeatherStyle::Snow     ? ParticleKind::Snow
                      : m_config.weather == WeatherStyle::Leaves ? ParticleKind::Leaf
                      : m_config.weather == WeatherStyle::Stars  ? ParticleKind::Star
                                                                 : ParticleKind::Petal;
    m_petalTimer += dt * m_config.petalAmount * kWeatherPerSecond * (.25f + .75f * m_motion);
    for (; m_petalTimer >= 1.f; m_petalTimer -= 1.f)
      this->spawnParticle(kind, headCenter, false);
  }

  if (m_sleepy > .6f)
  {
    m_zzzTimer += dt;
    if (m_zzzTimer >= kZzzEvery)
    {
      m_zzzTimer = 0.f;
      this->spawnParticle(ParticleKind::Zzz, headCenter, false);
    }
  }
}

void HairNode::updateParticles(float dt)
{
  m_particlesFrame = CCDirector::sharedDirector()->getTotalFrames();

  CCPoint const worldUp = m_gravityDir ? m_gravityDir() * -1.f : CCPoint{0.f, 1.f};
  CCPoint const sideways = perpendicular(worldUp);
  for (auto &particle : m_particles)
  {
    particle.age += dt;
    particle.rotation += particle.spin * dt;
    switch (particle.kind)
    {
    case ParticleKind::Petal:
    case ParticleKind::Snow:
    case ParticleKind::Leaf:
    case ParticleKind::Star:
    {
      // Weather flutters down, swaying from side to side
      CCPoint const sway = sideways * (std::sin(particle.age * 3.f + particle.phase) * kWeatherSway * m_simScale);
      particle.position = particle.position + (particle.velocity + sway) * dt;
      break;
    }
    case ParticleKind::Zzz:
      particle.position = particle.position + particle.velocity * dt;
      break;
    case ParticleKind::Trail:
      // Stays where it was left, drifting a little
      particle.position = particle.position + particle.velocity * dt;
      particle.velocity = particle.velocity * std::max(0.f, 1.f - 3.f * dt);
      break;
    case ParticleKind::Drop:
      particle.velocity = particle.velocity - worldUp * (kDropGravity * m_simScale * dt);
      particle.position = particle.position + particle.velocity * dt;
      break;
    default:
    {
      // Float up with a little sway and slow down
      CCPoint const sway = sideways * (std::sin(particle.age * 5.f + particle.phase) * kParticleWobble * m_simScale);
      particle.position = particle.position + (particle.velocity + sway) * dt;
      particle.velocity = particle.velocity * std::max(0.f, 1.f - kParticleDrag * dt);
      break;
    }
    }
  }
  std::erase_if(m_particles, [](Particle const &particle)
                { return particle.age >= particle.life; });
}

// ! --- Events --- !

void HairNode::onLanded(CCPoint const &headCenter, CCPoint const &, CCPoint const &)
{
  m_idleTime = 0.f;
  m_landPop = 1.f;

  if (m_config.blush && m_config.blushPop)
    m_blushPop = 1.f;

  // The pet hops when you land
  CCPoint const worldUp = m_gravityDir ? m_gravityDir() * -1.f : CCPoint{0.f, 1.f};
  m_petVelocity = m_petVelocity + worldUp * (60.f * m_simScale);

  if (m_config.sparkles != SparkleStyle::None && m_config.sparkleOnLanding)
    this->burst(ParticleKind::Heart, kBurst, headCenter);
}

void HairNode::burst(ParticleKind kind, int count, CCPoint const &headCenter)
{
  for (int i = 0; i < count; ++i)
    this->spawnParticle(kind, headCenter, true);
}

void HairNode::celebrate()
{
  if (m_config.reactions)
    this->burst(ParticleKind::Heart, kCelebrateHearts, m_frameParams.headCenter);
  this->petReact(PetMood::Happy, 3.f);
}

void HairNode::checkpointReached()
{
  if (m_config.reactions)
    this->burst(ParticleKind::Sparkle, kCheckpointSparkles, m_frameParams.headCenter);
  this->petReact(PetMood::Happy, 1.5f);
}

void HairNode::boosted(CCPoint const &up)
{
  if (!m_config.orbReaction || m_needsReset || !this->isActive())
    return;

  // The hair flies up with the icon and puffs out, the bows and wings get thrown too
  float const kick = m_config.orbKick;
  m_sim.kick(up * (kOrbKick * kick * m_simScale), m_frameParams.headCenter, kOrbPuff * kick * m_simScale);
  m_bowWobbleSpeed += (m_random() % 2 == 0 ? -1.f : 1.f) * 260.f * kick;
  if (m_config.wings != WingStyle::None)
    m_wingSpeed += 300.f * kick;
  this->burst(ParticleKind::Sparkle, kOrbSparkles, m_frameParams.headCenter);
}

void HairNode::onDeath()
{
  std::uniform_real_distribution<float> random(0.f, 1.f);
  CCPoint const center = m_frameParams.headCenter;
  CCPoint const worldUp = m_gravityDir ? m_gravityDir() * -1.f : CCPoint{0.f, 1.f};
  CCPoint const right = perpendicular(worldUp) * -1.f;
  float const unit = m_simScale;

  // A sweat drop by the head and two little tears
  if (m_config.reactions)
  {
    auto addDrop = [&](CCPoint const &position, CCPoint const &velocity, float size)
    {
      Particle drop;
      drop.kind = ParticleKind::Drop;
      drop.position = position;
      drop.velocity = velocity;
      drop.life = 1.1f;
      drop.size = size;
      m_particles.push_back(drop);
    };
    addDrop(center + (worldUp * .9f + right * .9f) * (kHeadRadius * unit), (worldUp * 25.f + right * 10.f) * unit, 1.4f);
    addDrop(center + right * (-6.f * unit), (worldUp * 30.f - right * 25.f) * unit, .8f);
    addDrop(center + right * (6.f * unit), (worldUp * 30.f + right * 25.f) * unit, .8f);
  }

  // Cute death: the hair bursts into petals of its own color, and a few hearts
  if (m_config.cuteDeath)
  {
    auto const &strands = m_sim.strands();
    auto const hair = this->hairColor();
    for (int i = 0; i < kDeathPetals && !strands.empty(); ++i)
    {
      auto const &strand = strands[static_cast<size_t>(random(m_random) * static_cast<float>(strands.size() - 1) + .5f)];
      CCPoint const from = strand[static_cast<size_t>(random(m_random) * static_cast<float>(strand.size() - 1) + .5f)];
      CCPoint const out = normalized(from - center, worldUp);

      Particle petal;
      petal.kind = ParticleKind::Petal;
      petal.tinted = true;
      petal.color = shaded(hair, .85f + .3f * random(m_random));
      petal.position = from;
      petal.velocity = (out * (40.f + 40.f * random(m_random)) + worldUp * 20.f) * unit;
      petal.life = 1.2f + .6f * random(m_random);
      petal.size = .8f + .5f * random(m_random);
      petal.rotation = random(m_random) * 2.f * kPi;
      petal.spin = (random(m_random) * 2.f - 1.f) * 6.f;
      m_particles.push_back(petal);
    }
    this->burst(ParticleKind::Heart, kDeathHearts, center);
  }

  if (m_particles.size() > m_config.maxParticles)
    m_particles.erase(m_particles.begin(), m_particles.end() - m_config.maxParticles);
}

// ! --- Spawning --- !

void HairNode::spawnParticle(ParticleKind kind, CCPoint const &headCenter, bool burst)
{
  if (m_particles.size() >= m_config.maxParticles)
    return;

  std::uniform_real_distribution<float> random(0.f, 1.f);
  CCPoint const worldUp = m_gravityDir ? m_gravityDir() * -1.f : CCPoint{0.f, 1.f};
  CCPoint const sideways = perpendicular(worldUp);
  float const unit = m_simScale;

  Particle particle;
  particle.kind = kind;
  particle.phase = random(m_random) * 2.f * kPi;

  switch (kind)
  {
  case ParticleKind::Heart:
  case ParticleKind::Sparkle:
  {
    // Hearts or sparkles (the setting decides for hearts); on the top half of the head going up,
    // a burst pops them out to the sides
    if (kind == ParticleKind::Heart)
    {
      bool const sparkle = m_config.sparkles == SparkleStyle::Sparkles ||
                           (m_config.sparkles == SparkleStyle::Both && random(m_random) < .5f);
      particle.kind = sparkle ? ParticleKind::Sparkle : ParticleKind::Heart;
    }
    CCPoint const radial = rotated(worldUp, radians(-100.f + 200.f * random(m_random)));
    particle.position = headCenter + radial * (this->headEdge(radial) * 1.05f);
    float const rise = kParticleRise * (.7f + .6f * random(m_random));
    float const push = burst ? 30.f + 20.f * random(m_random) : 4.f;
    particle.velocity = (worldUp * rise + radial * push) * unit;
    particle.life = 1.f + .5f * random(m_random);
    particle.size = (.8f + .4f * random(m_random)) * m_config.sparkleSize;
    break;
  }
  case ParticleKind::Trail:
  {
    // Around the middle of the head, they stay behind as it moves on
    CCPoint const offset = rotated(worldUp, random(m_random) * 2.f * kPi) * (kHeadRadius * .5f * random(m_random) * unit);
    particle.position = headCenter + offset;
    particle.velocity = offset * 1.5f;
    particle.life = .3f + m_config.trailLength / 100.f;
    particle.size = .7f + .5f * random(m_random);
    particle.rotation = (random(m_random) - .5f) * .8f;
    break;
  }
  case ParticleKind::Petal:
  case ParticleKind::Snow:
  case ParticleKind::Leaf:
  case ParticleKind::Star:
  {
    // Above and around the head, drifting down with a slow spin
    float const height = kHeadRadius + 12.f * random(m_random);
    float const side = (random(m_random) * 2.f - 1.f) * kWeatherSpread;
    float const fall = kind == ParticleKind::Snow ? .6f : 1.f;
    particle.position = headCenter + (worldUp * height + sideways * side) * unit;
    particle.velocity = (worldUp * (-kWeatherFall * fall * (.7f + .6f * random(m_random))) +
                         sideways * ((random(m_random) * 2.f - 1.f) * 5.f)) *
                        unit;
    particle.life = 2.2f + .8f * random(m_random);
    particle.size = .8f + .4f * random(m_random);
    particle.rotation = random(m_random) * 2.f * kPi;
    particle.spin = (random(m_random) * 2.f - 1.f) * (kind == ParticleKind::Snow ? 1.f : 3.f);
    if (kind == ParticleKind::Leaf)
    {
      particle.tinted = true;
      particle.color = premultiplied(kAutumn[static_cast<size_t>(random(m_random) * 3.99f)], 1.f);
    }
    break;
  }
  case ParticleKind::Zzz:
  {
    // From the top of the head, drifting up and to the side, growing
    CCPoint const side = sideways * -1.f;
    particle.position = headCenter + (worldUp * kHeadRadius * 1.05f + side * kHeadRadius * .45f) * unit;
    particle.velocity = (worldUp * kZzzRise + side * 6.f) * unit;
    particle.life = 2.2f;
    particle.size = 1.f;
    particle.rotation = radians(-12.f);
    break;
  }
  case ParticleKind::Drop:
    break;
  }

  m_particles.push_back(particle);
}

// ! --- Drawing --- !

void HairNode::drawParticles(CCDrawNode *node)
{
  if (m_particles.empty())
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const alpha = m_head->getDisplayedOpacity() / 255.f;
  float const outline = m_config.outline ? kOutlineWidth * scale : 0.f;

  auto const sparkleColor = m_config.sparkleColorSource == HairColorSource::Hair
                                ? this->hairColor()
                                : this->sourceColor(m_config.sparkleColorSource, m_config.sparkleColor);
  auto const weatherColor = premultiplied(m_config.petalColor, alpha);
  auto const white = ccColor4F{alpha, alpha, alpha, alpha};
  auto const black = ccColor4F{0.f, 0.f, 0.f, alpha};
  CCPoint const up = normalized(applyVec(m_gravityDir ? m_gravityDir() * -1.f : CCPoint{0.f, 1.f}, simToNode), {0.f, 1.f});

  for (auto const &particle : m_particles)
  {
    // Fade in quickly, fade out over the last part of the life
    float const t = particle.age / particle.life;
    float const fade = std::min(1.f, particle.age / .15f) * (t > .6f ? (1.f - t) / .4f : 1.f);
    CCPoint const center = CCPointApplyAffineTransform(particle.position, simToNode);
    CCPoint const turned = rotated(up, particle.rotation);
    auto const own = faded(particle.tinted ? particle.color : weatherColor, fade);

    switch (particle.kind)
    {
    case ParticleKind::Heart:
    {
      float const size = particle.size * kParticleSize * scale * (.7f + .3f * std::sin(kPi * t));
      drawHeart(node, center, up, size, faded(sparkleColor, fade));
      fillCircle(node, center + up * (size * .4f) - perpendicular(up) * (size * .5f), size * .14f, faded(white, .6f * fade));
      break;
    }
    case ParticleKind::Sparkle:
    {
      float const size = particle.size * kParticleSize * scale * (.7f + .3f * std::sin(kPi * t));
      drawSparkle(node, center, size * 1.2f, faded(sparkleColor, fade));
      fillCircle(node, center, size * .18f, faded(white, .8f * fade));
      break;
    }
    case ParticleKind::Petal:
      // A sakura petal is a little heart with its point towards the stem
      drawHeart(node, center, turned, particle.size * kPetalSize * scale, own);
      break;
    case ParticleKind::Snow:
      drawSnowflake(node, center, turned, particle.size * kPetalSize * scale, faded(white, .95f * fade));
      break;
    case ParticleKind::Leaf:
    {
      float const size = particle.size * kPetalSize * scale;
      fillEllipse(node, center, turned, size, size * .45f, own);
      node->drawSegment(center - turned * (size * .9f), center + turned * (size * .9f), size * .06f, shaded(own, .7f));
      break;
    }
    case ParticleKind::Star:
    {
      float const size = particle.size * kPetalSize * scale;
      drawStar(node, center, turned, size, size * .45f, own);
      break;
    }
    case ParticleKind::Zzz:
    {
      float const size = kZzzSize * scale * (1.f + kZzzGrow * t);
      drawZ(node, center, turned, size, faded(white, fade), outline, faded(black, fade));
      break;
    }
    case ParticleKind::Drop:
      drawDrop(node, center, up, particle.size * kParticleSize * scale, premultiplied(kDropColor, alpha * fade), outline, faded(black, fade));
      break;
    case ParticleKind::Trail:
    {
      // Shrinks away instead of fading
      float const size = particle.size * kTrailSize * scale * (1.f - t);
      auto const color = m_config.trailColorSource == HairColorSource::Hair
                             ? this->hairColor()
                             : this->sourceColor(m_config.trailColorSource, m_config.trailColor);
      if (m_config.trail == TrailStyle::Hearts)
        drawHeart(node, center, turned, size, color);
      else if (m_config.trail == TrailStyle::Stars)
        drawStar(node, center, turned, size * 1.2f, size * .55f, color);
      else
        drawSparkle(node, center, size * 1.3f, color);
      break;
    }
    }
  }
}

// ! --- Effects node --- !

void HairNode::visitEffects(CCDrawNode *node)
{
  node->clear();
  unsigned const frame = CCDirector::sharedDirector()->getTotalFrames();

  // Death: the reactions start here, the hair node itself may be hidden with the player.
  // Only when the rig was showing, not in a game mode with the customization turned off
  bool const dead = m_isDeadFn && m_isDeadFn();
  if (dead && !m_wasDead)
  {
    m_diedActive = frame - m_aliveFrame < 5;
    if (m_diedActive)
    {
      this->onDeath();
      this->petReact(PetMood::Sad, 0.f);
    }
  }
  m_wasDead = dead;

  if (!dead && !this->isActive())
  {
    m_particles.clear();
    return;
  }

  // Nobody updated the particles and the pet this frame (the player is hidden): keep them going
  float const dt = CCDirector::sharedDirector()->getDeltaTime();
  if (m_particlesFrame != frame)
    this->updateParticles(dt);

  if (m_emote)
  {
    m_emoteAge += dt;
    if (m_emoteAge >= kEmoteTime || dead)
      m_emote.reset();
  }

  // The pet floats in the sim space too, so it stays with you after a death
  if (m_config.pet != PetStyle::None && (!dead || m_diedActive))
  {
    if (m_petFrame != frame)
      this->updatePetAlone(dt);
    this->drawPet(node);
  }

  this->drawParticles(node);
  this->drawEmote(node);
}

// ! --- Emotes --- !

void HairNode::drawEmote(CCDrawNode *node)
{
  if (!m_emote)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const alpha = m_head->getDisplayedOpacity() / 255.f;
  float const outline = kOutlineWidth * scale;

  // Pops in with a little overshoot, floats up slowly, fades out at the end
  float const t = m_emoteAge;
  float const pop = t < kEmotePop ? std::sin(t / kEmotePop * kPi * .75f) / std::sin(kPi * .75f) : 1.f;
  float const fade = std::clamp((kEmoteTime - t) / .3f, 0.f, 1.f) * alpha;
  if (pop <= 0.f || fade <= 0.f)
    return;

  CCPoint const worldUp = m_gravityDir ? m_gravityDir() * -1.f : CCPoint{0.f, 1.f};
  CCPoint const up = normalized(applyVec(worldUp, simToNode), {0.f, 1.f});
  CCPoint const right = perpendicular(up) * -1.f;
  CCPoint const anchor = CCPointApplyAffineTransform(
      m_frameParams.headCenter + worldUp * ((kHeadRadius + kEmoteHeight + t * kEmoteRise) * m_simScale), simToNode);
  float const radius = kEmoteRadius * scale * pop;
  CCPoint const center = anchor + right * (radius * .35f);

  auto const white = ccColor4F{fade, fade, fade, fade};
  auto const black = ccColor4F{0.f, 0.f, 0.f, fade};

  // The bubble with its little tail pointing at the head
  std::array<CCPoint, 3> tail = {center - up * (radius * .55f) - right * (radius * .55f), center - up * (radius * .25f) - right * (radius * .05f),
                                 center - up * (radius * 1.25f) - right * (radius * .75f)};
  std::array<CCPoint, 3> tailOutline = {tail[0] - right * outline, tail[1] + right * outline, tail[2] - up * outline - right * outline};
  node->drawPolygon(tailOutline.data(), 3, black, 0.f, black);
  fillCircle(node, center, radius + outline, black);
  node->drawPolygon(tail.data(), 3, white, 0.f, white);
  fillCircle(node, center, radius, white);

  float const r = radius;
  switch (*m_emote)
  {
  case Emote::Heart:
    drawHeart(node, center - up * (r * .05f), up, r * .5f, premultiplied(kEmoteHeart, fade));
    break;
  case Emote::Note:
  {
    // A note: a tilted head, a stem and a flag
    auto const ink = premultiplied(kEmoteInk, fade);
    CCPoint const head = center - up * (r * .3f) - right * (r * .15f);
    fillEllipse(node, head, rotated(right, .45f), r * .22f, r * .15f, ink);
    CCPoint const stemTop = head + right * (r * .19f) + up * (r * .72f);
    node->drawSegment(head + right * (r * .19f), stemTop, r * .06f, ink);
    node->drawSegment(stemTop, stemTop + right * (r * .28f) - up * (r * .22f), r * .07f, ink);
    break;
  }
  case Emote::Exclaim:
  {
    auto const red = premultiplied(kEmoteExclaim, fade);
    node->drawSegment(center + up * (r * .45f), center - up * (r * .12f), r * .12f, red);
    fillCircle(node, center - up * (r * .43f), r * .12f, red);
    break;
  }
  case Emote::Question:
  {
    // A hook: three quarters of a circle, then down to the dot
    auto const blue = premultiplied(kEmoteQuestion, fade);
    CCPoint const middle = center + up * (r * .2f);
    float const bend = r * .24f;
    CCPoint previous = middle + rotated(up, radians(60.f)) * bend;
    for (int i = 1; i <= 8; ++i)
    {
      CCPoint const point = middle + rotated(up, radians(60.f - 210.f * static_cast<float>(i) / 8.f)) * bend;
      node->drawSegment(previous, point, r * .09f, blue);
      previous = point;
    }
    node->drawSegment(previous, center - up * (r * .15f), r * .09f, blue);
    fillCircle(node, center - up * (r * .42f), r * .11f, blue);
    break;
  }
  }
}
