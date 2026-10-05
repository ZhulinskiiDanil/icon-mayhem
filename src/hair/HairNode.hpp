#pragma once

#include "HairConfig.hpp"
#include "HairSim.hpp"

#include <Geode/Geode.hpp>
#include <functional>

// ! --- Hair node --- !
// Draws the hair behind a "head" sprite (the cube, ball, wave, the cube riding a ship,
// a robot or spider head). Simulation runs in `simSpace`
// (the player's parent), so the hair reacts to the real movement of the icon.
// Everything is done in visit(), which runs after the game updated the player
// this frame, so the roots never lag behind the icon.
// The hairstyle is either glued to the top of the icon and spins with it, or oriented
// by gravity and movement direction so it always stays on top of the head.

class HairNode : public cocos2d::CCDrawNode
{
public:
  // Hair grows from `head` and is drawn right behind `behind` (usually the head itself,
  // the whole body for robots and spiders). `primary` / `secondary` give the icon colors
  static HairNode *attach(cocos2d::CCSprite *head, cocos2d::CCNode *behind, cocos2d::CCSprite *primary,
                          cocos2d::CCSprite *secondary, cocos2d::CCNode *simSpace);

  void setShouldShow(std::function<bool()> fn) { m_shouldShow = std::move(fn); }
  void setGravityDir(std::function<cocos2d::CCPoint()> fn) { m_gravityDir = std::move(fn); }
  // +1 when moving right in sim space, -1 when moving left
  void setFacing(std::function<float()> fn) { m_facingFn = std::move(fn); }
  // The head stands on the floor below it (a grounded cube or ball), hair can't go under it
  void setOnGround(std::function<bool()> fn) { m_onGround = std::move(fn); }
  // The head is a cube, the hair lies on its faces (a round collider otherwise)
  void setBoxHead(std::function<bool()> fn) { m_boxHead = std::move(fn); }
  void setIdleWind(bool enabled) { m_idleWind = enabled; }
  void setGarage(bool garage) { m_isGarage = garage; }
  void resetSim() { m_needsReset = true; }

  void visit() override;

private:
  // One lock of the hairstyle, generated once per config
  struct Lock
  {
    float angle;  // degrees from "up" towards the back where the lock grows
    float length; // icon units
    float width;  // icon units, at the root
    float curl;   // degrees per segment
    float depth;  // 0 = back layer (darker), 1 = front layer
  };

  bool init(cocos2d::CCSprite *head, cocos2d::CCSprite *primary, cocos2d::CCSprite *secondary, cocos2d::CCNode *simSpace);

  void reloadConfig();
  void generateLocks();
  float headUnit() const; // head-local units per hair unit
  bool isActive() const;
  void updateFrame(float dt, bool snap);
  void updateMotion(float dt, cocos2d::CCPoint const &headCenter, bool snap);
  void gravityAxes(cocos2d::CCPoint &up, cocos2d::CCPoint &back) const;
  void iconAxes(cocos2d::CCAffineTransform const &headToSim, cocos2d::CCPoint &up, cocos2d::CCPoint &back) const;
  void buildTargets(cocos2d::CCPoint const &headCenter, cocos2d::CCPoint const &up, cocos2d::CCPoint const &back);
  void simulate(float dt);
  void redraw();
  void drawCap(cocos2d::CCAffineTransform const &simToHair, cocos2d::ccColor4F const &color, float outline,
               cocos2d::ccColor4F const &outlineColor);
  cocos2d::ccColor4F hairColor() const;

  cocos2d::CCSprite *m_head = nullptr;
  cocos2d::CCSprite *m_primary = nullptr;
  cocos2d::CCSprite *m_secondary = nullptr;
  cocos2d::CCNode *m_simSpace = nullptr;

  std::function<bool()> m_shouldShow;
  std::function<cocos2d::CCPoint()> m_gravityDir;
  std::function<float()> m_facingFn;
  std::function<bool()> m_onGround;
  std::function<bool()> m_boxHead;

  HairConfig m_config;
  unsigned m_configVersion = 0;
  std::vector<Lock> m_locks; // sorted back to front
  HairSim m_sim;
  std::vector<HairStrandTarget> m_targets;
  std::vector<cocos2d::CCPoint> m_curve; // scratch buffer for drawing

  // Last simulated frame, the base under the locks is drawn from it
  HairSimParams m_frameParams;
  cocos2d::CCPoint m_frameUp = {0.f, 1.f};
  cocos2d::CCPoint m_frameBack = {-1.f, 0.f};
  float m_capFrom = 0.f; // degrees, the arc the locks grow on
  float m_capTo = 0.f;

  float m_simScale = 1.f; // sim units per hair unit, a hair unit is 1/30 of the head size
  float m_upAngle = 0.f;  // radians, smoothed "away from gravity" direction
  float m_facing = 1.f;   // smoothed, -1..1
  float m_motion = 0.f;   // smoothed, 0 standing still .. 1 moving fast enough to comb the hair back
  cocos2d::CCPoint m_lastHeadCenter;
  unsigned m_lastFrame = 0;
  float m_time = 0.f;
  bool m_needsReset = true;
  bool m_idleWind = false;
  bool m_isGarage = false;
};
