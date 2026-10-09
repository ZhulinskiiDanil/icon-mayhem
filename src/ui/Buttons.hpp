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

// A round button with one of the mod's icons (resources/icons, tools/make_icons.py), `size` units wide
inline geode::CircleButtonSprite *iconButton(char const *icon, geode::CircleBaseColor color, float size = 26.f)
{
  auto top = cocos2d::CCSprite::create(geode::Mod::get()->expandSpriteName(fmt::format("icon-{}.png", icon)).data());
  if (!top)
    top = cocos2d::CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
  auto sprite = geode::CircleButtonSprite::create(top, color, geode::CircleBaseSize::Small);
  if (float const width = sprite->getContentSize().width; width > 0.f)
    sprite->setScale(size / width);
  return sprite;
}
