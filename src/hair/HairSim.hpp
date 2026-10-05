#pragma once

#include <Geode/Geode.hpp>
#include <vector>

// ! --- Hair simulation --- !
// Verlet strands with a pinned root, follow-the-leader length constraint,
// a soft "style" spring that pulls every segment towards its rest direction
// (strong at the root, almost gone at the tip), a round or squircle collider for the head
// and an optional floor under it.
// Works in an arbitrary 2D space (the player's parent), knows nothing about nodes.

struct HairStrandTarget
{
  cocos2d::CCPoint root;
  std::vector<cocos2d::CCPoint> restDirs; // unit vectors, one per segment
  float segmentLength = 2.f;
  float stiffness = 0.f; // style spring at the root, 1 / s^2
};

struct HairSimParams
{
  float stiffnessPower = 2.f; // how fast the style spring fades towards the tip
  float damping = .06f;
  cocos2d::CCPoint gravity = {0.f, -900.f}; // units / s^2
  cocos2d::CCPoint wind = {0.f, 0.f};       // units / s^2
  float teleportDistance = 160.f;

  cocos2d::CCPoint headCenter;
  float headRadius = 0.f; // round collider, 0 disables it
  float rootRadius = 0.f; // distance from the head center to the roots

  // Squircle collider instead of the round one: flat enough for the hair to lie on the faces
  // of a cube, round enough to go around its corners instead of leaving them bald
  bool headBox = false;
  cocos2d::CCPoint headAxisX = {1.f, 0.f};
  cocos2d::CCPoint headAxisY = {0.f, 1.f};
  float headHalfSize = 0.f;
  float headExponent = 6.f; // 2 is a circle, the higher the squarer

  // Distance from the head center to the collider surface along a unit direction
  float headSurface(cocos2d::CCPoint const &dir) const;

  cocos2d::CCPoint floorDown = {0.f, -1.f};
  float floorDistance = 0.f; // floor below the head center, 0 disables it
};

class HairSim
{
public:
  void setup(int strandCount, int segments);
  void reset(std::vector<HairStrandTarget> const &targets, cocos2d::CCPoint const &headCenter);
  void step(float dt, std::vector<HairStrandTarget> const &targets, HairSimParams const &params);

  std::vector<std::vector<cocos2d::CCPoint>> const &strands() const { return m_pos; }
  int segments() const { return m_segments; }

private:
  void substep(float h, float alpha, std::vector<HairStrandTarget> const &targets, HairSimParams const &params);

  int m_segments = 0;
  float m_accumulator = 0.f;
  bool m_initialized = false;

  std::vector<std::vector<cocos2d::CCPoint>> m_pos;
  std::vector<std::vector<cocos2d::CCPoint>> m_prev;
  std::vector<cocos2d::CCPoint> m_lastRoots;  // roots at the end of the previous frame
  std::vector<cocos2d::CCPoint> m_frameRoots; // roots at the start of the current frame
  cocos2d::CCPoint m_lastHead;
  cocos2d::CCPoint m_frameHead;
};
