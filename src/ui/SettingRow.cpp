#include "SettingRow.hpp"

#include "../settings/Settings.hpp"


#include <algorithm>
#include <array>
#include <cmath>
#include <set>

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

  // The color picker under a color row
  constexpr float kPickerHeight = 78.f;
  constexpr float kSwatchSize = 13.f;
  constexpr float kSwatchStep = 17.f;
  constexpr float kPickerLineStep = 16.f;
  constexpr std::array<ccColor3B, 12> kPalette{
      ccColor3B{255, 255, 255}, ccColor3B{43, 41, 41}, ccColor3B{230, 57, 70}, ccColor3B{255, 143, 177},
      ccColor3B{255, 159, 67}, ccColor3B{255, 216, 74}, ccColor3B{126, 214, 105}, ccColor3B{72, 202, 228},
      ccColor3B{70, 110, 230}, ccColor3B{150, 95, 220}, ccColor3B{140, 90, 60}, ccColor3B{160, 160, 170}};

  // Color rows with the picker open, by setting key: they stay open when the list is built again
  std::set<std::string> s_openPickers;

  ccColor3B fromHsv(float hue, float saturation, float value)
  {
    float const c = value * saturation;
    float const h = std::fmod(std::max(hue, 0.f), 360.f) / 60.f;
    float const x = c * (1.f - std::abs(std::fmod(h, 2.f) - 1.f));
    float r = 0.f, g = 0.f, b = 0.f;
    if (h < 1.f)
      r = c, g = x;
    else if (h < 2.f)
      r = x, g = c;
    else if (h < 3.f)
      g = c, b = x;
    else if (h < 4.f)
      g = x, b = c;
    else if (h < 5.f)
      r = x, b = c;
    else
      r = c, b = x;
    float const m = value - c;
    auto channel = [&](float v)
    { return static_cast<GLubyte>(std::clamp(std::lround((v + m) * 255.f), 0l, 255l)); };
    return {channel(r), channel(g), channel(b)};
  }

  std::array<float, 3> toHsv(ccColor3B color)
  {
    float const r = color.r / 255.f, g = color.g / 255.f, b = color.b / 255.f;
    float const high = std::max({r, g, b});
    float const low = std::min({r, g, b});
    float const delta = high - low;
    float hue = 0.f;
    if (delta > 0.f)
    {
      if (high == r)
        hue = 60.f * std::fmod((g - b) / delta, 6.f);
      else if (high == g)
        hue = 60.f * ((b - r) / delta + 2.f);
      else
        hue = 60.f * ((r - g) / delta + 4.f);
    }
    if (hue < 0.f)
      hue += 360.f;
    return {hue, high > 0.f ? delta / high : 0.f, high};
  }
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
  bool const picking = m_def->type == settings::Type::Color && s_openPickers.contains(m_def->key);
  this->setContentSize({width, isTitle ? kTitleHeight : kRowHeight + (picking ? kPickerHeight : 0.f)});
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
  float const centerY = this->lineY();

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
  float const centerY = this->lineY();
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
  m_toggle->setPosition({m_width - kPadding - 12.f, this->lineY()});
  m_menu->addChild(m_toggle);
}

void SettingRow::addSlider()
{
  float const centerY = this->lineY();
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
  float const centerY = this->lineY();
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
  button->setPosition({m_width - kPadding - 12.f, this->lineY()});
  m_menu->addChild(button);

  if (s_openPickers.contains(m_def->key))
    this->addColorPicker();
}

float SettingRow::lineY() const
{
  return this->getContentHeight() - kRowHeight / 2.f;
}

void SettingRow::addColorPicker()
{
  // A darker panel under the row, the main line stays as it was
  auto panel = CCLayerColor::create({0, 0, 0, 60}, m_width - 2.f * kPadding, kPickerHeight - 4.f);
  panel->setPosition({kPadding, 2.f});
  this->addChild(panel, -1);

  // The palette and the hex code
  float const paletteY = kPickerHeight - 12.f;
  for (size_t i = 0; i < kPalette.size(); ++i)
  {
    auto sprite = CCSprite::createWithSpriteFrameName("GJ_colorBtn_001.png");
    sprite->setScale(kSwatchSize / std::max(sprite->getContentWidth(), 1.f));
    sprite->setColor(kPalette[i]);
    auto swatch = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(SettingRow::onSwatch));
    swatch->setTag(static_cast<int>(i));
    swatch->setPosition({kPadding + 10.f + static_cast<float>(i) * kSwatchStep, paletteY});
    m_menu->addChild(swatch);
  }

  float const hexWidth = 62.f;
  m_hexInput = TextInput::create(hexWidth / .5f, "#rrggbb");
  m_hexInput->setScale(.5f);
  m_hexInput->setFilter("#0123456789abcdefABCDEF");
  m_hexInput->setMaxCharCount(7);
  m_hexInput->setPosition({m_width - kPadding - 6.f - hexWidth / 2.f, paletteY});
  m_hexInput->setCallback([this](std::string const &text)
                          {
                            auto hex = text;
                            if (!hex.empty() && hex.front() == '#')
                              hex.erase(0, 1);
                            if (hex.size() != 6)
                              return;
                            if (auto color = cc3bFromHexString(hex, true))
                              this->setColor(color.unwrap(), false);
                          });
  this->addChild(m_hexInput);

  // Hue, saturation, brightness
  constexpr std::array<char const *, 3> kNames{"Hue", "Saturation", "Brightness"};
  constexpr std::array<float, 3> kMax{360.f, 1.f, 1.f};
  m_hsvValue = toHsv(settings::color(m_def->key));
  float const sliderLeft = kPadding + 62.f;
  float const sliderWidth = m_width - sliderLeft - kPadding - 10.f;
  for (int i = 0; i < 3; ++i)
  {
    float const y = paletteY - 20.f - static_cast<float>(i) * kPickerLineStep;
    auto name = CCLabelBMFont::create(kNames[i], "bigFont.fnt");
    name->setScale(.26f);
    name->setAnchorPoint({0.f, .5f});
    name->setPosition({kPadding + 6.f, y});
    name->setOpacity(200);
    this->addChild(name);

    auto slider = SliderNode::create([this, i](SliderNode *, float value)
                                     {
                                       m_hsvValue[i] = value;
                                       this->setColor(fromHsv(m_hsvValue[0], m_hsvValue[1], m_hsvValue[2]), true);
                                     });
    slider->setMin(0.f);
    slider->setMax(kMax[i]);
    slider->setValue(m_hsvValue[i]);
    slider->setAnchorPoint({0.f, .5f});
    slider->setScale(.8f);
    slider->setContentSize({sliderWidth / .8f, slider->getContentHeight()});
    slider->setPosition({sliderLeft, y});
    this->addChild(slider);
    m_hsv[i] = slider;
  }
}

void SettingRow::setColor(ccColor3B color, bool fromSliders)
{
  settings::set(m_def->key, "#" + cc3bToHexString(color));
  if (m_colorSprite)
    m_colorSprite->setColor(color);
  if (m_hexInput)
    m_hexInput->setString("#" + utils::string::toLower(cc3bToHexString(color)));
  // The sliders follow a palette color or a typed code, not their own drags
  if (!fromSliders)
  {
    m_hsvValue = toHsv(color);
    for (int i = 0; i < 3; ++i)
    {
      if (m_hsv[i])
        m_hsv[i]->setValue(m_hsvValue[i]);
    }
  }
  this->changed();
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
  // The picker opens under the row (or closes), the preview stays in sight
  if (s_openPickers.contains(m_def->key))
    s_openPickers.erase(m_def->key);
  else
    s_openPickers.insert(m_def->key);
  if (m_onResize)
    m_onResize();
}

void SettingRow::onSwatch(CCObject *sender)
{
  auto const index = static_cast<size_t>(static_cast<CCNode *>(sender)->getTag());
  if (index < kPalette.size())
    this->setColor(kPalette[index], false);
}

void SettingRow::onInfo(CCObject *)
{
  FLAlertLayer::create(
      m_def->name.c_str(),
      m_def->description.c_str(),
      "OK")
      ->show();
}
