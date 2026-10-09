#include "SimplePlayerHair.hpp"
#include "../presets/Looks.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/SimplePlayer.hpp>

using namespace geode::prelude;

// ! --- Helpers --- !

namespace
{
  // A ship, UFO or jetpack preview is the vehicle alone, there is no head to put hair on
  bool showsHeadSprite(IconType type)
  {
    return type == IconType::Cube || type == IconType::Ball || type == IconType::Wave || type == IconType::Swing;
  }

  CCSprite *headOf(GJRobotSprite *body)
  {
    return body->m_headSprite ? static_cast<CCSprite *>(body->m_headSprite) : body;
  }
}

GameMode gameModeOf(IconType type)
{
  switch (type)
  {
  case IconType::Ship:
    return GameMode::Ship;
  case IconType::Ball:
    return GameMode::Ball;
  case IconType::Ufo:
    return GameMode::Ufo;
  case IconType::Wave:
    return GameMode::Wave;
  case IconType::Robot:
    return GameMode::Robot;
  case IconType::Spider:
    return GameMode::Spider;
  case IconType::Swing:
    return GameMode::Swing;
  case IconType::Jetpack:
    return GameMode::Jetpack;
  default:
    return GameMode::Cube;
  }
}

// ! --- SimplePlayer --- !

class $modify(HairSimplePlayer, SimplePlayer)
{
  struct Fields
  {
    IconType m_iconType = IconType::Cube;
    int m_iconId = -1;
    bool m_hairEnabled = false;
    PreviewPlace m_place = PreviewPlace::Customizer;
    SimplePlayerHair m_hair;
  };

  void updatePlayerFrame(int id, IconType type)
  {
    SimplePlayer::updatePlayerFrame(id, type);

    auto fields = m_fields.self();
    fields->m_iconType = type;
    fields->m_iconId = id;

    // Robot and spider sprites are created lazily on the first switch to them
    if (fields->m_hairEnabled)
      this->attachBodyHair();

    for (auto hair : {fields->m_hair.icon, fields->m_hair.robot, fields->m_hair.spider})
    {
      if (hair)
        hair->resetSim();
    }
  }

  HairNode *makeHair(CCSprite *head, CCNode *behind, std::function<bool(IconType)> showFor)
  {
    auto hair = HairNode::attach(head, behind, m_firstLayer, m_secondLayer, this->getParent());
    if (!hair)
      return nullptr;

    auto const place = m_fields->m_place;
    hair->setGarage(place == PreviewPlace::Garage);
    hair->setMenu(place == PreviewPlace::Menu);
    // A light breeze keeps the garage and menus alive, the customizer preview stays calm to show the real drape
    hair->setIdleWind(place != PreviewPlace::Customizer);
    // Outside the customizer the icon wears its own look, like in a level
    if (place == PreviewPlace::Garage || place == PreviewPlace::Menu)
    {
      // The icon the player wears: the frame a preview was last given can be stale (More Icons
      // swaps the sprites of a preview without it)
      hair->setLook([this]
                    {
                      auto const mode = gameModeOf(m_fields->m_iconType);
                      return looks::wornLook(false, mode);
                    });
    }
    hair->setShouldShow([this, showFor = std::move(showFor)]
                        { return showFor(m_fields->m_iconType); });
    hair->setGameMode([this]
                      { return gameModeOf(m_fields->m_iconType); });
    return hair;
  }

  void attachBodyHair()
  {
    auto &hair = m_fields->m_hair;

    if (m_robotSprite && !hair.robot)
    {
      hair.robot = this->makeHair(headOf(m_robotSprite), m_robotSprite, [](IconType type)
                                  { return type == IconType::Robot; });
    }

    if (m_spiderSprite && !hair.spider)
    {
      hair.spider = this->makeHair(headOf(m_spiderSprite), m_spiderSprite, [](IconType type)
                                   { return type == IconType::Spider; });
    }
  }

  void attachHair(PreviewPlace place)
  {
    auto fields = m_fields.self();
    if (fields->m_hairEnabled)
      return;
    fields->m_hairEnabled = true;
    fields->m_place = place;

    fields->m_hair.icon = this->makeHair(m_firstLayer, m_firstLayer, showsHeadSprite);
    if (auto hair = fields->m_hair.icon)
    {
      hair->setBoxHead([this]
                       { return m_fields->m_iconType == IconType::Cube; });
    }
    this->attachBodyHair();
  }
};

// ! --- API --- !

void attachSimplePlayerHair(SimplePlayer *player, PreviewPlace place)
{
  if (player)
    static_cast<HairSimplePlayer *>(player)->attachHair(place);
}

SimplePlayerHair getSimplePlayerHair(SimplePlayer *player)
{
  if (!player)
    return {};
  return static_cast<HairSimplePlayer *>(player)->m_fields->m_hair;
}
