#include "LooksPopup.hpp"
#include "../presets/Looks.hpp"
#include "../presets/Presets.hpp"

#include <algorithm>
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
  constexpr char const *kPlayerOneHint = "Your icon wears the look of its game mode.\n"
                                         "Modes left at \"Main look\" wear the loaded preset";
  constexpr char const *kPlayerTwoHint = "The second icon in dual: its own look of the mode first,\n"
                                         "then \"All modes\", then player 1's look of the mode";
}

// ! --- Creation --- !

LooksPopup *LooksPopup::create()
{
  auto popup = new LooksPopup();
  if (popup->initLooks())
  {
    popup->autorelease();
    return popup;
  }
  delete popup;
  return nullptr;
}

bool LooksPopup::initLooks()
{
  if (!Popup::init(kWidth, kHeight))
    return false;

  this->setID("looks-popup"_spr);
  this->setTitle("Looks per mode");

  m_hint = CCLabelBMFont::create(kPlayerOneHint, "chatFont.fnt", 460.f, kCCTextAlignmentCenter);
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

  // The main look, the shipped presets, then the player's own
  m_options.push_back("");
  for (auto const &preset : presets::builtIn())
    m_options.push_back(preset.name);
  for (auto const &preset : presets::list())
  {
    if (std::find(m_options.begin(), m_options.end(), preset.name) == m_options.end())
      m_options.push_back(preset.name);
  }

  this->buildPlayerTabs();
  this->buildRows();
  return true;
}

void LooksPopup::buildPlayerTabs()
{
  m_playerMenu = CCMenu::create();
  m_playerMenu->ignoreAnchorPointForPosition(false);
  m_playerMenu->setContentSize({200.f, 30.f});
  m_playerMenu->setLayout(RowLayout::create()->setGap(8.f));
  m_playerMenu->setTouchPriority(m_list->getTouchPriority() - 1);

  for (bool two : {false, true})
  {
    auto sprite = ButtonSprite::create(two ? "Player 2" : "Player 1", 70, true, "goldFont.fnt",
                                       two == m_playerTwo ? "GJ_button_01.png" : "GJ_button_04.png", 24.f, .6f);
    m_playerTabs[two ? 1 : 0] = sprite;
    auto tab = CCMenuItemExt::createSpriteExtra(sprite, [this, two](auto)
                                                {
                                                  if (two == m_playerTwo)
                                                    return;
                                                  m_playerTwo = two;
                                                  m_playerTabs[0]->updateBGImage(two ? "GJ_button_04.png" : "GJ_button_01.png");
                                                  m_playerTabs[1]->updateBGImage(two ? "GJ_button_01.png" : "GJ_button_04.png");
                                                  m_hint->setString(two ? kPlayerTwoHint : kPlayerOneHint);
                                                  this->buildRows();
                                                });
    m_playerMenu->addChild(tab);
  }
  m_playerMenu->updateLayout();
  m_mainLayer->addChildAtPosition(m_playerMenu, Anchor::Top, {0.f, -44.f});
}

void LooksPopup::buildRows()
{
  m_list->m_contentLayer->removeAllChildren();

  // Player 2 falls back to its look for all modes, then to player 1's looks
  bool const two = m_playerTwo;
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

CCNode *LooksPopup::createRow(char const *label, char const *unset, std::function<std::string()> current,
                              std::function<void(std::string const &)> assign)
{
  auto row = CCNode::create();
  row->setContentSize({kListSize.width, kRowHeight});
  float const centerY = kRowHeight / 2.f;

  auto text = CCLabelBMFont::create(label, "bigFont.fnt");
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

  for (int dir : {-1, 1})
  {
    auto sprite = CCSprite::createWithSpriteFrameName("navArrowBtn_001.png");
    sprite->setScale(.3f);
    sprite->setFlipX(dir < 0);
    auto arrow = CCMenuItemExt::createSpriteExtra(sprite, [this, dir, current, assign, show](auto)
                                                  {
                                                    auto const name = current();
                                                    auto it = std::find(m_options.begin(), m_options.end(), name);
                                                    int index = it == m_options.end() ? 0 : static_cast<int>(it - m_options.begin());
                                                    int const count = static_cast<int>(m_options.size());
                                                    index = (index + dir + count) % count;
                                                    assign(m_options[static_cast<size_t>(index)]);
                                                    show();
                                                  });
    arrow->setPosition({dir < 0 ? kListSize.width - 10.f - kChoiceWidth + 6.f : kListSize.width - 16.f, centerY});
    menu->addChild(arrow);
  }

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

  auto sprite = ButtonSprite::create("Edit", 40, true, "goldFont.fnt", "GJ_button_01.png", 22.f, .6f);
  auto button = CCMenuItemExt::createSpriteExtra(sprite, [](auto)
                                                 {
                                                   if (auto popup = LooksPopup::create())
                                                     popup->show();
                                                 });
  button->setPosition({width - 34.f, 15.f});
  menu->addChild(button);
  return row;
}
