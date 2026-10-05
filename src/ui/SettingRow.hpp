#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/SliderNode.hpp>
#include <Geode/ui/TextInput.hpp>

// ! --- Setting row --- !
// One row of the customizer list, built from a mod setting by its type:
// title, toggle, slider with a text input (int / float), arrows (string with options)
// or color picker.
// Writing the setting is enough, HairConfig::version() makes the hair pick it up.

class SettingRow : public cocos2d::CCNode
{
public:
  static SettingRow *create(std::string_view key, float width);

  // Pulls the value from the setting again, after a reset for example
  void refresh();

private:
  bool init(std::shared_ptr<geode::SettingV3> setting, float width);

  void addTitle();
  void addLabel();
  void addToggle();
  void addSlider();
  void addArrows();
  void addColor();

  void onToggle(cocos2d::CCObject *sender);
  void onArrow(cocos2d::CCObject *sender);
  void onColor(cocos2d::CCObject *sender);
  void onInfo(cocos2d::CCObject *sender);

  // Number settings as doubles, so int and float share one slider
  double numberValue() const;
  void setNumberValue(double value);
  double numberMin() const;
  double numberMax() const;
  double numberSnap() const;

  std::shared_ptr<geode::SettingV3> m_setting;
  float m_width = 0.f;

  cocos2d::CCMenu *m_menu = nullptr;
  CCMenuItemToggler *m_toggle = nullptr;
  geode::SliderNode *m_slider = nullptr;
  geode::TextInput *m_input = nullptr;
  cocos2d::CCLabelBMFont *m_valueLabel = nullptr;
  cocos2d::CCSprite *m_colorSprite = nullptr;
};
