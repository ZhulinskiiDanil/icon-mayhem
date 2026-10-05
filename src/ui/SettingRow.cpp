#include "SettingRow.hpp"

#include <Geode/ui/ColorPickPopup.hpp>

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

// ! --- Constants --- !

namespace
{
  constexpr float kRowHeight = 28.f;
  constexpr float kTitleHeight = 24.f;
  constexpr float kPadding = 8.f;
  constexpr float kLabelScale = .4f;
  constexpr float kLabelWidth = .42f;  // part of the row the setting name may take
  constexpr float kSliderScale = .45f;
  constexpr float kValueWidth = 30.f;  // space for the slider value on the right
  constexpr float kChoiceWidth = 90.f; // space for the "< Option >" control

  template <class S>
  std::shared_ptr<S> as(std::shared_ptr<SettingV3> const &setting)
  {
    return typeinfo_pointer_cast<S>(setting);
  }
}

// ! --- Creation --- !

SettingRow *SettingRow::create(std::string_view key, float width)
{
  auto setting = Mod::get()->getSetting(key);
  if (!setting)
  {
    log::warn("Customizer: unknown setting '{}'", key);
    return nullptr;
  }

  auto row = new SettingRow();
  if (row->init(std::move(setting), width))
  {
    row->autorelease();
    return row;
  }
  delete row;
  return nullptr;
}

bool SettingRow::init(std::shared_ptr<SettingV3> setting, float width)
{
  if (!CCNode::init())
    return false;

  m_setting = std::move(setting);
  m_width = width;

  bool const isTitle = as<TitleSettingV3>(m_setting) != nullptr;
  this->setContentSize({width, isTitle ? kTitleHeight : kRowHeight});
  this->setAnchorPoint({.5f, .5f});
  this->setID(fmt::format("{}-row", m_setting->getKey()));

  m_menu = CCMenu::create();
  m_menu->setPosition({0.f, 0.f});
  m_menu->setContentSize(this->getContentSize());
  this->addChild(m_menu);

  if (isTitle)
  {
    this->addTitle();
    return true;
  }

  this->addLabel();

  if (as<BoolSettingV3>(m_setting))
    this->addToggle();
  else if (as<IntSettingV3>(m_setting) || as<FloatSettingV3>(m_setting))
    this->addSlider();
  else if (as<StringSettingV3>(m_setting))
    this->addArrows();
  else if (as<Color3BSettingV3>(m_setting))
    this->addColor();

  this->refresh();
  return true;
}

// ! --- Layout --- !

void SettingRow::addTitle()
{
  auto label = CCLabelBMFont::create(m_setting->getDisplayName().c_str(), "goldFont.fnt");
  label->setScale(.5f);
  label->setPosition(this->getContentSize() / 2.f);
  this->addChild(label);
}

void SettingRow::addLabel()
{
  float const centerY = this->getContentHeight() / 2.f;

  auto label = CCLabelBMFont::create(m_setting->getDisplayName().c_str(), "bigFont.fnt");
  label->setAnchorPoint({0.f, .5f});
  label->limitLabelWidth(m_width * kLabelWidth, kLabelScale, .1f);
  label->setPosition({kPadding, centerY});
  this->addChild(label);

  if (!m_setting->getDescription())
    return;

  auto infoSprite = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
  infoSprite->setScale(.4f);
  auto info = CCMenuItemSpriteExtra::create(infoSprite, this, menu_selector(SettingRow::onInfo));
  info->setPosition({kPadding + label->getScaledContentWidth() + 8.f, centerY});
  m_menu->addChild(info);
}

void SettingRow::addToggle()
{
  m_toggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(SettingRow::onToggle), .55f);
  m_toggle->setPosition({m_width - kPadding - 12.f, this->getContentHeight() / 2.f});
  m_menu->addChild(m_toggle);
}

void SettingRow::addSlider()
{
  float const centerY = this->getContentHeight() / 2.f;

  m_slider = Slider::create(this, menu_selector(SettingRow::onSlider));
  m_slider->setScale(kSliderScale);
  m_slider->setPosition({m_width - kPadding - kValueWidth - 52.f, centerY});
  this->addChild(m_slider);

  m_valueLabel = CCLabelBMFont::create("0", "bigFont.fnt");
  m_valueLabel->setAnchorPoint({1.f, .5f});
  m_valueLabel->setScale(.32f);
  m_valueLabel->setPosition({m_width - kPadding, centerY});
  this->addChild(m_valueLabel);
}

void SettingRow::addArrows()
{
  float const centerY = this->getContentHeight() / 2.f;
  float const right = m_width - kPadding;

  auto makeArrow = [&](bool left)
  {
    auto sprite = CCSprite::createWithSpriteFrameName("navArrowBtn_001.png");
    sprite->setScale(.3f);
    sprite->setFlipX(left);
    auto arrow = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(SettingRow::onArrow));
    arrow->setTag(left ? -1 : 1);
    m_menu->addChild(arrow);
    return arrow;
  };

  makeArrow(true)->setPosition({right - kChoiceWidth + 6.f, centerY});
  makeArrow(false)->setPosition({right - 6.f, centerY});

  m_valueLabel = CCLabelBMFont::create("", "bigFont.fnt");
  m_valueLabel->setScale(.35f);
  m_valueLabel->setPosition({right - kChoiceWidth / 2.f, centerY});
  this->addChild(m_valueLabel);
}

void SettingRow::addColor()
{
  m_colorSprite = CCSprite::createWithSpriteFrameName("GJ_colorBtn_001.png");
  m_colorSprite->setScale(.55f);
  auto button = CCMenuItemSpriteExtra::create(m_colorSprite, this, menu_selector(SettingRow::onColor));
  button->setPosition({m_width - kPadding - 12.f, this->getContentHeight() / 2.f});
  m_menu->addChild(button);
}

// ! --- Values --- !

void SettingRow::refresh()
{
  if (auto setting = as<BoolSettingV3>(m_setting); setting && m_toggle)
  {
    m_toggle->toggle(setting->getValue());
  }
  else if (m_slider)
  {
    float const range = static_cast<float>(this->numberMax() - this->numberMin());
    float const value = range > 0.f ? static_cast<float>(this->numberValue() - this->numberMin()) / range : 0.f;
    m_slider->m_touchLogic->m_thumb->setValue(std::clamp(value, 0.f, 1.f));
    m_slider->updateBar();
    this->updateValueLabel();
  }
  else if (auto setting = as<StringSettingV3>(m_setting); setting && m_valueLabel)
  {
    m_valueLabel->setString(setting->getValue().c_str());
    m_valueLabel->limitLabelWidth(kChoiceWidth - 28.f, .35f, .1f);
  }
  else if (auto setting = as<Color3BSettingV3>(m_setting); setting && m_colorSprite)
  {
    m_colorSprite->setColor(setting->getValue());
  }
}

double SettingRow::numberValue() const
{
  if (auto setting = as<IntSettingV3>(m_setting))
    return static_cast<double>(setting->getValue());
  if (auto setting = as<FloatSettingV3>(m_setting))
    return setting->getValue();
  return 0.0;
}

void SettingRow::setNumberValue(double value)
{
  value = std::clamp(value, this->numberMin(), this->numberMax());
  if (auto setting = as<IntSettingV3>(m_setting))
    setting->setValue(static_cast<int64_t>(std::llround(value)));
  else if (auto setting = as<FloatSettingV3>(m_setting))
    setting->setValue(value);
}

double SettingRow::numberMin() const
{
  if (auto setting = as<IntSettingV3>(m_setting))
    return static_cast<double>(setting->getMinValue().value_or(0));
  if (auto setting = as<FloatSettingV3>(m_setting))
    return setting->getMinValue().value_or(0.0);
  return 0.0;
}

double SettingRow::numberMax() const
{
  if (auto setting = as<IntSettingV3>(m_setting))
    return static_cast<double>(setting->getMaxValue().value_or(100));
  if (auto setting = as<FloatSettingV3>(m_setting))
    return setting->getMaxValue().value_or(1.0);
  return 1.0;
}

double SettingRow::numberSnap() const
{
  if (auto setting = as<IntSettingV3>(m_setting))
    return static_cast<double>(std::max<int64_t>(setting->getSliderSnap(), 1));
  if (auto setting = as<FloatSettingV3>(m_setting))
    return setting->getSliderSnap();
  return 0.0;
}

void SettingRow::updateValueLabel()
{
  if (!m_valueLabel)
    return;

  if (as<IntSettingV3>(m_setting))
    m_valueLabel->setString(fmt::format("{}", std::llround(this->numberValue())).c_str());
  else
    m_valueLabel->setString(fmt::format("{:.2f}", this->numberValue()).c_str());
}

// ! --- Callbacks --- !

void SettingRow::onToggle(CCObject *)
{
  // The toggler flips its state after the callback
  if (auto setting = as<BoolSettingV3>(m_setting))
    setting->setValue(!m_toggle->isToggled());
}

void SettingRow::onSlider(CCObject *)
{
  double const min = this->numberMin();
  double const max = this->numberMax();
  double value = min + m_slider->m_touchLogic->m_thumb->getValue() * (max - min);

  double const snap = this->numberSnap();
  if (snap > 0.0)
    value = std::round(value / snap) * snap;

  this->setNumberValue(value);
  this->updateValueLabel();
}

void SettingRow::onArrow(CCObject *sender)
{
  auto setting = as<StringSettingV3>(m_setting);
  if (!setting)
    return;

  auto options = setting->getEnumOptions();
  if (!options || options->empty())
    return;

  auto const count = static_cast<int>(options->size());
  auto const current = std::find(options->begin(), options->end(), setting->getValue());
  int index = current == options->end() ? 0 : static_cast<int>(current - options->begin());
  index = (index + static_cast<CCNode *>(sender)->getTag() + count) % count;

  setting->setValue((*options)[index]);
  this->refresh();
}

void SettingRow::onColor(CCObject *)
{
  auto setting = as<Color3BSettingV3>(m_setting);
  if (!setting)
    return;

  auto popup = ColorPickPopup::create(setting->getValue());
  popup->setCallback([self = Ref(this), setting](ccColor4B const &color)
                     {
                       setting->setValue(ccColor3B{color.r, color.g, color.b});
                       self->refresh();
                     });
  popup->show();
}

void SettingRow::onInfo(CCObject *)
{
  FLAlertLayer::create(
      m_setting->getDisplayName().c_str(),
      m_setting->getDescription().value_or("").c_str(),
      "OK")
      ->show();
}
