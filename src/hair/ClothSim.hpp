#pragma once

#include <Geode/Geode.hpp>

#include <vector>

// ! --- Cloth simulation --- !
// A verlet grid: `cols` points along the pinned edge, `rows` points away from it. Structural and
// shear distance constraints keep it a sheet, gravity pulls it, air drag holds it back while the
// pins move so it streams behind. Fixed steps like the hair, and a frame without a step carries the
// sheet along with its pins so it never lags behind at high FPS.

struct ClothParams
{
  cocos2d::CCPoint gravity;  // units / s^2
  cocos2d::CCPoint air;      // velocity of the air around, units / s (a breeze)
  float drag = 3.f;          // 1 / s, how strongly the air holds the cloth
  float damping = .5f;       // 1 / s, settles the swinging
  float teleportDistance = 160.f;
};

class ClothSim
{
public:
  // `pins` are the points of the pinned edge, the sheet hangs from them along `hang`
  void setup(int cols, int rows, float spacing);
  void reset(std::vector<cocos2d::CCPoint> const &pins, cocos2d::CCPoint const &hang);
  void step(float dt, std::vector<cocos2d::CCPoint> const &pins, cocos2d::CCPoint const &hang, ClothParams const &params);
  void setStepRate(float hz) { m_fixedStep = 1.f / std::max(hz, 30.f); }

  int cols() const { return m_cols; }
  int rows() const { return m_rows; }
  bool ready() const { return m_initialized; }
  cocos2d::CCPoint const &at(int col, int row) const { return m_pos[static_cast<size_t>(row * m_cols + col)]; }

private:
  void substep(float h, float alpha, std::vector<cocos2d::CCPoint> const &pins, ClothParams const &params);
  void satisfy(float spacing);

  int m_cols = 0;
  int m_rows = 0;
  float m_spacing = 1.f;
  float m_fixedStep = 1.f / 240.f;
  float m_accumulator = 0.f;
  bool m_initialized = false;
  std::vector<cocos2d::CCPoint> m_pos;
  std::vector<cocos2d::CCPoint> m_prev;
  std::vector<cocos2d::CCPoint> m_lastPins;
};
