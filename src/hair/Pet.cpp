#include "HairNode.hpp"
#include "HairShared.hpp"
#include "LevelQuery.hpp"

#include <array>
#include <cmath>
#include <random>

using namespace geode::prelude;
using namespace hair;

// ! --- Pet --- !
// A tiny cat, ghost, bird, bunny or slime floating behind and above the icon on a lazy spring.
// It bobs, looks where you go, blinks now and then and hops when you land. With moods it falls
// asleep with the icon, cheers on checkpoints and level completes and is sad after a death.
// It's drawn by the effects node in the sim space, so it stays when the player is hidden.
// Sizes are in icon units.

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
  constexpr ccColor3B kTear = {120, 200, 255};
  constexpr float kPetSleepSink = 4.f; // icon units lower while asleep
  constexpr float kPetHop = 90.f;      // icon units / s up when it cheers
  constexpr float kPetZEvery = 2.2f;   // s between the little "z"s of a sleeping pet
  constexpr float kPetTiltTime = 1.2f;  // s of the puzzled head tilt
  constexpr float kPetTiltAngle = 18.f; // degrees

  // The running pet, in level units (a block is 30)
  constexpr float kRunGravity = 1800.f;  // units / s^2
  constexpr float kRunJump = 560.f;      // units / s up, clears a spike
  constexpr float kRunFollow = 45.f;     // units behind you
  constexpr float kRunSpeed = 420.f;     // units / s, more when you're fast
  constexpr float kRunSee = 55.f;        // units ahead it watches for spikes and gaps
  constexpr float kRunLost = 450.f;      // further away than this it gives up and comes back
  constexpr float kRunHop = 8.f;         // 1 / s, how fast it hops onto your head
  constexpr float kLevelFloor = 90.f;    // the top of the ground of a level
}

CCPoint HairNode::petTarget(CCPoint const &headCenter) const
{
  // Behind the head (against the movement) and above it, in world up
  CCPoint const worldUp = m_gravityDir ? m_gravityDir() * -1.f : CCPoint{0.f, 1.f};
  CCPoint worldUpFrame;
  CCPoint worldBack;
  this->gravityAxes(worldUpFrame, worldBack);
  CCPoint const back = normalized(worldBack, perpendicular(worldUp));
  // Sleeps a bit lower with a slower, smaller bob
  float const sleepy = m_config.petMoods ? m_sleepy : 0.f;
  float const bob = std::sin(m_petClock * (2.5f - 1.3f * sleepy)) * kPetBob * (1.f - .6f * sleepy);
  return headCenter + worldUp * ((kHeadRadius + kPetHeight + bob - kPetSleepSink * sleepy) * m_simScale) +
         back * ((kHeadRadius + m_config.petDistance) * m_simScale);
}

void HairNode::updatePet(float dt, CCPoint const &headCenter)
{
  if (m_config.pet == PetStyle::None)
    return;

  // Only in a level there are blocks to run on
  if (m_config.petBehavior == PetBehavior::Run && m_focusFn)
    this->updatePetRunner(dt, headCenter);
  else
  {
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
    m_petGrounded = false;
    m_petRiding = false;
  }

  // Back alive: no more tears
  if (m_petMood == PetMood::Sad)
    m_petMood = PetMood::Normal;
  m_petClock += dt;
  m_petTilt = std::max(0.f, m_petTilt - dt);
  m_petMoodTime -= dt;
  if (m_petMood == PetMood::Happy && m_petMoodTime <= 0.f)
    m_petMood = PetMood::Normal;
  m_petBlink -= dt;
  if (m_petBlink < -kPetBlink)
    m_petBlink = 2.f + 3.f * std::uniform_real_distribution<float>(0.f, 1.f)(m_random);
  m_petFrame = CCDirector::sharedDirector()->getTotalFrames();
}

void HairNode::updatePetRunner(float dt, CCPoint const &headCenter)
{
  CCPoint const down = m_gravityDir ? m_gravityDir() : CCPoint{0.f, -1.f};
  bool const flipped = down.y > 0.f;
  float const upSign = flipped ? -1.f : 1.f;
  float const body = kPetBody * m_config.petSize * m_simScale;
  float const dir = m_facing >= 0.f ? 1.f : -1.f;

  // In the flying modes it rides on your head
  auto const mode = m_gameModeFn ? m_gameModeFn() : GameMode::Cube;
  bool const flying = mode == GameMode::Ship || mode == GameMode::Ufo || mode == GameMode::Wave ||
                      mode == GameMode::Swing || mode == GameMode::Jetpack;
  CCPoint const seat = headCenter + CCPoint{0.f, upSign * (this->headEdge({0.f, upSign}) + body)};

  // Respawn, teleport or lost: back next to you
  float const away = m_petPosition.getDistance(headCenter);
  if (m_needsReset || away > kRunLost * m_simScale)
  {
    m_petPosition = flying ? seat : headCenter - CCPoint{dir * kRunFollow * m_simScale, 0.f};
    m_petVelocity = CCPoint{};
    m_petGrounded = false;
    m_petRiding = flying;
    return;
  }

  if (flying || m_petRiding)
  {
    // Hops up and sits; jumps back down once you're back on the ground
    if (!flying)
    {
      m_petRiding = false;
      m_petVelocity = CCPoint{-dir * 120.f * m_simScale, upSign * kRunJump * .6f * m_simScale};
    }
    else
    {
      m_petRiding = true;
      float const hop = std::min(1.f, kRunHop * dt);
      m_petPosition = m_petPosition + (seat - m_petPosition) * hop;
      m_petVelocity = CCPoint{};
      m_petGrounded = false;
      return;
    }
  }

  // Runs to a spot a little behind you, faster the further it is
  float const targetX = headCenter.x - dir * kRunFollow * m_simScale;
  float const maxRun = std::max(kRunSpeed * m_simScale, std::abs(m_headVelocity.x) * 1.3f);
  float const runX = std::clamp((targetX - m_petPosition.x) * 6.f, -maxRun, maxRun);
  m_petVelocity.x = m_petVelocity.x + (runX - m_petVelocity.x) * std::min(1.f, 12.f * dt);
  m_petVelocity.y -= upSign * kRunGravity * m_simScale * dt;

  CCPoint const next = m_petPosition + m_petVelocity * dt;
  float const feetY = next.y - upSign * body;
  // The floor of the level isn't an object: blocks or the floor, whichever is higher
  auto ground = level::groundBelow(next.x, m_petPosition.y, body * .6f, 400.f * m_simScale, flipped);
  if (!flipped)
    ground = std::max(ground.value_or(kLevelFloor), kLevelFloor);
  if (ground && (flipped ? feetY >= *ground : feetY <= *ground) && m_petVelocity.y * upSign <= 0.f)
  {
    m_petPosition = CCPoint{next.x, *ground + upSign * body};
    m_petVelocity.y = 0.f;
    m_petGrounded = true;
  }
  else
  {
    m_petPosition = next;
    m_petGrounded = false;
  }

  // On the ground: jumps a spike or a gap ahead, or after you when you're up high
  if (m_petGrounded)
  {
    float const going = m_petVelocity.x >= 0.f ? 1.f : -1.f;
    CCPoint const ahead{going, 0.f};
    bool const spike = level::nearestHazard(m_petPosition, ahead, kRunSee * m_simScale, body * 1.5f).has_value();
    auto groundAhead = level::groundBelow(m_petPosition.x + going * body * 2.f, m_petPosition.y, body * .5f,
                                          body * 3.f, flipped);
    if (!flipped && m_petPosition.y - body <= kLevelFloor + 1.f)
      groundAhead = kLevelFloor;
    bool const gap = !groundAhead && std::abs(m_petVelocity.x) > 40.f * m_simScale;
    bool const climb = (headCenter.y - m_petPosition.y) * upSign > 45.f * m_simScale &&
                       std::abs(headCenter.x - m_petPosition.x) < 90.f * m_simScale;
    if (spike || gap || climb)
    {
      m_petVelocity.y = upSign * kRunJump * m_simScale;
      m_petGrounded = false;
    }
  }

  // Feet patter while running
  if (m_petGrounded)
    m_petRunPhase += std::abs(m_petVelocity.x) / std::max(body, .001f) * dt * 1.6f;
}

void HairNode::updatePetAlone(float dt)
{
  m_petFrame = CCDirector::sharedDirector()->getTotalFrames();
  if (dt <= 0.f)
    return;

  // No head to follow: settles where it is and sinks a little, sad
  m_petVelocity = m_petVelocity * std::exp(-kPetDamping * dt);
  CCPoint const worldUp = m_gravityDir ? m_gravityDir() * -1.f : CCPoint{0.f, 1.f};
  m_petVelocity = m_petVelocity - worldUp * (6.f * m_simScale * dt);
  m_petPosition = m_petPosition + m_petVelocity * dt;
  m_petClock += dt;
  m_petBlink -= dt;
  if (m_petBlink < -kPetBlink)
    m_petBlink = 2.f + 3.f * std::uniform_real_distribution<float>(0.f, 1.f)(m_random);
}

void HairNode::emote(Emote emote)
{
  m_emote = emote;
  m_emoteAge = 0.f;

  // The pet answers: hearts make it happy, a question makes it tilt its head
  if (emote == Emote::Heart)
    this->petReact(PetMood::Happy, 1.5f);
  else if (emote == Emote::Question && m_config.pet != PetStyle::None && m_config.petMoods)
    m_petTilt = kPetTiltTime;
}

void HairNode::petReact(PetMood mood, float duration)
{
  if (m_config.pet == PetStyle::None || !m_config.petMoods)
    return;

  m_petMood = mood;
  m_petMoodTime = duration;
  if (mood != PetMood::Happy)
    return;

  // A hop and a few little hearts
  CCPoint const worldUp = m_gravityDir ? m_gravityDir() * -1.f : CCPoint{0.f, 1.f};
  m_petVelocity = m_petVelocity + worldUp * (kPetHop * m_simScale);
  std::uniform_real_distribution<float> random(0.f, 1.f);
  for (int i = 0; i < 3; ++i)
  {
    Particle heart;
    heart.kind = ParticleKind::Heart;
    heart.position = m_petPosition + rotated(worldUp, radians(-50.f + 50.f * static_cast<float>(i))) * (kPetBody * m_simScale);
    heart.velocity = (worldUp * (20.f + 10.f * random(m_random)) + perpendicular(worldUp) * (static_cast<float>(i - 1) * 12.f)) * m_simScale;
    heart.life = 1.f + .3f * random(m_random);
    heart.size = .6f;
    heart.phase = random(m_random) * 2.f * kPi;
    heart.tinted = true;
    heart.color = premultiplied(kPetBlush, 1.f);
    m_particles.push_back(heart);
  }
}

void HairNode::drawPet(CCDrawNode *node)
{
  if (m_config.pet == PetStyle::None)
    return;

  auto const simToNode = CCAffineTransformConcat(m_simSpace->nodeToWorldTransform(), node->worldToNodeTransform());
  auto const headToNode = CCAffineTransformConcat(m_head->nodeToWorldTransform(), node->worldToNodeTransform());
  float const scale = applyVec({1.f, 0.f}, headToNode).getLength() * this->headUnit();
  float const alpha = this->drawAlpha();
  float const outline = m_config.outline ? kOutlineWidth * scale : 0.f;
  auto const outlineColor = this->ink(alpha);

  auto color = this->sourceColor(m_config.petColorSource, m_config.petColor);
  CCPoint const center = CCPointApplyAffineTransform(m_petPosition, simToNode);
  // Tilts its head when puzzled
  float const tilt = m_petTilt > 0.f ? std::sin(kPi * std::min(1.f, (kPetTiltTime - m_petTilt) * 3.f)) * kPetTiltAngle : 0.f;
  CCPoint const up = rotated(normalized(applyVec(m_gravityDir ? m_gravityDir() * -1.f : CCPoint{0.f, 1.f}, simToNode), {0.f, 1.f}),
                             radians(tilt));
  CCPoint const right = perpendicular(up) * -1.f;
  float const face = m_facing >= 0.f ? 1.f : -1.f; // looks where you go
  float const r = kPetBody * m_config.petSize * scale;

  auto body = [&](ccColor4F const &fill)
  {
    if (outline > 0.f)
      fillCircle(node, center, r + outline, outlineColor);
    fillCircle(node, center, r, fill);
  };

  // Little feet under a running pet (the ghost and the slime have none)
  if (m_config.petBehavior == PetBehavior::Run && m_config.pet != PetStyle::Ghost && m_config.pet != PetStyle::Slime &&
      (m_petGrounded || m_petRiding))
  {
    for (float side : {-1.f, 1.f})
    {
      float const step = m_petGrounded ? std::sin(m_petRunPhase + (side > 0.f ? kPi : 0.f)) : 0.f;
      CCPoint const foot = center - up * (r * (.9f - .12f * std::max(0.f, step))) + right * (side * r * .4f + step * r * .25f);
      if (outline > 0.f)
        fillEllipse(node, foot, right, r * .3f + outline, r * .18f + outline, outlineColor);
      fillEllipse(node, foot, right, r * .3f, r * .18f, shaded(color, .85f));
    }
  }

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
        CCPoint const point = center - right * (face * (r * .8f + t * r * .3f)) + up * (std::sin(m_petClock * 3.f + t * .6f) * r * .25f + t * r * .12f);
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
    CCPoint const wing = rotated(right * -face, radians(std::sin(m_petClock * 12.f) * 25.f));
    fillEllipse(node, center - right * (face * r * .1f) - up * (r * .05f), wing, r * .55f, r * .3f, shaded(color, .85f), outline * .8f, outlineColor);
    std::array<CCPoint, 3> beak = {center + right * (face * r * .85f) + up * (r * .18f), center + right * (face * r * .85f) - up * (r * .12f),
                                   center + right * (face * r * 1.35f) + up * (r * .03f)};
    node->drawPolygon(beak.data(), 3, premultiplied(kBeak, alpha), outline * .8f, outlineColor);
    break;
  }
  case PetStyle::Bunny:
  {
    // Long ears that sway and lag behind when it moves (and droop when it's sad or sleepy), a cotton tail
    CCPoint const velocity = applyVec(m_petVelocity, simToNode);
    float const lean = std::clamp(velocity.dot(right) / (60.f * scale + .001f), -1.f, 1.f) * 25.f;
    float const droop = m_config.petMoods ? std::max(m_sleepy, m_petMood == PetMood::Sad ? 1.f : 0.f) * 40.f : 0.f;
    CCPoint const tail = center - right * (face * r * .95f) - up * (r * .35f);
    if (outline > 0.f)
      fillCircle(node, tail, r * .32f + outline, outlineColor);
    fillCircle(node, tail, r * .32f, mixedWhite(color, .6f));
    for (float side : {-1.f, 1.f})
    {
      CCPoint const dir = rotated(up, radians(-side * (12.f + droop) + lean + std::sin(m_petClock * 2.2f + side) * 6.f));
      CCPoint const ear = center + up * (r * .6f) + right * (side * r * .32f) + dir * (r * .75f);
      fillEllipse(node, ear, dir, r * .8f, r * .26f, color, outline, outlineColor);
      fillEllipse(node, ear + dir * (r * .05f), dir, r * .55f, r * .12f, premultiplied(kInnerEar, alpha));
    }
    body(color);
    break;
  }
  case PetStyle::Slime:
  {
    // A wobbly drop of jelly: stretches when it rises, squashes when it falls
    CCPoint const velocity = applyVec(m_petVelocity, simToNode);
    float const stretch = std::clamp(velocity.dot(up) / (80.f * scale + .001f), -.25f, .25f) + std::sin(m_petClock * 4.f) * .05f;
    float const tall = r * (.85f + stretch);
    float const wide = r * (1.1f - stretch * .6f);
    CCPoint const middle = center - up * (r * .1f);
    auto const jelly = faded(color, .88f);
    fillCircle(node, middle + up * (tall * .92f) + right * (face * wide * .12f), r * .18f + outline, outlineColor);
    fillEllipse(node, middle, up, tall, wide, jelly, outline, outlineColor);
    fillCircle(node, middle + up * (tall * .92f) + right * (face * wide * .12f), r * .18f, jelly);
    fillEllipse(node, middle + up * (tall * .45f) - right * (wide * .45f), rotated(up, .5f), tall * .22f, wide * .12f,
                ccColor4F{.6f * alpha, .6f * alpha, .6f * alpha, .6f * alpha});
    break;
  }
  case PetStyle::None:
    break;
  }

  // Face: eyes looking where you go and blush. Closed while blinking or asleep, ^^ when happy,
  // sad brows and a tear after a death
  bool const moods = m_config.petMoods;
  bool const asleep = moods && m_sleepy > .6f;
  bool const happy = moods && m_petMood == PetMood::Happy;
  bool const sad = moods && m_petMood == PetMood::Sad;
  bool const blinking = m_petBlink < 0.f;
  bool const bird = m_config.pet == PetStyle::Bird;
  auto const eyeColor = premultiplied(kEye, alpha);
  float const line = r * .06f;
  std::array<float, 2> eyes = {face * r * .45f, face * r * .02f};
  for (size_t i = 0; i < (bird ? 1u : 2u); ++i)
  {
    CCPoint const eye = center + right * eyes[i] + up * (r * .1f);
    if (happy)
    {
      node->drawSegment(eye - right * (r * .13f) - up * (r * .04f), eye + up * (r * .08f), line, eyeColor);
      node->drawSegment(eye + up * (r * .08f), eye + right * (r * .13f) - up * (r * .04f), line, eyeColor);
    }
    else if (asleep)
    {
      node->drawSegment(eye - right * (r * .13f), eye - up * (r * .06f), line, eyeColor);
      node->drawSegment(eye - up * (r * .06f), eye + right * (r * .13f), line, eyeColor);
    }
    else if (blinking)
      node->drawSegment(eye - right * (r * .12f), eye + right * (r * .12f), r * .05f, eyeColor);
    else
    {
      fillCircle(node, eye, r * .14f, eyeColor);
      fillCircle(node, eye + up * (r * .05f) + right * (r * .04f), r * .05f, {alpha, alpha, alpha, alpha});
    }
    if (sad)
    {
      // Brows raised on the inner ends
      CCPoint const inner = right * (i == 0 ? -face : face);
      node->drawSegment(eye + up * (r * .32f) + inner * (r * .12f), eye + up * (r * .24f) - inner * (r * .12f), r * .045f, eyeColor);
    }
    fillCircle(node, eye - up * (r * .25f) + right * (face * r * .05f), r * .1f, premultiplied(kPetBlush, alpha * .6f));
  }

  if (sad)
  {
    // A tear rolling down from the front eye, again and again
    float const t = std::fmod(m_petClock, 1.2f) / 1.2f;
    CCPoint const tear = center + right * eyes[0] - up * (r * (.2f + .5f * t));
    auto const blue = premultiplied(kTear, alpha * (1.f - t * .6f));
    fillCircle(node, tear, r * .1f, blue);
    std::array<CCPoint, 3> tip = {tear - right * (r * .095f), tear + right * (r * .095f), tear + up * (r * .2f)};
    node->drawPolygon(tip.data(), 3, blue, 0.f, blue);
  }

  if (asleep)
  {
    // A tiny "z" floating up
    float const t = std::fmod(m_petClock, kPetZEvery) / kPetZEvery;
    CCPoint const z = center + up * (r * (1.3f + 1.6f * t)) + right * (face * r * (-.3f + .4f * t));
    float const half = r * (.22f + .18f * t);
    auto const zColor = premultiplied(kEye, alpha * (1.f - t));
    node->drawSegment(z - right * half + up * half, z + right * half + up * half, r * .06f, zColor);
    node->drawSegment(z + right * half + up * half, z - right * half - up * half, r * .06f, zColor);
    node->drawSegment(z - right * half - up * half, z + right * half - up * half, r * .06f, zColor);
  }
}
