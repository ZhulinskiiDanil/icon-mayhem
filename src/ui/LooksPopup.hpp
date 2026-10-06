#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>

#include <array>
#include <string>
#include <vector>

// ! --- Looks popup --- !
// Picks a preset for each game mode, for player 1 and player 2 (switched at the top), see Looks.hpp.
// Opened from the General tab of the customizer.

class LooksPopup : public geode::Popup
{
public:
  static LooksPopup *create();

private:
  bool initLooks();
  void buildPlayerTabs();
  void buildRows();
  // A row with arrows cycling through the looks; `current` reads the assignment, `assign` saves it,
  // `unset` is shown while nothing is chosen
  cocos2d::CCNode *createRow(char const *label, char const *unset, std::function<std::string()> current,
                             std::function<void(std::string const &)> assign);

  geode::ScrollLayer *m_list = nullptr;
  cocos2d::CCMenu *m_playerMenu = nullptr;
  std::array<ButtonSprite *, 2> m_playerTabs = {nullptr, nullptr};
  cocos2d::CCLabelBMFont *m_hint = nullptr;
  bool m_playerTwo = false;
  std::vector<std::string> m_options; // "" is the main look, then the presets
};

// The General tab row that opens the popup
cocos2d::CCNode *createLooksRow(float width);
