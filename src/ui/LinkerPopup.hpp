#pragma once

#include "../hair/HairConfig.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <string>
#include <vector>

// ! --- Linker --- !
// Links presets to icons: the icons of a game mode in a grid like the garage, the selected one on
// the right, alive in its look, with the preset it is linked to. Many at once: the brush links
// every tapped icon to its preset, and "Link all shown" links what the search found. A linked icon wears its preset
// over the look of its game mode (see looks::lookFor). Opened from the garage and the General tab.

class LinkerPopup : public geode::Popup
{
public:
  // Opens on `mode` with the icon you wear selected
  static LinkerPopup *create(GameMode mode = GameMode::Cube);

private:
  bool initLinker(GameMode mode);

  void buildModeTabs();
  void buildGrid();
  void buildDetail();
  void refreshDetail();
  void selectMode(GameMode mode);
  // An icon of the game by its number, or of More Icons by its name
  struct IconRef
  {
    int id = 0;
    std::string custom;
    bool operator==(IconRef const &) const = default;
  };

  void select(IconRef const &icon);
  // The icons shown: unlocked ones (or all of More Icons), or only the linked ones
  void collectIcons();
  void showPageOf(IconRef const &icon);
  void buildSourceTabs();
  IconRef wornIcon() const;
  std::string linkOf(IconRef const &icon) const;
  void setLink(IconRef const &icon, std::string const &preset);
  std::string nameOf(IconRef const &icon) const;
  void rebuildPreview();

  void onPage(cocos2d::CCObject *sender);
  void onLinkedOnly(cocos2d::CCObject *sender);
  void onMyIcon(cocos2d::CCObject *sender);
  void onPreset(cocos2d::CCObject *sender);
  void onBrush(cocos2d::CCObject *sender);
  void onLinkAll(cocos2d::CCObject *sender);
  void refreshBrush();

  GameMode m_mode = GameMode::Cube;
  IconRef m_selected;
  bool m_custom = false; // showing the icons of More Icons
  size_t m_page = 0;
  bool m_linkedOnly = false;
  std::vector<IconRef> m_icons;
  cocos2d::CCMenu *m_sourceMenu = nullptr;

  cocos2d::CCMenu *m_modeMenu = nullptr;
  cocos2d::CCMenu *m_grid = nullptr;
  cocos2d::CCLabelBMFont *m_pageLabel = nullptr;
  cocos2d::CCLabelBMFont *m_emptyLabel = nullptr;

  SimplePlayer *m_preview = nullptr;
  std::string m_previewLook;
  cocos2d::CCLabelBMFont *m_iconName = nullptr;
  cocos2d::CCLabelBMFont *m_linkName = nullptr;
  cocos2d::CCLabelBMFont *m_fallback = nullptr;
  // Linking many at once
  std::string m_brush; // the preset every tapped icon gets, "" while off
  std::string m_search;
  geode::TextInput *m_searchInput = nullptr;
  ButtonSprite *m_brushSprite = nullptr;
  CCMenuItemSpriteExtra *m_linkAll = nullptr;
};

// The General tab row that opens the Linker
cocos2d::CCNode *createLinkerRow(float width);
