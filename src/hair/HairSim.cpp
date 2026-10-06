#include "HairSim.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

// ! --- Constants --- !

namespace
{
  constexpr float kFixedStep = 1.f / 240.f;
  constexpr float kMaxFrameTime = 1.f / 20.f;

  // How quickly the windward side gets pressed fully, at 1 only hair right in front is pressed fully
  constexpr float kWindPressGain = 1.5f;

  // Max part of the way a segment moves towards its rest pose in one substep, keeps stiff hair stable
  constexpr float kMaxSpringPull = .9f;

  CCPoint lerp(CCPoint const &a, CCPoint const &b, float t)
  {
    return a + (b - a) * t;
  }

  // Flutter, see HairSimParams::flutter
  constexpr float kFlutterFrequency = 3.f; // noise cells / s
  constexpr float kFlutterAlongLock = .35f; // noise cells between neighbouring segments
  constexpr float kFlutterPerLock = 7.13f;  // noise offset between locks, so they move on their own

  float noiseHash(int i)
  {
    auto x = static_cast<uint32_t>(i) * 374761393u + 668265263u;
    x = (x ^ (x >> 13)) * 1274126177u;
    x ^= x >> 16;
    return static_cast<float>(x) / 4294967295.f * 2.f - 1.f;
  }

  // Converts a "per 1/60 s" factor into a factor for a step of `h` seconds
  float perStep(float perFrame, float h)
  {
    return 1.f - std::pow(1.f - std::clamp(perFrame, 0.f, .999f), h * 60.f);
  }
}

// ! --- Noise --- !

float hairNoise(float x)
{
  float const cell = std::floor(x);
  float const t = x - cell;
  float const smooth = t * t * (3.f - 2.f * t);
  int const i = static_cast<int>(cell);
  return noiseHash(i) + (noiseHash(i + 1) - noiseHash(i)) * smooth;
}

// ! --- Params --- !

float HairSimParams::headSurface(CCPoint const &dir) const
{
  // Squircle |x|^n + |y|^n = a^n in the head space
  auto squircle = [&](float halfSize, float exponent)
  {
    float const x = std::abs(dir.dot(headAxisX));
    float const y = std::abs(dir.dot(headAxisY));
    float const norm = std::pow(std::pow(x, exponent) + std::pow(y, exponent), 1.f / exponent);
    return norm > .0001f ? halfSize / norm : halfSize;
  };

  float const relaxed = headBox ? squircle(headHalfSize, headExponent) : headRadius;

  float const press = std::clamp(dir.dot(windDir) * windPress * kWindPressGain, 0.f, 1.f);
  if (press <= 0.f)
    return relaxed;

  float const tight = headBox ? squircle(tightHalfSize, tightExponent) : tightRadius;
  return relaxed + (std::min(tight, relaxed) - relaxed) * press;
}

// ! --- Setup --- !

void HairSim::setup(int strandCount, int segments)
{
  m_segments = segments;
  m_pos.assign(strandCount, std::vector<CCPoint>(segments + 1));
  m_prev = m_pos;
  m_lastRoots.assign(strandCount, CCPoint{});
  m_frameRoots = m_lastRoots;
  m_accumulator = 0.f;
  m_initialized = false;
}

void HairSim::kick(CCPoint const &velocity, CCPoint const &center, float puff)
{
  if (!m_initialized || m_segments <= 0)
    return;

  // Verlet keeps the velocity in the previous positions
  for (size_t s = 0; s < m_pos.size(); ++s)
  {
    for (int k = 1; k <= m_segments; ++k)
    {
      float const weight = static_cast<float>(k) / static_cast<float>(m_segments);
      CCPoint const away = m_pos[s][k] - center;
      float const length = away.getLength();
      CCPoint const outward = length > .0001f ? away / length : CCPoint{};
      m_prev[s][k] = m_prev[s][k] - (velocity + outward * puff) * (weight * kFixedStep);
    }
  }
}

void HairSim::reset(std::vector<HairStrandTarget> const &targets, CCPoint const &headCenter)
{
  for (size_t s = 0; s < m_pos.size() && s < targets.size(); ++s)
  {
    auto const &target = targets[s];
    auto &pos = m_pos[s];

    pos[0] = target.root;
    for (int k = 1; k <= m_segments; ++k)
      pos[k] = pos[k - 1] + target.restDirs[k - 1] * target.segmentLength;

    m_prev[s] = pos;
    m_lastRoots[s] = target.root;
  }

  m_lastHead = headCenter;
  m_accumulator = 0.f;
  m_initialized = true;
}

// ! --- Step --- !

void HairSim::step(float dt, std::vector<HairStrandTarget> const &targets, HairSimParams const &params)
{
  if (targets.size() != m_pos.size())
    return;

  if (!m_initialized)
  {
    reset(targets, params.headCenter);
    return;
  }

  // Respawn, checkpoint or teleport portal: snap into the rest pose instead of flying across the screen
  for (size_t s = 0; s < targets.size(); ++s)
  {
    if (targets[s].root.getDistance(m_lastRoots[s]) > params.teleportDistance)
    {
      reset(targets, params.headCenter);
      return;
    }
  }

  m_frameRoots = m_lastRoots;
  m_frameHead = m_lastHead;
  m_accumulator += std::min(dt, kMaxFrameTime);

  int const steps = static_cast<int>(m_accumulator / kFixedStep);
  CCPoint const headStep = steps > 0 ? (params.headCenter - m_frameHead) / static_cast<float>(steps) : CCPoint{};
  for (int i = 0; i < steps; ++i)
  {
    // Roots move smoothly between last frame and this one across the substeps
    float const alpha = static_cast<float>(i + 1) / static_cast<float>(steps);
    substep(kFixedStep, alpha, headStep, targets, params);
  }
  m_accumulator -= steps * kFixedStep;

  for (size_t s = 0; s < targets.size(); ++s)
  {
    m_lastRoots[s] = targets[s].root;
    // Keep the root glued to the icon even when no substep ran this frame
    m_pos[s][0] = targets[s].root;
    m_prev[s][0] = targets[s].root;
  }
  m_lastHead = params.headCenter;
}

void HairSim::substep(float h, float alpha, CCPoint const &headStep, std::vector<HairStrandTarget> const &targets,
                      HairSimParams const &params)
{
  float const keep = 1.f - perStep(params.damping, h);
  float const friction = perStep(params.friction, h);

  // Damping is air drag: it pulls the hair towards the speed of the air around it. With full wind
  // the air stands still and the hair trails behind, without wind it moves along with the head
  CCPoint const airStep = headStep * (1.f - params.windMultiplier) + params.breeze * h;

  float const spinStep = params.spinSpeed * h;
  float const calm = params.calm > 0.f ? perStep(params.calm, h) : 0.f;
  CCPoint const accel = (params.gravity + params.wind) * (h * h);
  CCPoint const head = lerp(m_frameHead, params.headCenter, alpha);

  float const flutter = params.flutter * h * h;
  CCPoint const flutterAcross = {-params.flutterDir.y, params.flutterDir.x};
  float const flutterTime = params.time * kFlutterFrequency;

  for (size_t s = 0; s < m_pos.size(); ++s)
  {
    auto const &target = targets[s];
    auto &pos = m_pos[s];
    auto &prev = m_prev[s];
    float const segLen = target.segmentLength;

    // Pinned root
    CCPoint const lastRoot = pos[0];
    pos[0] = lerp(m_frameRoots[s], target.root, alpha);
    prev[0] = pos[0];

    // Internal friction works from the root out: every segment is pulled towards the speed of its parent.
    // Swinging and whipping die out, a strand moving as a whole (trailing in the wind) stays as is
    CCPoint parentVelocity = pos[0] - lastRoot;

    // Verlet integration
    for (int k = 1; k <= m_segments; ++k)
    {
      CCPoint velocity = airStep + (pos[k] - prev[k] - airStep) * keep;

      // Where this point would go if the hair were glued to the spinning head
      if (calm > 0.f)
      {
        CCPoint const offset = pos[k] - head;
        CCPoint const rigid = headStep + CCPoint{-offset.y, offset.x} * spinStep;
        velocity = velocity + (rigid - velocity) * calm;
      }

      velocity = velocity + (parentVelocity - velocity) * friction;
      parentVelocity = velocity;

      // Mostly across the air flow like a flag, a bit along it, stronger towards the tip
      CCPoint gust = accel;
      if (flutter > 0.f)
      {
        float const key = flutterTime + static_cast<float>(s) * kFlutterPerLock + static_cast<float>(k) * kFlutterAlongLock;
        float const tip = static_cast<float>(k) / static_cast<float>(m_segments);
        gust = gust + (flutterAcross * hairNoise(key) + params.flutterDir * (.5f * hairNoise(key + 101.f))) * (flutter * tip);
      }

      prev[k] = pos[k];
      pos[k] = pos[k] + velocity + gust;
    }

    for (int k = 1; k <= m_segments; ++k)
    {
      // Style spring towards the rest direction relative to the parent point,
      // sampled mid-segment so even the tip keeps a little of its shape
      float const along = (static_cast<float>(k) - .5f) / static_cast<float>(m_segments);
      float const spring = target.stiffness * std::pow(1.f - along, target.stiffnessPower);
      CCPoint const rest = pos[k - 1] + target.restDirs[k - 1] * segLen;
      pos[k] = lerp(pos[k], rest, std::min(spring * h * h, kMaxSpringPull));

      // Head collider, the first segments start inside it so let them grow out gradually
      if (target.collide && (params.headBox || params.headRadius > 0.f))
      {
        CCPoint const fromHead = pos[k] - head;
        float const dist = fromHead.getLength();
        if (dist > .0001f)
        {
          CCPoint const dir = fromHead / dist;
          float const grownOut = params.rootRadius + static_cast<float>(k) * segLen * .9f;
          float const minDist = std::min(params.headSurface(dir), grownOut);
          if (dist < minDist)
            pos[k] = head + dir * minDist;
        }
      }

      // Floor goes after the head so the hair never sinks into the ground
      if (params.floorDistance > 0.f)
      {
        float const below = (pos[k] - head).dot(params.floorDown) - params.floorDistance;
        if (below > 0.f)
          pos[k] = pos[k] - params.floorDown * below;
      }

      // Follow-the-leader length constraint, the root is fixed so one pass is exact
      CCPoint delta = pos[k] - pos[k - 1];
      float const len = delta.getLength();
      if (len > .0001f)
        pos[k] = pos[k - 1] + delta * (segLen / len);
      else
        pos[k] = rest;
    }
  }
}
