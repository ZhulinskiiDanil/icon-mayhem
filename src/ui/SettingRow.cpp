#include "SettingRow.hpp"

#include "../settings/Settings.hpp"

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
  constexpr float kSliderWidth = 70.f;
  constexpr float kInputWidth = 52.f;  // number input on the right, before scaling
  constexpr float kInputScale = .6f;
  constexpr float kChoiceWidth = 90.f; // space for the "< Option >" control
}

// ! --- Creation --- !

SettingRow *SettingRow::create(std::string_view key, float width)
{
  auto def = settings::def(key);
  if (!def)
  {
    log::warn("Customizer: unknown setting '{}'", key);
    return nullptr;
  }

  auto row = new SettingRow();
  if (row->init(def, width))
  {
    row->autorelease();
    return row;
  }
  delete row;
  return nullptr;
}

bool SettingRow::init(settings::Def const *def, float width)
{
  if (!CCNode::init())
    return false;

  m_def = def;
  m_width = width;

  bool const isTitle = m_def->type == settings::Type::Title;
  this->setContentSize({width, isTitle ? kTitleHeight : kRowHeight});
  this->setAnchorPoint({.5f, .5f});
  this->setID(fmt::format("{}-row", m_def->key));

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

  switch (m_def->type)
  {
  case settings::Type::Bool:
    this->addToggle();
    break;
  case settings::Type::Int:
  case settings::Type::Float:
    this->addSlider();
    break;
  case settings::Type::Choice:
    this->addArrows();
    break;
  case settings::Type::Color:
    this->addColor();
    break;
  default:
    break;
  }

  this->refresh();
  return true;
}

// ! --- Layout --- !

void SettingRow::addTitle()
{
  auto label = CCLabelBMFont::create(m_def->name.c_str(), "goldFont.fnt");
  label->setScale(.5f);
  label->setPosition(this->getContentSize() / 2.f);
  this->addChild(label);
}

void SettingRow::addLabel()
{
  float const centerY = this->getContentHeight() / 2.f;

  auto label = CCLabelBMFont::create(m_def->name.c_str(), "bigFont.fnt");
  label->setAnchorPoint({0.f, .5f});
  label->limitLabelWidth(m_width * kLabelWidth, kLabelScale, .1f);
  label->setPosition({kPadding, centerY});
  this->addChild(label);
  m_label = label;

  if (m_def->description.empty())
    return;

  auto infoSprite = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
  infoSprite->setScale(.4f);
  m_info = CCMenuItemSpriteExtra::create(infoSprite, this, menu_selector(SettingRow::onInfo));
  m_info->setPosition({kPadding + label->getScaledContentWidth() + 8.f, centerY});
  m_menu->addChild(m_info);
}

float SettingRow::useAsHeader(std::string const &title, float left, bool dim)
{
  if (!m_label)
    return left;
  float const centerY = this->getContentHeight() / 2.f;
  m_label->removeFromParent();
  m_label = CCLabelBMFont::create(title.c_str(), "goldFont.fnt");
  m_label->setAnchorPoint({0.f, .5f});
  m_label->limitLabelWidth(m_width * kLabelWidth, .5f, .1f);
  m_label->setPosition({left, centerY});
  if (dim)
    m_label->setColor({170, 170, 170});
  this->addChild(m_label);

  float const right = left + m_label->getScaledContentWidth();
  if (m_info)
    m_info->setPosition({right + 8.f, centerY});
  return right;
}

void SettingRow::changed()
{
  if (m_onChange)
    m_onChange();
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
  float const inputWidth = kInputWidth * kInputScale;
  bool const isInt = m_def->type == settings::Type::Int;

  // Typing a value works everywhere, the slider is for quick tweaks with the live preview
  m_input = TextInput::create(kInputWidth, "0");
  m_input->setScale(kInputScale);
  m_input->setPosition({m_width - kPadding - inputWidth / 2.f, centerY});
  this->addChild(m_input);

  m_slider = SliderNode::create([this](SliderNode *, float value)
                                {
                                  double const snap = m_def->step;
                                  double snapped = value;
                                  if (snap > 0.0)
                                    snapped = std::round(snapped / snap) * snap;
                                  this->setNumberValue(snapped);
                                  this->changed();
                                });
  m_slider->setMin(static_cast<float>(m_def->min));
  m_slider->setMax(static_cast<float>(m_def->max));
  if (m_def->step > 0.0)
    m_slider->setSnapStep(static_cast<float>(m_def->step));
  m_slider->setContentSize({kSliderWidth, m_slider->getContentHeight()});
  m_slider->setAnchorPoint({1.f, .5f});
  m_slider->setPosition({m_width - kPadding - inputWidth - 8.f, centerY});
  this->addChild(m_slider);

  m_slider->linkTextInput(m_input, isInt ? 0 : 2);
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
  if (m_toggle)
  {
    m_toggle->toggle(settings::flag(m_def->key));
  }
  else if (m_slider)
  {
    // Also updates the linked input
    m_slider->setValue(static_cast<float>(this->numberValue()));
  }
  else if (m_valueLabel)
  {
    m_valueLabel->setString(settings::text(m_def->key).c_str());
    m_valueLabel->limitLabelWidth(kChoiceWidth - 28.f, .35f, .1f);
  }
  else if (m_colorSprite)
  {
    m_colorSprite->setColor(settings::color(m_def->key));
  }
}

double SettingRow::numberValue() const
{
  return settings::get(m_def->key).asDouble().unwrapOr(0.0);
}

void SettingRow::setNumberValue(double value)
{
  // Clamped and rounded for its type by the store
  settings::set(m_def->key, std::clamp(value, m_def->min, m_def->max));
}

// ! --- Callbacks --- !

void SettingRow::onToggle(CCObject *)
{
  // The toggler flips its state after the callback
  settings::set(m_def->key, !m_toggle->isToggled());
  this->changed();
}

void SettingRow::onArrow(CCObject *sender)
{
  auto const &options = m_def->options;
  if (options.empty())
    return;

  auto const count = static_cast<int>(options.size());
  auto const current = std::find(options.begin(), options.end(), settings::text(m_def->key));
  int index = current == options.end() ? 0 : static_cast<int>(current - options.begin());
  index = (index + static_cast<CCNode *>(sender)->getTag() + count) % count;

  settings::set(m_def->key, options[index]);
  this->refresh();
  this->changed();
}

void SettingRow::onColor(CCObject *)
{
  auto popup = ColorPickPopup::create(settings::color(m_def->key));
  popup->setCallback([self = Ref(this)](ccColor4B const &color)
                     {
                       settings::set(self->m_def->key, "#" + cc3bToHexString(ccColor3B{color.r, color.g, color.b}));
                       self->refresh();
                       self->changed();
                     });
  popup->show();
}

void SettingRow::onInfo(CCObject *)
{
  FLAlertLayer::create(
      m_def->name.c_str(),
      m_def->description.c_str(),
      "OK")
      ->show();
}
