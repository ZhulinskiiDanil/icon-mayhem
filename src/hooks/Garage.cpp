#include "SimplePlayerHair.hpp"
#include "../ui/CustomizerPopup.hpp"
#include "../ui/LinkerPopup.hpp"
#include "SimplePlayerHair.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GJGarageLayer.hpp>

using namespace geode::prelude;

// ! --- Garage --- !
// The preview wears the look of the selected icon. Next to it: the customizer, and the look of this icon

class $modify(HairGarageLayer, GJGarageLayer)
{
  bool init()
  {
    if (!GJGarageLayer::init())
      return false;

    attachSimplePlayerHair(m_playerObject, PreviewPlace::Garage);

    // Next to the icon: the customizer (no level needed) and the look of this icon
    auto menu = CCMenu::create();
    menu->setID("icon-mayhem-menu"_spr);
    menu->setPosition(m_playerObject->getPosition() + CCPoint{48.f, 0.f});
    this->addChild(menu);

    if (auto icon = CCSprite::create("hair-btn.png"_spr))
    {
      auto sprite = CircleButtonSprite::create(icon, CircleBaseColor::Green, CircleBaseSize::Small);
      sprite->setScale(.7f);
      auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(HairGarageLayer::onCustomizer));
      button->setID("customizer-button"_spr);
      button->setPosition({0.f, 14.f});
      menu->addChild(button);
    }
    if (auto icon = CCSprite::create("icon-link.png"_spr))
    {
      // The Linker: a chain, the presets linked to icons
      auto sprite = CircleButtonSprite::create(icon, CircleBaseColor::Pink, CircleBaseSize::Small);
      sprite->setScale(.7f);
      auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(HairGarageLayer::onIconLooks));
      button->setID("icon-looks-button"_spr);
      button->setPosition({0.f, -14.f});
      menu->addChild(button);
    }

    return true;
  }

  void onCustomizer(CCObject *)
  {
    if (auto popup = CustomizerPopup::create())
      popup->show();
  }

  void onIconLooks(CCObject *)
  {
    // The Linker on the tab of the garage
    if (auto popup = LinkerPopup::create(gameModeOf(m_iconType)))
      popup->show();
  }
};
