#include "HairSim.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

// ! --- Constants --- !

namespace
{
  constexpr float kFixedStep = 1.f / 240.f;
  constexpr float kMaxFrameTime = 1.f / 20.f;

  // Max part of the way a segment moves towards its rest pose in one substep, keeps stiff hair stable
  constexpr float kMaxSpringPull = .9f;

  CCPoint lerp(CCPoint const &a, CCPoint const &b, float t)
  {
    return a + (b - a) * t;
  }

  // Converts a "per 1/60 s" factor into a factor for a step of `h` seconds
  float perStep(float perFrame, float h)
  {
    return 1.f - std::pow(1.f - std::clamp(perFrame, 0.f, .999f), h * 60.f);
  }
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
  for (int i = 0; i < steps; ++i)
  {
    // Roots move smoothly between last frame and this one across the substeps
    float const alpha = static_cast<float>(i + 1) / static_cast<float>(steps);
    substep(kFixedStep, alpha, targets, params);
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

void HairSim::substep(float h, float alpha, std::vector<HairStrandTarget> const &targets, HairSimParams const &params)
{
  float const keep = 1.f - perStep(params.damping, h);
  CCPoint const accel = (params.gravity + params.wind) * (h * h);
  CCPoint const head = lerp(m_frameHead, params.headCenter, alpha);

  for (size_t s = 0; s < m_pos.size(); ++s)
  {
    auto const &target = targets[s];
    auto &pos = m_pos[s];
    auto &prev = m_prev[s];
    float const segLen = target.segmentLength;

    // Pinned root
    pos[0] = lerp(m_frameRoots[s], target.root, alpha);
    prev[0] = pos[0];

    // Verlet integration
    for (int k = 1; k <= m_segments; ++k)
    {
      CCPoint const velocity = (pos[k] - prev[k]) * keep;
      prev[k] = pos[k];
      pos[k] = pos[k] + velocity + accel;
    }

    for (int k = 1; k <= m_segments; ++k)
    {
      // Style spring towards the rest direction relative to the parent point,
      // sampled mid-segment so even the tip keeps a little of its shape
      float const along = (static_cast<float>(k) - .5f) / static_cast<float>(m_segments);
      float const spring = target.stiffness * std::pow(1.f - along, params.stiffnessPower);
      CCPoint const rest = pos[k - 1] + target.restDirs[k - 1] * segLen;
      pos[k] = lerp(pos[k], rest, std::min(spring * h * h, kMaxSpringPull));

      // Head collider, the first segments start inside it so let them grow out gradually
      if (params.headRadius > 0.f)
      {
        CCPoint const fromHead = pos[k] - head;
        float const dist = fromHead.getLength();
        float const minDist = std::min(params.headRadius, params.rootRadius + k * segLen * .9f);
        if (dist < minDist && dist > .0001f)
          pos[k] = head + fromHead * (minDist / dist);
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
