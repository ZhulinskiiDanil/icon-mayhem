#include "../hair/HairNode.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// ! --- Helpers --- !

namespace
{
  bool isAlive(PlayerObject *player)
  {
    return !player->m_isDead && !player->m_isHidden;
  }

  // Robot and spider draw their own body, every other mode shows m_iconSprite
  // (the cube itself, the ball, wave, swing, or the cube riding a ship / UFO / jetpack)
  bool usesIconSprite(PlayerObject *player)
  {
    return !player->m_isRobot && !player->m_isSpider;
  }

  // The icon itself touches the ground, a cube riding a ship or UFO doesn't
  bool iconStandsOnGround(PlayerObject *player)
  {
    return player->m_isOnGround && !player->m_isShip && !player->m_isBird;
  }

  // The cube itself or the cube riding a ship / UFO / jetpack, the ball, wave and swing are rounder
  bool iconIsCube(PlayerObject *player)
  {
    return !player->m_isBall && !player->m_isDart && !player->m_isSwing;
  }

  // The jetpack is the ship of platformer levels
  GameMode gameModeOf(PlayerObject *player)
  {
    if (player->m_isShip)
      return player->m_isPlatformer ? GameMode::Jetpack : GameMode::Ship;
    if (player->m_isBird)
      return GameMode::Ufo;
    if (player->m_isBall)
      return GameMode::Ball;
    if (player->m_isDart)
      return GameMode::Wave;
    if (player->m_isRobot)
      return GameMode::Robot;
    if (player->m_isSpider)
      return GameMode::Spider;
    if (player->m_isSwing)
      return GameMode::Swing;
    return GameMode::Cube;
  }

  CCSprite *headOf(GJRobotSprite *body)
  {
    return body->m_headSprite ? static_cast<CCSprite *>(body->m_headSprite) : body;
  }

  HairNode *setupHair(HairNode *hair, PlayerObject *player, bool playerTwo, std::function<bool()> modeMatches)
  {
    if (!hair)
      return nullptr;

    hair->setShouldShow([player, modeMatches = std::move(modeMatches)]
                        { return isAlive(player) && modeMatches(); });
    hair->setGravityDir([player]
                        { return player->m_isUpsideDown ? CCPoint{0.f, 1.f} : CCPoint{0.f, -1.f}; });
    hair->setFacing([player]
                    { return player->m_isGoingLeft ? -1.f : 1.f; });
    hair->setIsDead([player]
                    { return player->m_isDead; });
    hair->setGameMode([player]
                      { return gameModeOf(player); });
    hair->setMusicDriven(true);
    hair->setPlayer(player);
    hair->setUseLooks(true, playerTwo);
    return hair;
  }

  // Attaches the hair for every mode of the player, adds the nodes to `nodes` for the reactions.
  // False while the player isn't in the scene yet: player 2 is only added on the first dual portal
  bool attachHair(PlayerObject *player, bool playerTwo, std::vector<Ref<HairNode>> &nodes)
  {
    if (!player || !player->getParent())
      return false;

    auto simSpace = player->getParent();
    auto primary = player->m_iconSprite;
    auto secondary = player->m_iconSpriteSecondary;

    auto iconHair = setupHair(HairNode::attach(player->m_iconSprite, player->m_iconSprite, primary, secondary, simSpace),
                              player, playerTwo, [player]
                              { return usesIconSprite(player); });
    if (iconHair)
    {
      nodes.push_back(iconHair);
      iconHair->setOnGround([player]
                            { return iconStandsOnGround(player); });
      iconHair->setBoxHead([player]
                           { return iconIsCube(player); });
    }

    if (auto robot = player->m_robotSprite)
    {
      if (auto hair = setupHair(HairNode::attach(headOf(robot), robot, primary, secondary, simSpace),
                                player, playerTwo, [player]
                                { return player->m_isRobot; }))
        nodes.push_back(hair);
    }

    if (auto spider = player->m_spiderSprite)
    {
      if (auto hair = setupHair(HairNode::attach(headOf(spider), spider, primary, secondary, simSpace),
                                player, playerTwo, [player]
                                { return player->m_isSpider; }))
        nodes.push_back(hair);
    }
    return true;
  }
}

// ! --- PlayLayer --- !

class $modify(HairPlayLayer, PlayLayer)
{
  struct Fields
  {
    std::vector<Ref<HairNode>> m_hair;
    bool m_player2Attached = false;
  };

  bool init(GJGameLevel *level, bool useReplay, bool dontCreateObjects)
  {
    if (!PlayLayer::init(level, useReplay, dontCreateObjects))
      return false;

    attachHair(m_player1, false, m_fields->m_hair);
    m_fields->m_player2Attached = attachHair(m_player2, true, m_fields->m_hair);

    return true;
  }

  // Player 2 joins the scene on the first dual portal, its hair is attached then
  void postUpdate(float dt)
  {
    PlayLayer::postUpdate(dt);

    auto fields = m_fields.self();
    if (!fields->m_player2Attached && m_player2 && m_player2->getParent())
      fields->m_player2Attached = attachHair(m_player2, true, fields->m_hair);
  }

  // ! --- Reactions --- !

  void levelComplete()
  {
    PlayLayer::levelComplete();
    for (auto &hair : m_fields->m_hair)
      hair->celebrate();
  }

  void storeCheckpoint(CheckpointObject *checkpoint)
  {
    PlayLayer::storeCheckpoint(checkpoint);
    for (auto &hair : m_fields->m_hair)
      hair->checkpointReached();
  }

  void checkpointActivated(CheckpointGameObject *checkpoint)
  {
    PlayLayer::checkpointActivated(checkpoint);
    for (auto &hair : m_fields->m_hair)
      hair->checkpointReached();
  }
};

// ! --- Orbs and pads --- !

class $modify(HairPlayerObject, PlayerObject)
{
  // Tells the player's rigs when an orb or a pad changed its vertical speed
  void notifyBoost(double velocityBefore)
  {
    if (m_yVelocity == velocityBefore)
      return;

    auto playLayer = static_cast<HairPlayLayer *>(PlayLayer::get());
    if (!playLayer)
      return;

    // The velocity goes up along the player's own up, which flips with gravity
    CCPoint const up = m_isUpsideDown ? CCPoint{0.f, -1.f} : CCPoint{0.f, 1.f};
    CCPoint const launch = m_yVelocity >= 0.0 ? up : up * -1.f;
    for (auto &hair : playLayer->m_fields->m_hair)
    {
      if (hair->player() == this)
        hair->boosted(launch);
    }
  }

  void ringJump(RingObject *object, bool skipCheck)
  {
    double const before = m_yVelocity;
    PlayerObject::ringJump(object, skipCheck);
    this->notifyBoost(before);
  }

  void bumpPlayer(float bumpMod, int objectType, bool noEffects, GameObject *object)
  {
    double const before = m_yVelocity;
    PlayerObject::bumpPlayer(bumpMod, objectType, noEffects, object);
    this->notifyBoost(before);
  }
};
