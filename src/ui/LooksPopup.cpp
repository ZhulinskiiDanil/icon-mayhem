#include "LooksPopup.hpp"
#include "Buttons.hpp"
#include "LinkerPopup.hpp"
#include "PresetPicker.hpp"
#include <Geode/ui/GeodeUI.hpp>
#include "../presets/Looks.hpp"
#include "../presets/Presets.hpp"

#include <algorithm>
#include <array>
#include <memory>

using namespace geode::prelude;

// ! --- Constants --- !

namespace
{
  constexpr float kWidth = 320.f;
  constexpr float kHeight = 270.f;
  constexpr CCPoint kListOrigin = {20.f, 20.f};
  constexpr CCSize kListSize = {270.f, 158.f};
  constexpr float kRowHeight = 26.f;
  constexpr float kChoiceWidth = 130.f;
  constexpr float kResetSpace = 24.f; // left of the row name, for the reset button

  constexpr char const *kMainLook = "Main look";
  constexpr char const *kPlayerOneHint = "Your icon wears the look of its game mode (an icon linked in the Linker wins).\n"
                                         "Modes left at \"Main look\" wear the loaded preset";
  constexpr char const *kPlayerTwoHint = "The second icon in dual: its own look of the mode first,\n"
                                         "then \"All modes\", then player 1's look of the mode";
  constexpr std::array<char const *, 2> kTabNames = {"Player 1", "Player 2"};
  constexpr std::array<char const *, 2> kHints = {kPlayerOneHint, kPlayerTwoHint};
}

// ! --- Creation --- !

LooksPopup *LooksPopup::create(Tab tab)
{
  auto popup = new LooksPopup();
  if (popup->initLooks(tab))
  {
    popup->autorelease();
    return popup;
  }
  delete popup;
  return nullptr;
}

bool LooksPopup::initLooks(Tab tab)
{
  if (!Popup::init(kWidth, kHeight))
    return false;

  m_tab = tab;
  this->setID("looks-popup"_spr);
  this->setTitle("Looks");

  m_hint = CCLabelBMFont::create(kHints[static_cast<size_t>(tab)], "chatFont.fnt", 460.f, kCCTextAlignmentCenter);
  m_hint->setScale(.5f);
  m_mainLayer->addChildAtPosition(m_hint, Anchor::Top, {0.f, -72.f});

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
  m_mainLayer->addChildAtPosition(scrollbar, Anchor::BottomLeft,
                                  {kListOrigin.x + kListSize.width + 8.f, kListOrigin.y + kListSize.height / 2.f});

  this->buildTabs();
  this->buildRows();
  return true;
}

void LooksPopup::buildTabs()
{
  m_tabMenu = CCMenu::create();
  m_tabMenu->ignoreAnchorPointForPosition(false);
  m_tabMenu->setContentSize({260.f, 30.f});
  m_tabMenu->setLayout(RowLayout::create()->setGap(8.f));
  m_tabMenu->setTouchPriority(m_list->getTouchPriority() - 1);

  for (size_t i = 0; i < kTabNames.size(); ++i)
  {
    auto const tab = static_cast<Tab>(i);
    auto sprite = textButton(kTabNames[i], 70.f, tab == m_tab ? "GJ_button_01.png" : "GJ_button_04.png");
    m_tabs[i] = sprite;
    // Recolors the buttons instead of rebuilding the menu that is handling this touch
    auto button = CCMenuItemExt::createSpriteExtra(sprite, [this, tab](auto)
                                                   {
                                                     if (tab == m_tab)
                                                       return;
                                                     m_tab = tab;
                                                     for (size_t j = 0; j < m_tabs.size(); ++j)
                                                       m_tabs[j]->updateBGImage(static_cast<Tab>(j) == tab ? "GJ_button_01.png" : "GJ_button_04.png");
                                                     m_hint->setString(kHints[static_cast<size_t>(tab)]);
                                                     this->buildRows();
                                                   });
    m_tabMenu->addChild(button);
  }

  // Presets of single icons live in the Linker
  auto linker = CCMenuItemExt::createSpriteExtra(textButton("Linker", 70.f, "GJ_button_02.png"), [](auto)
                                                 {
                                                   if (auto popup = LinkerPopup::create())
                                                     popup->show();
                                                 });
  linker->setID("linker-button");
  m_tabMenu->addChild(linker);
  m_tabMenu->updateLayout();
  m_mainLayer->addChildAtPosition(m_tabMenu, Anchor::Top, {0.f, -44.f});
}

void LooksPopup::addSectionLabel(char const *text)
{
  auto row = CCNode::create();
  row->setContentSize({kListSize.width, 18.f});
  auto label = CCLabelBMFont::create(text, "goldFont.fnt");
  label->setScale(.45f);
  label->setAnchorPoint({0.f, .5f});
  label->setPosition({8.f, 9.f});
  row->addChild(label);
  m_list->m_contentLayer->addChild(row);
}

void LooksPopup::buildRows()
{
  m_list->m_contentLayer->removeAllChildren();

  // Player 2 falls back to its look for all modes, then to the icon and player 1's looks
  bool const two = m_tab == Tab::PlayerTwo;
  if (two)
  {
    m_list->m_contentLayer->addChild(this->createRow("All modes", "Same as player 1", []
                                                     { return looks::forPlayerTwo(); }, [](std::string const &name)
                                                     { looks::setForPlayerTwo(name); }));
  }
  for (size_t i = 0; i < static_cast<size_t>(GameMode::Count); ++i)
  {
    auto const mode = static_cast<GameMode>(i);
    m_list->m_contentLayer->addChild(this->createRow(looks::modeName(mode), two ? "Same as all modes" : kMainLook, [mode, two]
                                                     { return looks::forMode(mode, two); }, [mode, two](std::string const &name)
                                                     { looks::setForMode(mode, two, name); }));
  }

  m_list->m_contentLayer->updateLayout();
  m_list->scrollToTop();
}

CCNode *LooksPopup::createRow(std::string const &label, char const *unset, std::function<std::string()> current,
                              std::function<void(std::string const &)> assign)
{
  auto row = CCNode::create();
  row->setContentSize({kListSize.width, kRowHeight});
  float const centerY = kRowHeight / 2.f;

  auto text = CCLabelBMFont::create(label.c_str(), "bigFont.fnt");
  text->setScale(.4f);
  text->setAnchorPoint({0.f, .5f});
  text->setPosition({kResetSpace, centerY});
  row->addChild(text);

  auto menu = CCMenu::create();
  menu->setPosition({0.f, 0.f});
  menu->setContentSize(row->getContentSize());
  row->addChild(menu);

  // Back to nothing chosen, left of the name; hidden while nothing is chosen
  auto resetButton = std::make_shared<CCMenuItemSpriteExtra *>(nullptr);
  auto value = CCLabelBMFont::create("", "bigFont.fnt");
  value->setPosition({kListSize.width - 10.f - kChoiceWidth / 2.f, centerY});
  row->addChild(value);
  auto show = [value, resetButton, current, unset]
  {
    auto const name = current();
    value->setString(name.empty() ? unset : name.c_str());
    value->limitLabelWidth(kChoiceWidth - 30.f, .35f, .1f);
    // Nothing chosen is dimmed, a chosen preset stands out
    value->setColor(name.empty() ? ccColor3B{170, 170, 170} : ccColor3B{255, 255, 255});
    if (*resetButton)
      (*resetButton)->setVisible(!name.empty());
  };

  auto resetSprite = CCSprite::createWithSpriteFrameName("geode.loader/reset-gold.png");
  if (!resetSprite)
    resetSprite = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");
  resetSprite->setScale(.5f);
  auto reset = CCMenuItemExt::createSpriteExtra(resetSprite, [assign, show](auto)
                                                {
                                                  assign("");
                                                  show();
                                                });
  reset->setPosition({kResetSpace / 2.f + 2.f, centerY});
  menu->addChild(reset);
  *resetButton = reset;
  show();

  // The preset in a box: one tap opens the list with search
  auto box = NineSlice::create("square02b_001.png");
  box->setColor({0, 0, 0});
  box->setOpacity(90);
  box->setContentSize({kChoiceWidth, kRowHeight - 6.f});
  auto choose = CCMenuItemExt::createSpriteExtra(box, [label, unset, current, assign, show](auto)
                                                 {
                                                   auto picker = PresetPicker::create(label, current(), unset, [assign, show](std::string const &name)
                                                                                      {
                                                                                        assign(name);
                                                                                        show();
                                                                                      });
                                                   if (picker)
                                                     picker->show();
                                                 });
  choose->m_scaleMultiplier = 1.03f;
  choose->setPosition({kListSize.width - 10.f - kChoiceWidth / 2.f, centerY});
  menu->addChild(choose);
  // The name on top of the box
  value->removeFromParent();
  value->setZOrder(1);
  row->addChild(value);

  return row;
}

// ! --- General tab row --- !

CCNode *createLooksRow(float width)
{
  auto row = CCNode::create();
  row->setContentSize({width, 30.f});

  auto label = CCLabelBMFont::create("Looks per mode", "bigFont.fnt");
  label->setScale(.4f);
  label->setAnchorPoint({0.f, .5f});
  label->setPosition({8.f, 15.f});
  row->addChild(label);

  auto menu = CCMenu::create();
  menu->setPosition({0.f, 0.f});
  menu->setContentSize(row->getContentSize());
  row->addChild(menu);

  auto sprite = textButton("Edit", 40.f, "GJ_button_01.png", 22.f);
  auto button = CCMenuItemExt::createSpriteExtra(sprite, [](auto)
                                                 {
                                                   if (auto popup = LooksPopup::create())
                                                     popup->show();
                                                 });
  button->setPosition({width - 34.f, 15.f});
  menu->addChild(button);
  return row;
}

// ! --- Emote keys row --- !

CCNode *createEmoteKeysRow(float width)
{
  auto row = CCNode::create();
  row->setContentSize({width, 30.f});

  // Keys are a setting type the customizer rows don't draw: Geode's own settings do
  auto label = CCLabelBMFont::create("Emote keys", "bigFont.fnt");
  label->setScale(.4f);
  label->setAnchorPoint({0.f, .5f});
  label->setPosition({8.f, 15.f});
  row->addChild(label);

  auto menu = CCMenu::create();
  menu->setPosition({0.f, 0.f});
  menu->setContentSize(row->getContentSize());
  row->addChild(menu);

  auto sprite = textButton("Keys", 40.f, "GJ_button_01.png", 22.f);
  auto button = CCMenuItemExt::createSpriteExtra(sprite, [](auto)
                                                 { openSettingsPopup(Mod::get(), false); });
  button->setPosition({width - 34.f, 15.f});
  menu->addChild(button);
  return row;
}
