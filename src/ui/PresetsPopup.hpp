#pragma once

#include "../presets/Presets.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>

#include <functional>
#include <optional>
#include <vector>

// ! --- Presets popup --- !
// On top, the current look: its preset, whether it changed, Save into it in one tap or Save as.
// On the left your looks (latest first) and the built-in ones, with search. On the right the
// selected preset on a live icon, to load, save over, rename, share or delete.
// `onApplied` runs after the look changed, so the customizer can refresh its rows.

class PresetsPopup : public geode::Popup
{
public:
  static PresetsPopup *create(std::function<void()> onApplied);

private:
  bool initPresets(std::function<void()> onApplied);
  void update(float dt) override;

  void buildCurrentBar();
  void buildDetail();
  void buildTabs();

  // Reads the presets again (after a save, rename, delete or import); `select` picks one by name
  void reload(std::optional<std::string> select = std::nullopt, bool builtIn = false);
  // `keepScroll`: stay where the list was scrolled to (a star, a selection), else from the top
  void refreshList(bool keepScroll = true);
  void refreshCurrent();
  void refreshDetail();
  cocos2d::CCNode *createRow(Preset const &preset);
  void select(Preset const &preset);

  void onTab(cocos2d::CCObject *sender);
  void onSaveCurrent(cocos2d::CCObject *sender);
  void onSaveAs(cocos2d::CCObject *sender);
  void onPaste(cocos2d::CCObject *sender);
  void onEmpty(cocos2d::CCObject *sender);
  void onImportFile(cocos2d::CCObject *sender);
  void onFolder(cocos2d::CCObject *sender);

  void load(Preset const &preset);
  void saveOver(Preset const &preset);
  void rename(Preset const &preset);
  void copy(Preset const &preset);
  void exportFile(Preset const &preset);
  void remove(Preset const &preset);
  // Saves an imported preset under a free name and selects it
  void addImported(Preset preset);
  // After a save from the current bar: show that preset
  void afterSave();
  void applied();

  std::function<void()> m_onApplied;

  std::vector<Preset> m_mine;    // the latest first
  std::vector<Preset> m_builtIn; // by name
  bool m_showBuiltIn = false;
  std::string m_query;
  std::optional<Preset> m_selected;

  // Current look
  cocos2d::CCLabelBMFont *m_currentName = nullptr;
  cocos2d::CCLabelBMFont *m_currentStatus = nullptr;
  unsigned m_seenVersion = 0;
  float m_sinceRefresh = 0.f;

  // List
  cocos2d::CCMenu *m_tabMenu = nullptr;
  geode::TextInput *m_search = nullptr;
  geode::ScrollLayer *m_list = nullptr;
  cocos2d::CCLabelBMFont *m_emptyLabel = nullptr;

  // Selected preset
  SimplePlayer *m_preview = nullptr;
  std::string m_previewLook;
  cocos2d::CCLabelBMFont *m_detailName = nullptr;
  cocos2d::CCLabelBMFont *m_detailTag = nullptr;
  cocos2d::CCLabelBMFont *m_detailHint = nullptr;
  cocos2d::CCMenu *m_detailMenu = nullptr;
};
