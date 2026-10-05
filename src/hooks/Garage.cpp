#include "../hair/HairNode.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <Geode/modify/SimplePlayer.hpp>

using namespace geode::prelude;

// ! --- Helpers --- !

namespace
{
  // The garage preview of a ship, UFO or jetpack is the vehicle alone, there is no head to put hair on
  bool showsHeadSprite(IconType type)
  {
    return type == IconType::Cube || type == IconType::Ball || type == IconType::Wave || type == IconType::Swing;
  }

  CCSprite *headOf(GJRobotSprite *body)
  {
    return body->m_headSprite ? static_cast<CCSprite *>(body->m_headSprite) : body;
  }
}

// ! --- SimplePlayer --- !

class $modify(HairSimplePlayer, SimplePlayer)
{
  struct Fields
  {
    IconType m_iconType = IconType::Cube;
    bool m_hairEnabled = false;
    HairNode *m_iconHair = nullptr;
    HairNode *m_robotHair = nullptr;
    HairNode *m_spiderHair = nullptr;
  };

  void updatePlayerFrame(int id, IconType type)
  {
    SimplePlayer::updatePlayerFrame(id, type);

    auto fields = m_fields.self();
    fields->m_iconType = type;

    // Robot and spider sprites are created lazily on the first switch to them
    if (fields->m_hairEnabled)
      this->attachBodyHair();

    for (auto hair : {fields->m_iconHair, fields->m_robotHair, fields->m_spiderHair})
    {
      if (hair)
        hair->resetSim();
    }
  }

  HairNode *makeHair(CCSprite *head, CCNode *behind, IconType type)
  {
    auto hair = HairNode::attach(head, behind, m_firstLayer, m_secondLayer, this->getParent());
    if (!hair)
      return nullptr;

    hair->setGarage(true);
    hair->setIdleWind(true);
    hair->setShouldShow([this, type]
                        { return m_fields->m_iconType == type; });
    return hair;
  }

  void attachBodyHair()
  {
    auto fields = m_fields.self();

    if (m_robotSprite && !fields->m_robotHair)
      fields->m_robotHair = this->makeHair(headOf(m_robotSprite), m_robotSprite, IconType::Robot);

    if (m_spiderSprite && !fields->m_spiderHair)
      fields->m_spiderHair = this->makeHair(headOf(m_spiderSprite), m_spiderSprite, IconType::Spider);
  }

  void attachHair()
  {
    auto fields = m_fields.self();
    if (fields->m_hairEnabled)
      return;
    fields->m_hairEnabled = true;

    fields->m_iconHair = HairNode::attach(m_firstLayer, m_firstLayer, m_firstLayer, m_secondLayer, this->getParent());
    if (auto hair = fields->m_iconHair)
    {
      hair->setGarage(true);
      hair->setIdleWind(true);
      hair->setShouldShow([this]
                          { return showsHeadSprite(m_fields->m_iconType); });
    }

    this->attachBodyHair();
  }
};

// ! --- Garage --- !

class $modify(HairGarageLayer, GJGarageLayer)
{
  bool init()
  {
    if (!GJGarageLayer::init())
      return false;

    if (m_playerObject)
      static_cast<HairSimplePlayer *>(m_playerObject)->attachHair();

    return true;
  }
};
