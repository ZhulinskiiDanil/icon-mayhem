#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>

#include <string>

// ! --- Players --- !
// The other players of the level on Globed, each in their look: save it, try it on, like it, or
// gift them yours. Looks gifted to you wait on the top. Opened from the pause menu.

class PlayersPopup : public geode::Popup
{
public:
  static PlayersPopup *create();

private:
  bool initPlayers();
  void update(float dt) override;
  void rebuild();
  // The list again on the next frame (after a button of a row was tapped)
  void rebuildSoon();
  // What the list shows, to rebuild it only when something changed
  std::string signature() const;

  cocos2d::CCNode *createPreview(int cube, cocos2d::ccColor3B color1, cocos2d::ccColor3B color2, bool glow,
                                 cocos2d::ccColor3B glowColor, std::string const &look, bool yours = false);
  void saveLook(std::string const &look, std::string const &owner);
  void toggleTryOn(std::string const &look);

  geode::ScrollLayer *m_list = nullptr;
  std::string m_shown;
  float m_sinceCheck = 0.f;
};
