#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>

using namespace geode::prelude;

// ! --- PauseLayer --- !
// Settings button in the pause menu, changes apply right away thanks to HairConfig::version()

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
    openSettingsPopup(Mod::get());
  }
};
