#pragma once

#include "SettingRow.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>

// ! --- Customizer popup --- !
// Icon preview on the left (switch game modes, jump, run), settings of the selected
// section on the right. Sections are plain lists of setting keys, see kSections.

class CustomizerPopup : public geode::Popup
{
public:
  static CustomizerPopup *create();

private:
  bool initCustomizer();

  void buildPreview();
  void buildTabs();
  void showSection(size_t index);
  void updatePreviewIcon();

  void update(float dt) override;

  void onTab(cocos2d::CCObject *sender);
  void onMode(cocos2d::CCObject *sender);
  void onJump(cocos2d::CCObject *sender);
  void onRun(cocos2d::CCObject *sender);
  void onReset(cocos2d::CCObject *sender);
  void onHitboxes(cocos2d::CCObject *sender);
  void applyHitboxes();

  // Preview
  cocos2d::CCNode *m_stage = nullptr; // simulation space of the preview hair, moves while running
  SimplePlayer *m_player = nullptr;
  cocos2d::CCLabelBMFont *m_modeLabel = nullptr;
  ButtonSprite *m_runSprite = nullptr;
  cocos2d::CCNode *m_legend = nullptr;
  bool m_showHitboxes = false;
  size_t m_mode = 0;
  bool m_running = false;
  float m_runDistance = 0.f;
  float m_jumpTime = -1.f; // < 0 while on the ground
  float m_spin = 0.f;      // degrees, cube rotation that stays after a jump

  // Settings
  cocos2d::CCMenu *m_tabMenu = nullptr;
  geode::ScrollLayer *m_list = nullptr;
  std::vector<SettingRow *> m_rows;
  size_t m_section = 0;
};
