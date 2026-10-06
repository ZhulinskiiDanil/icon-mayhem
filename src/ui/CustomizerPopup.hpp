#pragma once

#include "SettingRow.hpp"
#include "../presets/Presets.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>

// ! --- Customizer popup --- !
// Icon preview on the left (switch game modes, jump, run), settings of the selected
// section on the right. Sections are plain lists of setting keys, see Sections.hpp.

class CustomizerPopup : public geode::Popup
{
public:
  static CustomizerPopup *create();

private:
  bool initCustomizer();

  void buildPreview();
  void buildTabs();
  void showSection(size_t index);
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
  void applyHitboxes();

  // Preview
  cocos2d::CCNode *m_stage = nullptr; // simulation space of the preview hair, moves while running
  SimplePlayer *m_player = nullptr;
  cocos2d::CCLabelBMFont *m_modeLabel = nullptr;
  ButtonSprite *m_runSprite = nullptr;
  cocos2d::CCNode *m_legend = nullptr;
  bool m_showHitboxes = false;

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
};
