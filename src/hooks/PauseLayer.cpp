#include "Globed.hpp"
#include "../ui/Buttons.hpp"
#include "../ui/CustomizerPopup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

// ! --- PauseLayer --- !
// One button in the pause menu: the customizer, changes apply right away thanks to
// HairConfig::version(). Players on Globed and gifts are in the customizer; the button wears a
// little badge when there are some

class $modify(HairPauseLayer, PauseLayer)
{
  void customSetup()
  {
    PauseLayer::customSetup();

    auto icon = CCSprite::create("hair-btn.png"_spr);
    if (!icon)
      return;

    auto sprite = CircleButtonSprite::create(icon, CircleBaseColor::Green, CircleBaseSize::Medium);
    sprite->setScale(.75f);

    // A gift waiting shows as a gift, players as people
    bool const gifts = !globedGifts().empty();
    if (gifts || !globedPeers().empty())
    {
      float const size = sprite->getContentSize().width;
      auto badge = iconButton(gifts ? "gift" : "people", gifts ? CircleBaseColor::Green : CircleBaseColor::Pink, size * .42f);
      badge->setPosition({size * .86f, size * .86f});
      badge->setID("players-badge"_spr);
      sprite->addChild(badge);
    }

    auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(HairPauseLayer::onHairSettings));
    button->setID("hair-settings-button"_spr);

    if (auto menu = this->getChildByID("right-button-menu"))
    {
      menu->addChild(button);
      menu->updateLayout();
      return;
    }

    // Without node IDs: own menu in the bottom right corner
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    auto menu = CCMenu::create();
    menu->setID("hair-menu"_spr);
    menu->setPosition({winSize.width - 30.f, 30.f});
    menu->addChild(button);
    this->addChild(menu);
  }

  void onHairSettings(CCObject *)
  {
    if (auto popup = CustomizerPopup::create())
      popup->show();
  }
};
