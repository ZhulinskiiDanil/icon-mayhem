#include "SimplePlayerHair.hpp"
#include "../ui/LooksPopup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GJGarageLayer.hpp>

using namespace geode::prelude;

// ! --- Garage --- !
// The preview wears the look of the selected icon; the button next to it picks that look

class $modify(HairGarageLayer, GJGarageLayer)
{
  bool init()
  {
    if (!GJGarageLayer::init())
      return false;

    attachSimplePlayerHair(m_playerObject, PreviewPlace::Garage);

    if (auto icon = CCSprite::create("hair-btn.png"_spr))
    {
      auto sprite = CircleButtonSprite::create(icon, CircleBaseColor::Pink, CircleBaseSize::Small);
      sprite->setScale(.7f);
      auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(HairGarageLayer::onIconLooks));
      button->setID("icon-looks-button"_spr);

      auto menu = CCMenu::create();
      menu->setID("icon-looks-menu"_spr);
      menu->setPosition(m_playerObject->getPosition() + CCPoint{48.f, 0.f});
      menu->addChild(button);
      this->addChild(menu);
    }

    return true;
  }

  void onIconLooks(CCObject *)
  {
    if (auto popup = LooksPopup::create(LooksPopup::Tab::Icons))
      popup->show();
  }
};
