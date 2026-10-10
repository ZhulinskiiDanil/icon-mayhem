#include "PartsPopup.hpp"

#include "Buttons.hpp"
#include "Sections.hpp"

#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/Scrollbar.hpp>

using namespace geode::prelude;

namespace
{
  constexpr float kWidth = 380.f;
  constexpr float kHeight = 260.f;
  constexpr CCPoint kListOrigin = {18.f, 18.f};
  constexpr CCSize kListSize = {334.f, 200.f};
  constexpr float kChipHeight = 20.f;
  constexpr float kChipGap = 4.f;
  constexpr float kPadding = 6.f;
}

PartsPopup *PartsPopup::create(std::function<void(std::string const &)> picked)
{
  auto popup = new PartsPopup();
  if (popup->initParts(std::move(picked)))
  {
    popup->autorelease();
    return popup;
  }
  delete popup;
  return nullptr;
}

bool PartsPopup::initParts(std::function<void(std::string const &)> picked)
{
  if (!Popup::init(kWidth, kHeight))
    return false;

  m_picked = std::move(picked);
  this->setID("parts-popup"_spr);
  this->setTitle("All parts");

  auto listBg = NineSlice::create("square02b_001.png");
  listBg->setColor({0, 0, 0});
  listBg->setOpacity(80);
  listBg->setContentSize(kListSize);
  listBg->setAnchorPoint({0.f, 0.f});
  m_mainLayer->addChildAtPosition(listBg, Anchor::BottomLeft, kListOrigin);

  auto list = ScrollLayer::create(kListSize);
  list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(2.f));
  m_mainLayer->addChildAtPosition(list, Anchor::BottomLeft, kListOrigin);
  auto scrollbar = Scrollbar::create(list);
  m_mainLayer->addChildAtPosition(scrollbar, Anchor::BottomLeft,
                                  {kListOrigin.x + kListSize.width + 8.f, kListOrigin.y + kListSize.height / 2.f});

  auto const &sections = customizerSections();
  auto const &groups = customizerGroups();
  for (size_t s = 0; s < sections.size(); ++s)
  {
    // The tab name, then its blocks as chips that wrap
    auto title = CCLabelBMFont::create(sections[s].name, "goldFont.fnt");
    title->setScale(.5f);
    title->setAnchorPoint({0.f, .5f});
    auto titleRow = CCNode::create();
    titleRow->setContentSize({kListSize.width, 18.f});
    title->setPosition({kPadding, 9.f});
    titleRow->addChild(title);
    list->m_contentLayer->addChild(titleRow);

    auto menu = CCMenu::create();
    menu->ignoreAnchorPointForPosition(false);
    menu->setTouchPriority(list->getTouchPriority() - 1);

    std::vector<std::pair<CCMenuItemSpriteExtra *, float>> chips;
    for (auto const &group : groups)
    {
      if (group.section != s)
        continue;
      bool const on = group.master && groupIsOn(group);

      auto measure = CCLabelBMFont::create(group.name.c_str(), "goldFont.fnt");
      float const width = std::min(measure->getContentWidth() * .5f + 16.f, kListSize.width - 2.f * kPadding);
      auto sprite = textButton(group.name.c_str(), width, on ? "GJ_button_01.png" : "GJ_button_04.png", kChipHeight, .5f);
      auto chip = CCMenuItemExt::createSpriteExtra(sprite, [this, id = group.id](auto)
                                                   {
                                                     auto picked = m_picked;
                                                     this->onClose(nullptr);
                                                     if (picked)
                                                       picked(id);
                                                   });
      chips.emplace_back(chip, sprite->getScaledContentWidth());
    }

    // Left to right, a new line when the next chip doesn't fit
    std::vector<std::vector<std::pair<CCMenuItemSpriteExtra *, float>>> lines(1);
    float x = 0.f;
    for (auto const &chip : chips)
    {
      if (x > 0.f && x + chip.second > kListSize.width - 2.f * kPadding)
      {
        lines.emplace_back();
        x = 0.f;
      }
      lines.back().push_back(chip);
      x += chip.second + kChipGap;
    }

    float const height = static_cast<float>(lines.size()) * (kChipHeight + kChipGap);
    menu->setContentSize({kListSize.width, height});
    for (size_t line = 0; line < lines.size(); ++line)
    {
      float left = kPadding;
      float const y = height - (static_cast<float>(line) + .5f) * (kChipHeight + kChipGap);
      for (auto const &[chip, width] : lines[line])
      {
        chip->setPosition({left + width / 2.f, y});
        menu->addChild(chip);
        left += width + kChipGap;
      }
    }
    list->m_contentLayer->addChild(menu);
  }

  list->m_contentLayer->updateLayout();
  list->scrollToTop();

  auto hint = CCLabelBMFont::create("Green: on. Tip: tap a part of the icon in the preview", "bigFont.fnt");
  hint->limitLabelWidth(kListSize.width, .25f, .1f);
  hint->setOpacity(170);
  m_mainLayer->addChildAtPosition(hint, Anchor::Bottom, {0.f, 10.f});
  return true;
}
