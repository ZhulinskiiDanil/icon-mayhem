#include "Globed.hpp"
#include "../ui/Buttons.hpp"
#include "../ui/CustomizerPopup.hpp"
#include "../ui/PlayersPopup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

// ! --- PauseLayer --- !
// Customizer button in the pause menu, changes apply right away thanks to HairConfig::version().
// With other players on Globed (or a gift waiting) a Players button joins it

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
    auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(HairPauseLayer::onHairSettings));
    button->setID("hair-settings-button"_spr);

    // A gift waiting shows as a gift, players as people
    CCMenuItemSpriteExtra *players = nullptr;
    bool const gifts = !globedGifts().empty();
    if (gifts || !globedPeers().empty())
    {
      players = CCMenuItemSpriteExtra::create(iconButton(gifts ? "gift" : "people", gifts ? CircleBaseColor::Green : CircleBaseColor::Pink,
                                                         sprite->getScaledContentSize().width),
                                              this, menu_selector(HairPauseLayer::onPlayers));
      players->setID("players-button"_spr);
    }

    if (auto menu = this->getChildByID("right-button-menu"))
    {
      menu->addChild(button);
      if (players)
        menu->addChild(players);
      menu->updateLayout();
      return;
    }

    // Without node IDs: own menu in the bottom right corner
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    auto menu = CCMenu::create();
    menu->setID("hair-menu"_spr);
    menu->setPosition({winSize.width - 30.f, 30.f});
    menu->addChild(button);
    if (players)
    {
      players->setPosition({0.f, 40.f});
      menu->addChild(players);
    }
    this->addChild(menu);
  }

  void onPlayers(CCObject *)
  {
    if (auto popup = PlayersPopup::create())
      popup->show();
  }

  void onHairSettings(CCObject *)
  {
    if (auto popup = CustomizerPopup::create())
      popup->show();
  }
};
