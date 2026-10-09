#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>

#include <array>
#include <string>
#include <vector>

// ! --- Looks popup --- !
// Picks a preset for each game mode, for player 1 and player 2, see Looks.hpp. Presets of single
// icons are linked in the Linker. Opened from the General tab of the customizer.

class LooksPopup : public geode::Popup
{
public:
  enum class Tab
  {
    PlayerOne,
    PlayerTwo,
  };
  static LooksPopup *create(Tab tab = Tab::PlayerOne);

private:
  bool initLooks(Tab tab);
  void buildTabs();
  void buildRows();
  void addSectionLabel(char const *text);
  // A row with the chosen preset in a box, a tap opens the list with search; `current` reads the
  // assignment, `assign` saves it, `unset` is shown while nothing is chosen
  cocos2d::CCNode *createRow(std::string const &label, char const *unset, std::function<std::string()> current,
                             std::function<void(std::string const &)> assign);

  geode::ScrollLayer *m_list = nullptr;
  cocos2d::CCMenu *m_tabMenu = nullptr;
  std::array<ButtonSprite *, 2> m_tabs = {nullptr, nullptr};
  cocos2d::CCLabelBMFont *m_hint = nullptr;
  Tab m_tab = Tab::PlayerOne;
};

// The General tab row that opens the popup
cocos2d::CCNode *createLooksRow(float width);
// The General tab row that opens the mod settings, where the emote keys are set
cocos2d::CCNode *createEmoteKeysRow(float width);
