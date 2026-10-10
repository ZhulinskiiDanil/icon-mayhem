#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/SliderNode.hpp>
#include <Geode/ui/TextInput.hpp>

#include <array>
#include <functional>

namespace settings
{
  struct Def;
}

// ! --- Setting row --- !
// One row of the customizer list, built from a setting (look.json or Geode's page) by its type:
// title, toggle, slider with a text input (int / float), arrows (string with options)
// or color picker.
// Writing the setting is enough, HairConfig::version() makes the hair pick it up.

class SettingRow : public cocos2d::CCNode
{
public:
  static SettingRow *create(std::string_view key, float width);

  // Pulls the value from the setting again, after a reset for example
  void refresh();
  // Runs after the player changed the value
  void setOnChange(std::function<void()> onChange) { m_onChange = std::move(onChange); }
  // Runs when the row changes its height (a color picker opened or closed): the list lays out again
  void setOnResize(std::function<void()> onResize) { m_onResize = std::move(onResize); }
  // The row heads a block of settings: the block name in gold instead of the setting name,
  // starting at `left`. Gives where the name ends
  float useAsHeader(std::string const &title, float left, bool dim);

private:
  bool init(settings::Def const *def, float width);

  void addTitle();
  void addLabel();
  void addToggle();
  void addSlider();
  void addArrows();
  void addColor();
  // Under a color row: a palette, the hex code and hue / saturation / brightness sliders, the
  // icon changes while they move
  void addColorPicker();
  void setColor(cocos2d::ccColor3B color, bool fromSliders);
  // The middle of the main line (the picker hangs under it)
  float lineY() const;

  void onToggle(cocos2d::CCObject *sender);
  void onArrow(cocos2d::CCObject *sender);
  void onColor(cocos2d::CCObject *sender);
  void onSwatch(cocos2d::CCObject *sender);
  void onInfo(cocos2d::CCObject *sender);

  // Number settings as doubles, so int and float share one slider
  double numberValue() const;
  void setNumberValue(double value);

  void changed();

  settings::Def const *m_def = nullptr;
  std::function<void()> m_onChange;
  std::function<void()> m_onResize;
  cocos2d::CCLabelBMFont *m_label = nullptr;
  CCMenuItemSpriteExtra *m_info = nullptr;
  float m_width = 0.f;

  cocos2d::CCMenu *m_menu = nullptr;
  CCMenuItemToggler *m_toggle = nullptr;
  geode::SliderNode *m_slider = nullptr;
  geode::TextInput *m_input = nullptr;
  cocos2d::CCLabelBMFont *m_valueLabel = nullptr;
  cocos2d::CCSprite *m_colorSprite = nullptr;
  std::array<geode::SliderNode *, 3> m_hsv{}; // hue, saturation, brightness
  std::array<float, 3> m_hsvValue{};           // kept here: a gray has no hue of its own
  geode::TextInput *m_hexInput = nullptr;
};
