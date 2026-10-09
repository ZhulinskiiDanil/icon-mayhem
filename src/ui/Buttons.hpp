#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>

#include <algorithm>

// ! --- Buttons --- !
// A text button `width` wide on the screen. ButtonSprite pads the width it is asked for by its
// own amount (it depends on the caption, the scale and the texture pack), so neighbors overlap:
// measure the first try and ask again for less.

inline ButtonSprite *textButton(char const *text, float width, char const *texture = "GJ_button_01.png", float height = 24.f,
                                float scale = .6f)
{
  auto make = [&](float asked)
  { return ButtonSprite::create(text, static_cast<int>(asked), true, "goldFont.fnt", texture, height, scale); };
  auto measure = [](ButtonSprite *sprite)
  {
    float width = sprite->getContentSize().width;
    if (auto bg = sprite->m_BGSprite)
      width = std::max(width, bg->getContentSize().width * bg->getScaleX());
    return width * sprite->getScaleX();
  };

  auto sprite = make(width);
  float const extra = measure(sprite) - width;
  if (extra > .5f)
    sprite = make(std::max(width - extra, 8.f));

  // Still too wide (a long caption won't squeeze): shrink the whole button
  if (float const actual = measure(sprite); actual > width + .5f)
    sprite->setScale(sprite->getScale() * width / actual);
  return sprite;
}

// A round button with one of the mod's icons (resources/icons, tools/make_icons.py), `size` units wide.
// `caption`: a little word under it, saying what it does. It hangs below the button and doesn't
// count in its size, so the layout keeps its spacing (leave about 8 units free under the button)
inline geode::CircleButtonSprite *iconButton(char const *icon, geode::CircleBaseColor color, float size = 26.f,
                                            char const *caption = nullptr)
{
  auto top = cocos2d::CCSprite::create(geode::Mod::get()->expandSpriteName(fmt::format("icon-{}.png", icon)).data());
  if (!top)
    top = cocos2d::CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
  auto sprite = geode::CircleButtonSprite::create(top, color, geode::CircleBaseSize::Small);
  float const width = sprite->getContentSize().width;
  if (width > 0.f)
    sprite->setScale(size / width);

  if (caption && width > 0.f)
  {
    // In the button's own units: undo its scale so every caption reads the same size
    float const scale = sprite->getScale();
    auto label = cocos2d::CCLabelBMFont::create(caption, "bigFont.fnt");
    label->limitLabelWidth(std::max(size * 1.9f, 34.f) / scale, .22f / scale, .05f);
    label->setAnchorPoint({.5f, 1.f});
    label->setPosition({sprite->getContentSize().width / 2.f, -1.5f / scale});
    label->setOpacity(225);
    sprite->addChild(label);
  }
  return sprite;
}
