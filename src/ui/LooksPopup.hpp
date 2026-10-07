#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>

#include <array>
#include <string>
#include <vector>

// ! --- Looks popup --- !
// Picks a preset for each game mode (player 1 and player 2) and for each icon, see Looks.hpp.
// Opened from the General tab of the customizer, and on the Icons tab from the garage.

class LooksPopup : public geode::Popup
{
public:
  enum class Tab
  {
    PlayerOne,
    PlayerTwo,
    Icons,
  };
  static LooksPopup *create(Tab tab = Tab::PlayerOne);

private:
  bool initLooks(Tab tab);
  void buildTabs();
  void buildRows();
  void addSectionLabel(char const *text);
  // A row with arrows cycling through the looks; `current` reads the assignment, `assign` saves it,
  // `unset` is shown while nothing is chosen
  cocos2d::CCNode *createRow(std::string const &label, char const *unset, std::function<std::string()> current,
                             std::function<void(std::string const &)> assign);

  geode::ScrollLayer *m_list = nullptr;
  cocos2d::CCMenu *m_tabMenu = nullptr;
  std::array<ButtonSprite *, 3> m_tabs = {nullptr, nullptr, nullptr};
  cocos2d::CCLabelBMFont *m_hint = nullptr;
  Tab m_tab = Tab::PlayerOne;
  std::vector<std::string> m_options; // "" is the main look, then the presets
};

// The General tab row that opens the popup
cocos2d::CCNode *createLooksRow(float width);
// The General tab row that opens the mod settings, where the emote keys are set
cocos2d::CCNode *createEmoteKeysRow(float width);
