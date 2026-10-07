#pragma once

#include <Geode/Geode.hpp>
#include <algorithm>
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
  float stiffness = 0.f;      // style spring at the root, 1 / s^2
  float stiffnessPower = 2.f; // how fast the style spring fades towards the tip
  bool collide = true;        // false for hair lying in front of the face, it never touches the head
};

struct HairSimParams
{
  float damping = .06f;  // air drag, per 1/60 s
  float friction = .3f;  // per 1/60 s, internal friction: damps the motion of a segment relative to its parent
  float windMultiplier = 1.f; // 0: the air moves with the head, the hair doesn't trail behind

  // Calm spins: while the head spins the hair is pulled towards turning with it as one piece,
  // instead of whipping around the head and shaking after the landing
  float spinSpeed = 0.f; // radians / s the head turns, counterclockwise is positive
  float calm = 0.f;      // per 1/60 s, how strongly the hair is pulled towards turning with the head

  // Gusty air: a breeze blowing even standing still, and flutter, small random flicks that make
  // every lock move on its own, stronger towards the tips
  cocos2d::CCPoint breeze = {0.f, 0.f};    // units / s, velocity of the air on top of the movement
  float flutter = 0.f;                     // units / s^2
  cocos2d::CCPoint flutterDir = {-1.f, 0.f}; // direction the air flows to
  float time = 0.f;                        // s, drives the flutter noise
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

  // Oncoming air presses the hair to the windward side of the head: the collider shrinks
  // there towards a tight shape (a sharp squircle for cubes, a small circle otherwise)
  cocos2d::CCPoint windDir = {1.f, 0.f}; // direction of movement
  float windPress = 0.f;                 // 0 standing still .. 1 moving fast
  float tightRadius = 0.f;
  float tightHalfSize = 0.f;
  float tightExponent = 8.f;

  // Distance from the head center to the collider surface along a unit direction
  float headSurface(cocos2d::CCPoint const &dir) const;

  cocos2d::CCPoint floorDown = {0.f, -1.f};
  float floorDistance = 0.f; // floor below the head center, 0 disables it
};

// Smooth noise in -1..1, the same input always gives the same output
float hairNoise(float x);

class HairSim
{
public:
  void setup(int strandCount, int segments);
  void reset(std::vector<HairStrandTarget> const &targets, cocos2d::CCPoint const &headCenter);
  void step(float dt, std::vector<HairStrandTarget> const &targets, HairSimParams const &params);
  // Throws every strand: `velocity` for all of them plus `puff` away from `center`, more towards the tips
  void kick(cocos2d::CCPoint const &velocity, cocos2d::CCPoint const &center, float puff);
  // Simulation steps per second, fewer are cheaper (the performance mode)
  void setStepRate(float hz) { m_fixedStep = 1.f / std::max(hz, 30.f); }

  std::vector<std::vector<cocos2d::CCPoint>> const &strands() const { return m_pos; }
  int segments() const { return m_segments; }

private:
  void substep(float h, float alpha, cocos2d::CCPoint const &headStep, std::vector<HairStrandTarget> const &targets,
               HairSimParams const &params);

  int m_segments = 0;
  float m_fixedStep = 1.f / 240.f;
  float m_accumulator = 0.f;
  bool m_initialized = false;

  std::vector<std::vector<cocos2d::CCPoint>> m_pos;
  std::vector<std::vector<cocos2d::CCPoint>> m_prev;
  std::vector<cocos2d::CCPoint> m_lastRoots;  // roots at the end of the previous frame
  std::vector<cocos2d::CCPoint> m_frameRoots; // roots at the start of the current frame
  cocos2d::CCPoint m_lastHead;
  cocos2d::CCPoint m_frameHead;
};
