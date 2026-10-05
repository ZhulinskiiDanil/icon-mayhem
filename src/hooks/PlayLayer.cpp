#include "../hair/HairNode.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

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

  CCSprite *headOf(GJRobotSprite *body)
  {
    return body->m_headSprite ? static_cast<CCSprite *>(body->m_headSprite) : body;
  }

  void setupHair(HairNode *hair, PlayerObject *player, std::function<bool()> modeMatches)
  {
    if (!hair)
      return;

    hair->setShouldShow([player, modeMatches = std::move(modeMatches)]
                        { return isAlive(player) && modeMatches(); });
    hair->setGravityDir([player]
                        { return player->m_isUpsideDown ? CCPoint{0.f, 1.f} : CCPoint{0.f, -1.f}; });
    hair->setFacing([player]
                    { return player->m_isGoingLeft ? -1.f : 1.f; });
  }

  void attachHair(PlayerObject *player)
  {
    if (!player)
      return;

    auto simSpace = player->getParent();
    auto primary = player->m_iconSprite;
    auto secondary = player->m_iconSpriteSecondary;

    setupHair(HairNode::attach(player->m_iconSprite, player->m_iconSprite, primary, secondary, simSpace),
              player, [player]
              { return usesIconSprite(player); });

    if (auto robot = player->m_robotSprite)
    {
      setupHair(HairNode::attach(headOf(robot), robot, primary, secondary, simSpace),
                player, [player]
                { return player->m_isRobot; });
    }

    if (auto spider = player->m_spiderSprite)
    {
      setupHair(HairNode::attach(headOf(spider), spider, primary, secondary, simSpace),
                player, [player]
                { return player->m_isSpider; });
    }
  }
}

// ! --- PlayLayer --- !

class $modify(HairPlayLayer, PlayLayer)
{
  bool init(GJGameLevel *level, bool useReplay, bool dontCreateObjects)
  {
    if (!PlayLayer::init(level, useReplay, dontCreateObjects))
      return false;

    attachHair(m_player1);
    attachHair(m_player2);

    return true;
  }
};
