#include "../presets/Presets.hpp"
#include "../ui/CustomizerPopup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

// ! --- PauseLayer --- !
// Customizer button in the pause menu, changes apply right away thanks to HairConfig::version().
// Next to it a star button switches to the next favorite preset

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

    auto starSprite = CircleButtonSprite::createWithSpriteFrameName("GJ_starsIcon_001.png", 1.1f, CircleBaseColor::Pink,
                                                                    CircleBaseSize::Medium);
    starSprite->setScale(.75f);
    auto star = CCMenuItemSpriteExtra::create(starSprite, this, menu_selector(HairPauseLayer::onNextLook));
    star->setID("next-look-button"_spr);

    if (auto menu = this->getChildByID("right-button-menu"))
    {
      menu->addChild(button);
      menu->addChild(star);
      menu->updateLayout();
      return;
    }

    // Without node IDs: own menu in the bottom right corner
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    auto menu = CCMenu::create();
    menu->setID("hair-menu"_spr);
    menu->setPosition({winSize.width - 30.f, 30.f});
    star->setPosition({0.f, 40.f});
    menu->addChild(button);
    menu->addChild(star);
    this->addChild(menu);
  }

  void onHairSettings(CCObject *)
  {
    if (auto popup = CustomizerPopup::create())
      popup->show();
  }

  void onNextLook(CCObject *)
  {
    if (auto name = presets::applyNextFavorite())
      Notification::create(fmt::format("Look: {}", *name), NotificationIcon::Success)->show();
    else
      Notification::create("Star presets in the Presets menu to switch them here", NotificationIcon::Info)->show();
  }
};
