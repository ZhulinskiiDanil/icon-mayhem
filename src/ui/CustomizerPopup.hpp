#pragma once

#include "SettingRow.hpp"
#include "Sections.hpp"
#include "../presets/Presets.hpp"

#include <set>
#include <string>

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>

// ! --- Customizer popup --- !
// Icon preview on the left (switch game modes, jump, run), settings of the selected
// section on the right. Sections are plain lists of setting keys, see Sections.hpp.
// A tab is a list of blocks (Cape, Pet, Bangs...): each folds into one header with its switch,
// a block that is off shows nothing more, fine tuning waits behind "Fine tuning". "On now" lists
// the blocks that are on, from every tab.

class CustomizerPopup : public geode::Popup
{
public:
  static CustomizerPopup *create();

private:
  bool initCustomizer();

  void buildPreview();
  void buildTabs();
  void showSection(size_t index);
  // Everything that is on, from every tab
  void showOnNow();
  // The list again for what is shown now (a tab, the search, on now), `keepScroll` where it was
  void rebuildList(bool keepScroll);
  void addGroup(CustomizerGroup const &group, std::string const &title);
  cocos2d::CCNode *createGroupHeader(CustomizerGroup const &group, std::string const &title, bool open, bool expandable);
  cocos2d::CCNode *createFineRow(CustomizerGroup const &group, size_t count, bool open);
  bool isOpen(std::string const &id) const;
  void setOpen(std::string const &id, bool open);
  void onOnNow(cocos2d::CCObject *sender);
  // Every setting of every tab whose name or description has the query in it
  void showSearch(std::string const &query);
  void clearList();
  void addRow(char const *key);
  void updatePreviewIcon();

  void update(float dt) override;

  void onTab(cocos2d::CCObject *sender);
  void onMode(cocos2d::CCObject *sender);
  void onJump(cocos2d::CCObject *sender);
  void onSurprise(cocos2d::CCObject *sender);
  void onUndo(cocos2d::CCObject *sender);
  void onColors(cocos2d::CCObject *sender);
  void onMatch(cocos2d::CCObject *sender);
  // Applies a look from a button, the change lands on the undo stack like any other
  void applyLook(Preset const &preset);
  void trackUndo(float dt);
  void onRun(cocos2d::CCObject *sender);
  void onReset(cocos2d::CCObject *sender);
  void onHitboxes(cocos2d::CCObject *sender);
  void onPresets(cocos2d::CCObject *sender);
  // Main | Icon: edit the main look or the look of the icon shown in the preview
  void onLookSwitch(cocos2d::CCObject *sender);
  void refreshLookSwitch();
  // Back to the main look, asking to save the icon look first; `then` runs after
  void endIconEdit(std::function<void()> then);
  void onClose(cocos2d::CCObject *sender) override;
  void onSave(cocos2d::CCObject *sender);
  // The preset name on top: green when saved, yellow with a star when changed
  void refreshLookLabel();
  void applyHitboxes();

  // Preview
  cocos2d::CCNode *m_stage = nullptr; // simulation space of the preview hair, moves while running
  SimplePlayer *m_player = nullptr;
  cocos2d::CCLabelBMFont *m_modeLabel = nullptr;
  cocos2d::CCNode *m_legend = nullptr;
  bool m_showHitboxes = false;
  cocos2d::CCLabelBMFont *m_lookLabel = nullptr;
  cocos2d::CCMenu *m_lookSwitch = nullptr;
  ButtonSprite *m_mainSprite = nullptr;
  ButtonSprite *m_iconSprite = nullptr;
  bool m_editingIcon = false;
  std::string m_iconLook; // the preset of the icon being edited
  unsigned m_lookVersion = 0;
  float m_lookTimer = 0.f;

  // Undo: every burst of changes pushes the look from before it
  std::vector<Preset> m_undo;
  Preset m_stable;            // the look after the last burst settled
  unsigned m_seenVersion = 0; // settings version last looked at
  float m_quiet = 0.f;        // s since the last change
  bool m_changing = false;    // a burst is going on
  size_t m_mode = 0;
  bool m_running = false;
  float m_runDistance = 0.f;
  float m_jumpTime = -1.f; // < 0 while on the ground
  float m_spin = 0.f;      // degrees, cube rotation that stays after a jump

  // Settings
  cocos2d::CCMenu *m_tabMenu = nullptr;
  geode::ScrollLayer *m_list = nullptr;
  std::vector<SettingRow *> m_rows;
  std::vector<char const *> m_shownKeys; // what Reset resets
  size_t m_section = 0;
  geode::TextInput *m_search = nullptr;
  std::string m_query;
  bool m_onNow = false;
  ButtonSprite *m_onNowSprite = nullptr;
  std::set<std::string> m_open; // open blocks, and "<block>+fine" for their fine tuning
  bool m_rebuildPending = false; // rebuilt next frame, never inside the touch of a row
};
