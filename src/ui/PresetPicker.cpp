#include "PresetPicker.hpp"

#include "../presets/Presets.hpp"

#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/Scrollbar.hpp>

#include <algorithm>
#include <unordered_map>

using namespace geode::prelude;

namespace
{
  constexpr float kWidth = 300.f;
  constexpr float kHeight = 260.f;
  constexpr CCPoint kListOrigin = {20.f, 18.f};
  constexpr CCSize kListSize = {250.f, 176.f};
  constexpr float kSearchY = 212.f;
  constexpr float kRowHeight = 24.f;

  bool matches(std::string const &name, std::string const &query)
  {
    return query.empty() || utils::string::toLower(name).find(utils::string::toLower(query)) != std::string::npos;
  }
}

PresetPicker *PresetPicker::create(std::string const &title, std::string const &current, std::string const &none,
                                   std::function<void(std::string const &)> picked)
{
  auto popup = new PresetPicker();
  if (popup->initPicker(title, current, none, std::move(picked)))
  {
    popup->autorelease();
    return popup;
  }
  delete popup;
  return nullptr;
}

bool PresetPicker::initPicker(std::string const &title, std::string const &current, std::string const &none,
                              std::function<void(std::string const &)> picked)
{
  if (!Popup::init(kWidth, kHeight))
    return false;

  m_picked = std::move(picked);
  m_current = current;
  m_none = none;
  this->setID("preset-picker"_spr);
  this->setTitle(title.c_str());

  // Yours, the latest loaded or saved first; then the built-in ones
  auto mine = presets::list();
  std::unordered_map<std::string, size_t> order;
  auto const recent = presets::recent();
  for (size_t i = 0; i < recent.size(); ++i)
    order.emplace(recent[i], i);
  std::stable_sort(mine.begin(), mine.end(), [&](Preset const &a, Preset const &b)
                   {
                     auto const ia = order.find(a.name);
                     auto const ib = order.find(b.name);
                     return (ia == order.end() ? recent.size() : ia->second) < (ib == order.end() ? recent.size() : ib->second);
                   });
  for (auto const &preset : mine)
    m_mine.push_back(preset.name);
  for (auto const &preset : presets::builtIn())
  {
    // A saved preset of the same name wins when a look is worn
    if (std::find(m_mine.begin(), m_mine.end(), preset.name) == m_mine.end())
      m_builtIn.push_back(preset.name);
  }

  auto listBg = NineSlice::create("square02b_001.png");
  listBg->setColor({0, 0, 0});
  listBg->setOpacity(80);
  listBg->setContentSize(kListSize);
  listBg->setAnchorPoint({0.f, 0.f});
  m_mainLayer->addChildAtPosition(listBg, Anchor::BottomLeft, kListOrigin);

  m_list = ScrollLayer::create(kListSize);
  m_list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(0.f));
  m_list->setTouchEnabled(true);
  m_mainLayer->addChildAtPosition(m_list, Anchor::BottomLeft, kListOrigin);

  auto scrollbar = Scrollbar::create(m_list);
  scrollbar->setScale(.8f);
  m_mainLayer->addChildAtPosition(scrollbar, Anchor::BottomLeft,
                                  {kListOrigin.x + kListSize.width + 7.f, kListOrigin.y + kListSize.height / 2.f});
  m_buttonMenu->setTouchPriority(m_list->getTouchPriority() - 1);

  auto search = TextInput::create(kListSize.width / .7f, "Search presets");
  search->setScale(.7f);
  search->setCallback([this](std::string const &text)
                      {
                        m_query = text;
                        this->refreshList();
                      });
  m_mainLayer->addChildAtPosition(search, Anchor::BottomLeft, {kListOrigin.x + kListSize.width / 2.f, kSearchY});

  this->refreshList();
  search->focus();
  return true;
}

CCNode *PresetPicker::createLabelRow(char const *text)
{
  auto row = CCNode::create();
  row->setContentSize({kListSize.width, 18.f});
  auto label = CCLabelBMFont::create(text, "goldFont.fnt");
  label->setScale(.4f);
  label->setAnchorPoint({0.f, .5f});
  label->setPosition({8.f, 9.f});
  row->addChild(label);
  return row;
}

CCNode *PresetPicker::createRow(std::string const &name, std::string const &shown, bool builtIn)
{
  auto row = CCNode::create();
  row->setContentSize({kListSize.width, kRowHeight});
  row->setAnchorPoint({.5f, .5f});
  bool const chosen = name == m_current;

  auto highlight = CCLayerColor::create(chosen ? ccColor4B{120, 255, 140, 45} : ccColor4B{0, 0, 0, 0}, kListSize.width, kRowHeight);
  row->addChild(highlight);

  // The whole row picks
  auto menu = CCMenu::create();
  menu->setPosition({0.f, 0.f});
  menu->setContentSize(row->getContentSize());
  row->addChild(menu);
  auto hitArea = CCNode::create();
  hitArea->setContentSize(row->getContentSize());
  auto button = CCMenuItemExt::createSpriteExtra(hitArea, [this, name](auto)
                                                 { this->pick(name); });
  button->m_scaleMultiplier = 1.f;
  button->setPosition({kListSize.width / 2.f, kRowHeight / 2.f});
  menu->addChild(button);

  auto label = CCLabelBMFont::create(shown.c_str(), "bigFont.fnt");
  label->setAnchorPoint({0.f, .5f});
  label->limitLabelWidth(kListSize.width - 70.f, .4f, .1f);
  label->setPosition({12.f, kRowHeight / 2.f});
  if (name.empty())
    label->setColor({190, 190, 190});
  row->addChild(label);

  char const *mark = chosen ? "chosen" : builtIn ? "built-in" : nullptr;
  if (mark)
  {
    auto tag = CCLabelBMFont::create(mark, "bigFont.fnt");
    tag->setScale(.25f);
    tag->setAnchorPoint({1.f, .5f});
    tag->setColor(chosen ? ccColor3B{130, 255, 140} : ccColor3B{170, 170, 170});
    tag->setPosition({kListSize.width - 8.f, kRowHeight / 2.f});
    row->addChild(tag);
  }
  return row;
}

void PresetPicker::refreshList()
{
  auto content = m_list->m_contentLayer;
  content->removeAllChildren();

  if (!m_none.empty() && m_query.empty())
    content->addChild(this->createRow("", m_none, false));

  bool labelled = false;
  for (auto const &name : m_mine)
  {
    if (!matches(name, m_query))
      continue;
    if (!labelled)
      content->addChild(this->createLabelRow("Your presets"));
    labelled = true;
    content->addChild(this->createRow(name, name, false));
  }
  labelled = false;
  for (auto const &name : m_builtIn)
  {
    if (!matches(name, m_query))
      continue;
    if (!labelled)
      content->addChild(this->createLabelRow("Built-in"));
    labelled = true;
    content->addChild(this->createRow(name, name, true));
  }

  if (content->getChildrenCount() == 0)
  {
    auto row = CCNode::create();
    row->setContentSize({kListSize.width, 40.f});
    auto label = CCLabelBMFont::create("Nothing found", "bigFont.fnt");
    label->setScale(.35f);
    label->setOpacity(150);
    label->setPosition({kListSize.width / 2.f, 20.f});
    row->addChild(label);
    content->addChild(row);
  }

  content->updateLayout();
  m_list->scrollToTop();
}

void PresetPicker::pick(std::string const &name)
{
  // Closed first: the callback may open or rebuild other popups
  auto picked = std::move(m_picked);
  this->onClose(nullptr);
  if (picked)
    picked(name);
}
