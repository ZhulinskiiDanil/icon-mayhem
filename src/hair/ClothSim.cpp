#include "ClothSim.hpp"

#include <algorithm>

using namespace geode::prelude;

// ! --- Cloth simulation --- !

namespace
{
  constexpr float kMaxFrameTime = 1.f / 20.f;
  constexpr int kIterations = 4; // constraint passes per substep
  constexpr float kShear = 1.41421356f;
  constexpr float kMinFold = .5f; // a link folds down to this much of its length

  CCPoint lerp(CCPoint const &a, CCPoint const &b, float t)
  {
    return a + (b - a) * t;
  }

  // Cloth doesn't stretch but folds freely: only a stretched link is pulled back to its rest
  // length, a squeezed one only once it folds past half of it. In 2D a sheet that resists squeezing
  // is a stiff plate that never hangs down
  void constrain(CCPoint &a, CCPoint &b, float rest, bool aPinned, bool bPinned)
  {
    CCPoint const delta = b - a;
    float const length = delta.getLength();
    if (length < .0001f || (aPinned && bPinned))
      return;
    float target;
    if (length > rest)
      target = rest;
    else if (length < rest * kMinFold)
      target = rest * kMinFold;
    else
      return;
    CCPoint const fix = delta * ((length - target) / length);
    if (aPinned)
      b = b - fix;
    else if (bPinned)
      a = a + fix;
    else
    {
      a = a + fix * .5f;
      b = b - fix * .5f;
    }
  }
}

void ClothSim::setup(int cols, int rows, float spacing)
{
  m_cols = std::max(cols, 2);
  m_rows = std::max(rows, 2);
  m_spacing = spacing;
  m_pos.assign(static_cast<size_t>(m_cols * m_rows), CCPoint{});
  m_prev = m_pos;
  m_lastPins.assign(static_cast<size_t>(m_cols), CCPoint{});
  m_accumulator = 0.f;
  m_initialized = false;
}

void ClothSim::reset(std::vector<CCPoint> const &pins, CCPoint const &hang)
{
  if (static_cast<int>(pins.size()) != m_cols)
    return;
  for (int c = 0; c < m_cols; ++c)
  {
    for (int r = 0; r < m_rows; ++r)
      m_pos[static_cast<size_t>(r * m_cols + c)] = pins[static_cast<size_t>(c)] + hang * (m_spacing * static_cast<float>(r));
    m_lastPins[static_cast<size_t>(c)] = pins[static_cast<size_t>(c)];
  }
  m_prev = m_pos;
  m_accumulator = 0.f;
  m_initialized = true;
}

void ClothSim::step(float dt, std::vector<CCPoint> const &pins, CCPoint const &hang, ClothParams const &params)
{
  if (static_cast<int>(pins.size()) != m_cols)
    return;
  if (!m_initialized)
  {
    this->reset(pins, hang);
    return;
  }
  for (int c = 0; c < m_cols; ++c)
  {
    if (pins[static_cast<size_t>(c)].getDistance(m_lastPins[static_cast<size_t>(c)]) > params.teleportDistance)
    {
      this->reset(pins, hang);
      return;
    }
  }

  m_accumulator += std::min(dt, kMaxFrameTime);
  int const steps = static_cast<int>(m_accumulator / m_fixedStep);
  for (int i = 0; i < steps; ++i)
    this->substep(m_fixedStep, static_cast<float>(i + 1) / static_cast<float>(steps), pins, params);
  m_accumulator -= static_cast<float>(steps) * m_fixedStep;

  // A frame without a step: carry the sheet along with its pins
  if (steps == 0)
  {
    for (int c = 0; c < m_cols; ++c)
    {
      CCPoint const carry = pins[static_cast<size_t>(c)] - m_lastPins[static_cast<size_t>(c)];
      for (int r = 1; r < m_rows; ++r)
      {
        auto const index = static_cast<size_t>(r * m_cols + c);
        m_pos[index] = m_pos[index] + carry;
        m_prev[index] = m_prev[index] + carry;
      }
    }
  }

  for (int c = 0; c < m_cols; ++c)
  {
    m_pos[static_cast<size_t>(c)] = pins[static_cast<size_t>(c)];
    m_prev[static_cast<size_t>(c)] = pins[static_cast<size_t>(c)];
    m_lastPins[static_cast<size_t>(c)] = pins[static_cast<size_t>(c)];
  }
}

void ClothSim::substep(float h, float alpha, std::vector<CCPoint> const &pins, ClothParams const &params)
{
  float const keep = std::max(0.f, 1.f - params.damping * h);
  float const drag = std::min(1.f, params.drag * h);

  for (int r = 1; r < m_rows; ++r)
  {
    for (int c = 0; c < m_cols; ++c)
    {
      auto const index = static_cast<size_t>(r * m_cols + c);
      CCPoint &pos = m_pos[index];
      CCPoint &prev = m_prev[index];

      // Velocity eased towards the air, so moving pins pull the sheet out behind them
      CCPoint velocity = (pos - prev) / h * keep;
      velocity = velocity + (params.air - velocity) * drag;
      prev = pos;
      pos = pos + velocity * h + params.gravity * (h * h);
    }
  }

  // The pinned edge moves smoothly from last frame to this one across the substeps
  for (int c = 0; c < m_cols; ++c)
  {
    auto const index = static_cast<size_t>(c);
    m_pos[index] = lerp(m_lastPins[index], pins[index], alpha);
    m_prev[index] = m_pos[index];
  }

  for (int i = 0; i < kIterations; ++i)
    this->satisfy(m_spacing);
}

void ClothSim::satisfy(float spacing)
{
  auto at = [&](int c, int r) -> CCPoint & { return m_pos[static_cast<size_t>(r * m_cols + c)]; };
  for (int r = 0; r < m_rows; ++r)
  {
    for (int c = 0; c < m_cols; ++c)
    {
      bool const pinned = r == 0;
      if (c + 1 < m_cols)
        constrain(at(c, r), at(c + 1, r), spacing, pinned, pinned);
      if (r + 1 < m_rows)
      {
        constrain(at(c, r), at(c, r + 1), spacing, pinned, false);
        if (c + 1 < m_cols)
        {
          constrain(at(c, r), at(c + 1, r + 1), spacing * kShear, pinned, false);
          constrain(at(c + 1, r), at(c, r + 1), spacing * kShear, pinned, false);
        }
      }
    }
  }
}
