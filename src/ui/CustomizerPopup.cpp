#include "CustomizerPopup.hpp"
#include "GalleryPopup.hpp"
#include "LinkerPopup.hpp"
#include "LooksPopup.hpp"

// More Icons is optional: its functions are reached through Geode events, without linking to it
#define MORE_ICONS_EVENTS
#include <hiimjustin000.more_icons/include/MoreIcons.hpp>

#include "../hair/HairConfig.hpp"
#include "../hooks/LevelHair.hpp"
#include "../hooks/SimplePlayerHair.hpp"
#include "../presets/Looks.hpp"
#include "Buttons.hpp"
#include "Confirm.hpp"
#include "PresetsPopup.hpp"
#include "SaveLook.hpp"
#include "Sections.hpp"

#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Scrollbar.hpp>

#include <array>
#include <cmath>
#include <numbers>

using namespace geode::prelude;

// ! --- Constants --- !

namespace
{
  constexpr float kWidth = 440.f;
  constexpr float kHeight = 270.f;

  // Preview panel on the left
  constexpr CCPoint kPreviewCenter = {90.f, 113.f};
  constexpr CCSize kPreviewSize = {150.f, 190.f};
  constexpr CCPoint kStagePosition = {75.f, 112.f}; // in the preview panel
  constexpr float kPreviewScale = 1.3f;

  constexpr float kRunSpeed = 330.f;    // stage units / s, enough to comb the hair fully
  constexpr float kJumpDuration = .42f; // s, like a cube jump in the game
  constexpr float kJumpHeight = 45.f;   // stage units
  constexpr float kHeadRadius = 15.f;   // icon units, for the rolling ball

  // Settings list on the right
  constexpr CCPoint kListOrigin = {180.f, 38.f};
  constexpr CCSize kListSize = {240.f, 146.f};
  constexpr float kSearchY = 201.f;
  constexpr float kOnNowWidth = 58.f; // the "On now" button right of the search
  constexpr float kTabsY = 228.f;

  // Undo
  constexpr size_t kUndoSteps = 30;
  constexpr float kUndoQuiet = .6f; // s without changes that end a burst

  // Hitbox helpers, remembered between openings
  constexpr char const *kHitboxesSave = "customizer-hitboxes";
  constexpr char const *kWindSave = "customizer-wind";

  // Wind view
  constexpr size_t kMaxStreaks = 60;
  constexpr float kStreakRate = 30.f;     // streaks / s at full flow
  constexpr float kMoteRate = 9.f;        // motes of dust / s at full flow
  constexpr float kStreakSlow = 60.f;     // panel units / s at no flow
  constexpr float kStreakFast = 330.f;    // panel units / s more at full flow
  constexpr float kFlutterWave = 7.f;     // panel units the streaks wave at flutter 1
  constexpr float kCurlGust = 1.12f;      // gusts above this curl some near streaks
  constexpr float kHeadAir = 22.f;        // panel units: the air flows around the head this far out
  constexpr float kMeterWidth = 24.f;
  constexpr float kToolX = 16.f;          // the tool column on the left of the preview
  constexpr float kToolTop = 136.f;
  constexpr float kToolStep = 28.f;
  // Open blocks, remembered between openings
  constexpr char const *kOpenSave = "customizer-open-groups";
  constexpr char const *kNeverSaved = "@never";

  constexpr float kHeaderHeight = 28.f;
  constexpr float kHeaderTextLeft = 20.f;

  struct LegendEntry
  {
    char const *name;
    ccColor3B color;
  };

  // Same colors as HairNode::drawDebug()
  constexpr std::array kLegend{
      LegendEntry{"Hitbox", {0, 255, 255}},
      LegendEntry{"Floor", {255, 140, 0}},
      LegendEntry{"Roots", {255, 255, 0}},
      LegendEntry{"Face locks", {77, 255, 77}},
      LegendEntry{"Bangs", {255, 77, 255}},
  };

  struct PreviewMode
  {
    IconType type;
    char const *name;
  };

  constexpr std::array kModes{
      PreviewMode{IconType::Cube, "Cube"},
      PreviewMode{IconType::Ball, "Ball"},
      PreviewMode{IconType::Wave, "Wave"},
      PreviewMode{IconType::Robot, "Robot"},
      PreviewMode{IconType::Spider, "Spider"},
      PreviewMode{IconType::Swing, "Swing"},
  };

}

// ! --- Creation --- !

CustomizerPopup *CustomizerPopup::create()
{
  auto popup = new CustomizerPopup();
  if (popup->initCustomizer())
  {
    popup->autorelease();
    return popup;
  }
  delete popup;
  return nullptr;
}

bool CustomizerPopup::initCustomizer()
{
  if (!Popup::init(kWidth, kHeight))
    return false;

  this->setID("customizer-popup"_spr);
  this->setTitle("Icon Mayhem");

  // List first: every menu of the popup has to sit above it to receive touches
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

  int const buttonPriority = m_list->getTouchPriority() - 1;
  m_buttonMenu->setTouchPriority(buttonPriority);

  this->buildPreview();
  this->buildTabs();

  // Search over all tabs, between the tabs and the list
  m_search = TextInput::create((kListSize.width - kOnNowWidth - 6.f) / .7f, "Search settings");
  m_search->setScale(.7f);
  m_search->setID("search");
  m_search->setCallback([this](std::string const &text)
                        {
                          m_query = text;
                          this->rebuildList(false);
                          this->buildTabs();
                        });
  m_mainLayer->addChildAtPosition(m_search, Anchor::BottomLeft,
                                  {kListOrigin.x + (kListSize.width - kOnNowWidth - 6.f) / 2.f, kSearchY});

  m_onNowSprite = textButton("On now", kOnNowWidth, "GJ_button_04.png", 22.f);
  auto onNow = CCMenuItemSpriteExtra::create(m_onNowSprite, this, menu_selector(CustomizerPopup::onOnNow));
  onNow->setID("on-now-button");
  m_buttonMenu->addChildAtPosition(onNow, Anchor::BottomLeft, {kListOrigin.x + kListSize.width - kOnNowWidth / 2.f, kSearchY});

  // Open blocks: the first of every tab the first time
  auto const open = Mod::get()->getSavedValue<std::vector<std::string>>(kOpenSave, {kNeverSaved});
  if (open.size() == 1 && open.front() == kNeverSaved)
  {
    size_t last = static_cast<size_t>(-1);
    for (auto const &group : customizerGroups())
    {
      if (group.section != last)
        m_open.insert(group.id);
      last = group.section;
    }
  }
  else
    m_open.insert(open.begin(), open.end());

  // Under the list: the presets and saving on the left, the tools on the right
  auto presetsButton = CCMenuItemSpriteExtra::create(textButton("Presets", 70), this,
                                                     menu_selector(CustomizerPopup::onPresets));
  presetsButton->setID("presets-button");
  m_buttonMenu->addChildAtPosition(presetsButton, Anchor::BottomLeft, {kListOrigin.x + 37.f, 23.f});

  // One tap saves the look into the preset it came from
  auto save = CCMenuItemSpriteExtra::create(iconButton("save", CircleBaseColor::Green, 24.f, "Save"), this,
                                            menu_selector(CustomizerPopup::onSave));
  save->setID("save-button");
  m_buttonMenu->addChildAtPosition(save, Anchor::BottomLeft, {kListOrigin.x + 92.f, 25.f});

  // Hitbox helpers: gray off, cyan on
  m_showHitboxes = Mod::get()->getSavedValue<bool>(kHitboxesSave, false);
  auto hitboxes = CCMenuItemToggler::create(iconButton("hitboxes", CircleBaseColor::Gray, 24.f, "Hitboxes"),
                                            iconButton("hitboxes", CircleBaseColor::Cyan, 24.f, "Hitboxes"), this,
                                            menu_selector(CustomizerPopup::onHitboxes));
  hitboxes->toggle(m_showHitboxes);
  hitboxes->setID("hitboxes-toggle");
  m_buttonMenu->addChildAtPosition(hitboxes, Anchor::BottomLeft, {kListOrigin.x + kListSize.width - 50.f, 25.f});

  auto reset = CCMenuItemSpriteExtra::create(iconButton("reset", CircleBaseColor::Gray, 24.f, "Reset"), this,
                                             menu_selector(CustomizerPopup::onReset));
  reset->setID("reset-button");
  m_buttonMenu->addChildAtPosition(reset, Anchor::BottomLeft, {kListOrigin.x + kListSize.width - 14.f, 25.f});

  // Top right: the preset the look came from, and whether it is saved
  presets::adoptMatchingPreset();
  m_lookLabel = CCLabelBMFont::create("", "bigFont.fnt");
  m_lookLabel->setAnchorPoint({1.f, .5f});
  m_lookLabel->setID("look-label");
  m_mainLayer->addChildAtPosition(m_lookLabel, Anchor::TopRight, {-16.f, -17.f});
  this->refreshLookLabel();
  this->applyHitboxes();

  this->showSection(0);
  this->scheduleUpdate();

  // Undo starts from the look the popup opened with
  m_stable = presets::capture("Undo");
  m_seenVersion = HairConfig::version();

  // The icon in the preview has a preset linked: that's the look to edit, Main switches back
  this->startIconEdit(false);
  return true;
}

// ! --- Preview --- !

void CustomizerPopup::buildPreview()
{
  auto panel = NineSlice::create("square02b_001.png");
  panel->setColor({0, 0, 0});
  panel->setOpacity(80);
  panel->setContentSize(kPreviewSize);
  panel->setAnchorPoint({.5f, .5f});
  panel->setID("preview");
  m_mainLayer->addChildAtPosition(panel, Anchor::BottomLeft, kPreviewCenter);

  // Ground under the icon, the hair spreads over it
  float const groundY = kStagePosition.y - kHeadRadius * kPreviewScale;
  auto ground = CCLayerColor::create({255, 255, 255, 60}, kPreviewSize.width - 16.f, 1.f);
  ground->setPosition({8.f, groundY});
  panel->addChild(ground);

  m_wind = CCDrawNode::create();
  m_wind->setID("wind-view");
  panel->addChild(m_wind);
  m_windFront = CCDrawNode::create();
  m_windFront->setID("wind-view-front");
  m_windHint = CCLabelBMFont::create("Run, or set a breeze, to see the wind", "bigFont.fnt");
  m_windHint->limitLabelWidth(kPreviewSize.width - 20.f, .22f, .1f);
  m_windHint->setOpacity(160);
  m_windHint->setPosition({kPreviewSize.width / 2.f, groundY - 8.f});
  panel->addChild(m_windHint);

  m_stage = CCNode::create();
  m_stage->setPosition(kStagePosition);
  m_stage->setID("stage");
  panel->addChild(m_stage);
  // The near air passes in front of the icon
  panel->addChild(m_windFront);

  // What the hitbox helper colors mean
  m_legend = CCNode::create();
  m_legend->setID("hitboxes-legend");
  for (size_t i = 0; i < kLegend.size(); ++i)
  {
    auto label = CCLabelBMFont::create(kLegend[i].name, "bigFont.fnt");
    label->setScale(.25f);
    label->setColor(kLegend[i].color);
    label->setAnchorPoint({0.f, .5f});
    // Three on the first line, two on the second, under the ground line
    float const x = i < 3 ? 6.f + static_cast<float>(i) * 46.f : 6.f + static_cast<float>(i - 3) * 60.f;
    label->setPosition({x, i < 3 ? 82.f : 73.f});
    m_legend->addChild(label);
  }
  panel->addChild(m_legend);

  m_player = SimplePlayer::create(0);
  m_player->setScale(kPreviewScale);
  m_stage->addChild(m_player);

  attachSimplePlayerHair(m_player, PreviewPlace::Customizer);
  if (auto hair = getSimplePlayerHair(m_player).icon)
  {
    hair->setOnGround([this]
                      { return m_jumpTime < 0.f; });
  }

  // Mode switcher and actions
  auto menu = CCMenu::create();
  menu->setPosition({0.f, 0.f});
  menu->setContentSize(kPreviewSize);
  menu->setTouchPriority(m_list->getTouchPriority() - 1);
  panel->addChild(menu);

  for (int dir : {-1, 1})
  {
    auto sprite = CCSprite::createWithSpriteFrameName("navArrowBtn_001.png");
    sprite->setScale(.4f);
    sprite->setFlipX(dir < 0);
    auto arrow = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(CustomizerPopup::onMode));
    arrow->setTag(dir);
    arrow->setPosition({kPreviewSize.width / 2.f + dir * 55.f, 66.f});
    menu->addChild(arrow);
  }

  m_modeLabel = CCLabelBMFont::create("", "bigFont.fnt");
  m_modeLabel->setScale(.45f);
  m_modeLabel->setPosition({kPreviewSize.width / 2.f, 66.f});
  panel->addChild(m_modeLabel);

  // Quick actions in a row on the top of the preview: undo, a random look (the dice), random
  // colors (the palette), every part in the hair color (the drops)
  auto quick = [&](char const *icon, CircleBaseColor color, char const *caption, char const *id, SEL_MenuHandler handler, float x)
  {
    auto button = CCMenuItemSpriteExtra::create(iconButton(icon, color, 22.f, caption), this, handler);
    button->setID(id);
    button->setPosition({x, kPreviewSize.height - 15.f});
    menu->addChild(button);
  };
  float const step = 36.f;
  float const first = kPreviewSize.width / 2.f - step * 1.5f;
  quick("undo", CircleBaseColor::Gray, "Undo", "undo-button", menu_selector(CustomizerPopup::onUndo), first);
  quick("dice", CircleBaseColor::Pink, "Random", "surprise-button", menu_selector(CustomizerPopup::onSurprise), first + step);
  quick("palette", CircleBaseColor::Blue, "Colors", "colors-button", menu_selector(CustomizerPopup::onColors), first + step * 2.f);
  quick("match", CircleBaseColor::Cyan, "Same color", "match-button", menu_selector(CustomizerPopup::onMatch), first + step * 3.f);

  // Jump and run under the icon
  auto jump = CCMenuItemSpriteExtra::create(iconButton("jump", CircleBaseColor::Green, 22.f, "Jump"), this, menu_selector(CustomizerPopup::onJump));
  jump->setID("jump-button");
  jump->setPosition({kPreviewSize.width / 2.f - 18.f, 24.f});
  menu->addChild(jump);

  // Run, and stop while running
  auto run = CCMenuItemToggler::create(iconButton("run", CircleBaseColor::Green, 22.f, "Run"), iconButton("stop", CircleBaseColor::Pink, 22.f, "Stop"),
                                       this, menu_selector(CustomizerPopup::onRun));
  run->setID("run-button");
  run->setPosition({kPreviewSize.width / 2.f + 18.f, 24.f});
  menu->addChild(run);

  // Main | Icon, when the icon in the preview has a look of its own
  // Tools on the left of the icon, one under the other: the wind view
  m_showWind = Mod::get()->getSavedValue<bool>(kWindSave, false);
  auto wind = CCMenuItemToggler::create(iconButton("wind", CircleBaseColor::Gray, 22.f, "Wind"), iconButton("wind", CircleBaseColor::Cyan, 22.f, "Wind"),
                                        this, menu_selector(CustomizerPopup::onWind));
  wind->toggle(m_showWind);
  wind->setID("wind-toggle");
  wind->setPosition({kToolX, kToolTop});
  menu->addChild(wind);
  m_windMeter = CCDrawNode::create();
  m_windMeter->setID("wind-meter");
  m_windMeter->setPosition({kToolX, kToolTop - 24.f});
  panel->addChild(m_windMeter);

  m_lookSwitch = CCMenu::create();
  m_lookSwitch->setPosition({0.f, 0.f});
  m_lookSwitch->setContentSize(kPreviewSize);
  m_lookSwitch->setTouchPriority(m_list->getTouchPriority() - 1);
  panel->addChild(m_lookSwitch);
  m_mainSprite = textButton("Main", 52.f, "GJ_button_01.png", 18.f);
  m_iconSprite = textButton("Icon", 52.f, "GJ_button_04.png", 18.f);
  for (int i = 0; i < 2; ++i)
  {
    auto segment = CCMenuItemSpriteExtra::create(i == 0 ? m_mainSprite : m_iconSprite, this, menu_selector(CustomizerPopup::onLookSwitch));
    segment->setTag(i);
    segment->setID(i == 0 ? "main-look-button" : "icon-look-button");
    segment->setPosition({kPreviewSize.width / 2.f + (i == 0 ? -27.f : 27.f), 48.f});
    m_lookSwitch->addChild(segment);
  }

  this->updatePreviewIcon();
}

void CustomizerPopup::updatePreviewIcon()
{
  auto gm = GameManager::get();
  auto const &mode = kModes[m_mode];

  m_player->updatePlayerFrame(gm->activeIconForType(mode.type), mode.type);
  // A custom icon from More Icons, if it's installed and one is on (nothing happens otherwise)
  more_icons::updateSimplePlayer(m_player, mode.type);
  m_player->setColor(gm->colorForIdx(gm->getPlayerColor()));
  m_player->setSecondColor(gm->colorForIdx(gm->getPlayerColor2()));
  if (gm->getPlayerGlow())
    m_player->setGlowOutline(gm->colorForIdx(gm->getPlayerGlowColor()));
  else
    m_player->disableGlowOutline();

  m_modeLabel->setString(mode.name);
  this->refreshLookSwitch();

  // Robot and spider hair appears on the first switch to them
  this->applyHitboxes();
  m_jumpTime = -1.f;
  m_spin = 0.f;
  m_player->setRotation(0.f);
}

void CustomizerPopup::update(float dt)
{
  this->trackUndo(dt);
  this->updateWind(dt);
  if (m_rebuildPending)
  {
    m_rebuildPending = false;
    this->rebuildList(true);
  }
  // At most a few times a second while a slider is dragged
  m_lookTimer += dt;
  if (m_lookVersion != HairConfig::version() && m_lookTimer > .2f)
    this->refreshLookLabel();

  auto const type = kModes[m_mode].type;

  if (m_running)
  {
    m_runDistance += kRunSpeed * dt;

    // The ball rolls along the ground
    if (type == IconType::Ball)
    {
      float const degrees = kRunSpeed * dt / (kHeadRadius * kPreviewScale) * 180.f / std::numbers::pi_v<float>;
      m_spin = std::fmod(m_spin + degrees, 360.f);
    }
  }

  float height = 0.f;
  float rotation = m_spin;
  if (m_jumpTime >= 0.f)
  {
    m_jumpTime += dt;
    float const t = std::min(m_jumpTime / kJumpDuration, 1.f);
    height = 4.f * kJumpHeight * t * (1.f - t);

    // Half a turn per jump like in the game, so the hair spins with the cube
    if (type == IconType::Cube)
      rotation = m_spin + 180.f * t;

    if (t >= 1.f)
    {
      m_jumpTime = -1.f;
      if (type == IconType::Cube)
        m_spin = std::fmod(m_spin + 180.f, 360.f);
      rotation = m_spin;
    }
  }

  // The icon really moves through the stage (that's what the hair reacts to),
  // the stage moves back so the icon stays in the middle of the preview
  m_player->setPosition({m_runDistance, height});
  m_player->setRotation(rotation);
  m_stage->setPositionX(kStagePosition.x - m_runDistance);
}

// ! --- Sections --- !

void CustomizerPopup::buildTabs()
{
  if (m_tabMenu)
    m_tabMenu->removeFromParent();

  m_tabMenu = CCMenu::create();
  m_tabMenu->ignoreAnchorPointForPosition(false);
  m_tabMenu->setAnchorPoint({.5f, .5f});
  m_tabMenu->setContentSize({kListSize.width, 30.f});
  m_tabMenu->setLayout(RowLayout::create()->setGap(4.f));
  m_tabMenu->setTouchPriority(m_list->getTouchPriority() - 1);
  m_tabMenu->setID("tabs");

  auto const &list = customizerSections();
  for (size_t i = 0; i < list.size(); ++i)
  {
    bool const selected = m_query.empty() && !m_onNow && i == m_section;
    auto sprite = textButton(list[i].name, 40, selected ? "GJ_button_01.png" : "GJ_button_04.png");
    auto tab = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(CustomizerPopup::onTab));
    tab->setTag(static_cast<int>(i));
    m_tabMenu->addChild(tab);
  }
  m_tabMenu->updateLayout();

  m_mainLayer->addChildAtPosition(m_tabMenu, Anchor::BottomLeft,
                                  {kListOrigin.x + kListSize.width / 2.f, kTabsY});
}

void CustomizerPopup::showSection(size_t index)
{
  auto const &list = customizerSections();
  if (index >= list.size())
    return;
  m_section = index;

  this->clearList();
  for (auto const &group : customizerGroups())
  {
    if (group.section == index)
      this->addGroup(group, group.name);
  }

  m_list->m_contentLayer->updateLayout();
  m_list->scrollToTop();
}

void CustomizerPopup::showOnNow()
{
  this->clearList();
  auto const &sections = customizerSections();
  bool any = false;
  for (auto const &group : customizerGroups())
  {
    // Parts of the look only: the mod's own switches (General) and physics aren't things you wear
    std::string_view const tab = sections[group.section].name;
    if (!group.master || tab == "General" || tab == "Physics" || !groupIsOn(group))
      continue;
    any = true;
    this->addGroup(group, group.name == tab ? group.name : fmt::format("{} > {}", tab, group.name));
  }

  if (!any)
  {
    auto row = CCNode::create();
    row->setContentSize({kListSize.width, 40.f});
    auto label = CCLabelBMFont::create("Nothing is on: turn things on in the tabs", "bigFont.fnt");
    label->limitLabelWidth(kListSize.width - 20.f, .35f, .1f);
    label->setOpacity(150);
    label->setPosition({kListSize.width / 2.f, 20.f});
    row->addChild(label);
    m_list->m_contentLayer->addChild(row);
  }

  m_list->m_contentLayer->updateLayout();
  m_list->scrollToTop();
}

void CustomizerPopup::rebuildList(bool keepScroll)
{
  // Keep the distance from the top of the list
  auto content = m_list->m_contentLayer;
  float const fromTop = content->getContentSize().height + content->getPositionY() - m_list->getContentSize().height;

  if (!m_query.empty())
    this->showSearch(m_query);
  else if (m_onNow)
    this->showOnNow();
  else
    this->showSection(m_section);

  if (keepScroll && fromTop > 0.f)
    content->setPositionY(std::min(content->getPositionY() + fromTop, 0.f));
}

// ! --- Blocks --- !

bool CustomizerPopup::isOpen(std::string const &id) const
{
  return m_open.contains(id);
}

void CustomizerPopup::setOpen(std::string const &id, bool open)
{
  if (open)
    m_open.insert(id);
  else
    m_open.erase(id);
  Mod::get()->setSavedValue(kOpenSave, std::vector<std::string>(m_open.begin(), m_open.end()));
}

void CustomizerPopup::addGroup(CustomizerGroup const &group, std::string const &title)
{
  // A block that is off shows only its switch
  bool const on = groupIsOn(group);
  bool const expandable = (!group.hidesWhenOff || on) && !group.keys.empty();
  bool const open = expandable && this->isOpen(group.id);
  m_list->m_contentLayer->addChild(this->createGroupHeader(group, title, open, expandable));

  // Reset covers the whole block, folded or not
  if (group.master)
    m_shownKeys.push_back(group.master);
  for (auto key : group.keys)
  {
    if (key[0] != '@')
      m_shownKeys.push_back(key);
  }
  if (!open)
    return;

  std::vector<char const *> fine;
  for (auto key : group.keys)
  {
    if (isFineTuning(key))
      fine.push_back(key);
    else
      this->addRow(key);
  }
  if (fine.empty())
    return;

  bool const fineOpen = this->isOpen(group.id + "+fine");
  m_list->m_contentLayer->addChild(this->createFineRow(group, fine.size(), fineOpen));
  if (fineOpen)
  {
    for (auto key : fine)
      this->addRow(key);
  }
}

CCNode *CustomizerPopup::createGroupHeader(CustomizerGroup const &group, std::string const &title, bool open, bool expandable)
{
  float const width = kListSize.width;
  auto header = CCNode::create();
  header->setContentSize({width, kHeaderHeight});
  header->setAnchorPoint({.5f, .5f});
  header->setID(fmt::format("{}-header", group.id));

  auto background = CCLayerColor::create({0, 0, 0, 70}, width, kHeaderHeight);
  header->addChild(background);

  // The switch of the block on the right, the block name instead of its own
  float titleRight = kHeaderTextLeft;
  SettingRow *row = group.master ? SettingRow::create(group.master, width) : nullptr;
  if (row)
  {
    row->setPosition({width / 2.f, kHeaderHeight / 2.f});
    titleRight = row->useAsHeader(title, kHeaderTextLeft, !groupIsOn(group));
    row->setOnChange([this, group, wasOn = groupIsOn(group)]() mutable
                     {
                       bool const on = groupIsOn(group);
                       if (on == wasOn || !group.hidesWhenOff)
                         return;
                       wasOn = on;
                       // Turned on: its settings open right away
                       if (on)
                         this->setOpen(group.id, true);
                       m_rebuildPending = true;
                     });
    header->addChild(row);
    m_rows.push_back(row);
  }
  else
  {
    auto label = CCLabelBMFont::create(title.c_str(), "goldFont.fnt");
    label->setAnchorPoint({0.f, .5f});
    label->limitLabelWidth(width - kHeaderTextLeft - 10.f, .5f, .1f);
    label->setPosition({kHeaderTextLeft, kHeaderHeight / 2.f});
    header->addChild(label);
    titleRight = kHeaderTextLeft + label->getScaledContentWidth();
  }

  if (!expandable)
    return header;

  // The arrow and the name fold and unfold the block
  auto arrow = CCSprite::createWithSpriteFrameName("navArrowBtn_001.png");
  arrow->setScale(.28f);
  arrow->setRotation(open ? 90.f : 0.f);
  arrow->setPosition({10.f, kHeaderHeight / 2.f});
  header->addChild(arrow);

  auto menu = CCMenu::create();
  menu->setPosition({0.f, 0.f});
  menu->setContentSize(header->getContentSize());
  header->addChild(menu);

  auto hitArea = CCNode::create();
  hitArea->setContentSize({titleRight + 4.f, kHeaderHeight});
  auto fold = CCMenuItemExt::createSpriteExtra(hitArea, [this, id = group.id, open](auto)
                                               {
                                                 this->setOpen(id, !open);
                                                 m_rebuildPending = true;
                                               });
  fold->m_scaleMultiplier = 1.f;
  fold->setPosition({(titleRight + 4.f) / 2.f, kHeaderHeight / 2.f});
  menu->addChild(fold);
  return header;
}

CCNode *CustomizerPopup::createFineRow(CustomizerGroup const &group, size_t count, bool open)
{
  float const width = kListSize.width;
  auto row = CCNode::create();
  row->setContentSize({width, 22.f});
  row->setAnchorPoint({.5f, .5f});

  auto menu = CCMenu::create();
  menu->setPosition({0.f, 0.f});
  menu->setContentSize(row->getContentSize());
  row->addChild(menu);

  std::string const text = open ? "Hide fine tuning" : fmt::format("Fine tuning: position and detail ({})", count);
  auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
  label->limitLabelWidth(width - 30.f, .3f, .1f);
  label->setColor({150, 205, 255});
  auto button = CCMenuItemExt::createSpriteExtra(label, [this, id = group.id + "+fine", open](auto)
                                                 {
                                                   this->setOpen(id, !open);
                                                   m_rebuildPending = true;
                                                 });
  button->setPosition({width / 2.f, 11.f});
  menu->addChild(button);
  return row;
}

void CustomizerPopup::onOnNow(CCObject *)
{
  m_onNow = !m_onNow;
  m_query.clear();
  m_search->setString("");
  m_onNowSprite->updateBGImage(m_onNow ? "GJ_button_01.png" : "GJ_button_04.png");
  this->rebuildList(false);
  this->buildTabs();
}

void CustomizerPopup::clearList()
{
  m_list->m_contentLayer->removeAllChildren();
  m_rows.clear();
  m_shownKeys.clear();
}

void CustomizerPopup::addRow(char const *key)
{
  if (std::string_view(key) == "@looks")
  {
    m_list->m_contentLayer->addChild(createLooksRow(kListSize.width));
    return;
  }
  if (std::string_view(key) == "@gallery")
  {
    m_list->m_contentLayer->addChild(createGalleryRow(kListSize.width));
    return;
  }
  if (std::string_view(key) == "@linker")
  {
    m_list->m_contentLayer->addChild(createLinkerRow(kListSize.width));
    return;
  }
  if (std::string_view(key) == "@emote-keys")
  {
    m_list->m_contentLayer->addChild(createEmoteKeysRow(kListSize.width));
    return;
  }

  if (auto row = SettingRow::create(key, kListSize.width))
  {
    m_list->m_contentLayer->addChild(row);
    m_rows.push_back(row);
    m_shownKeys.push_back(key);
  }
}

void CustomizerPopup::showSearch(std::string const &query)
{
  auto lower = [](std::string text)
  {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });
    return text;
  };
  std::string const needle = lower(query);
  auto matches = [&](char const *key, std::shared_ptr<SettingV3> const &setting)
  {
    return lower(key).find(needle) != std::string::npos ||
           lower(setting->getDisplayName()).find(needle) != std::string::npos ||
           lower(setting->getDescription().value_or("")).find(needle) != std::string::npos;
  };

  this->clearList();
  for (auto const &section : customizerSections())
  {
    // A header for each tab with matches: "Tab", or "Tab > Section" under a title.
    // A matching title brings its whole block along
    std::string title;
    bool titleMatches = false;
    std::string lastHeader;
    for (auto key : section.keys)
    {
      auto setting = Mod::get()->getSetting(key);
      if (!setting)
        continue;
      if (typeinfo_pointer_cast<TitleSettingV3>(setting))
      {
        title = setting->getDisplayName();
        titleMatches = matches(key, setting);
        continue;
      }
      if (!titleMatches && !matches(key, setting))
        continue;

      std::string const header = title.empty() ? section.name : fmt::format("{} > {}", section.name, title);
      if (header != lastHeader)
      {
        lastHeader = header;
        auto row = CCNode::create();
        row->setContentSize({kListSize.width, 18.f});
        auto label = CCLabelBMFont::create(header.c_str(), "goldFont.fnt");
        label->setScale(.45f);
        label->setAnchorPoint({0.f, .5f});
        label->setPosition({6.f, 9.f});
        row->addChild(label);
        m_list->m_contentLayer->addChild(row);
      }
      this->addRow(key);
    }
  }

  if (m_rows.empty())
  {
    auto row = CCNode::create();
    row->setContentSize({kListSize.width, 40.f});
    auto label = CCLabelBMFont::create("Nothing found", "bigFont.fnt");
    label->setScale(.4f);
    label->setOpacity(150);
    label->setPosition({kListSize.width / 2.f, 20.f});
    row->addChild(label);
    m_list->m_contentLayer->addChild(row);
  }

  m_list->m_contentLayer->updateLayout();
  m_list->scrollToTop();
}

// ! --- Callbacks --- !

void CustomizerPopup::onTab(CCObject *sender)
{
  auto const index = static_cast<size_t>(static_cast<CCNode *>(sender)->getTag());
  if (index == m_section && m_query.empty() && !m_onNow)
    return;

  // A tab ends the search and "On now"
  m_query.clear();
  m_search->setString("");
  m_onNow = false;
  m_onNowSprite->updateBGImage("GJ_button_04.png");
  this->showSection(index);
  this->buildTabs();
}

void CustomizerPopup::onMode(CCObject *sender)
{
  int const count = static_cast<int>(kModes.size());
  int const dir = static_cast<CCNode *>(sender)->getTag();
  m_mode = static_cast<size_t>((static_cast<int>(m_mode) + dir + count) % count);
  this->updatePreviewIcon();
}

// ! --- Undo and quick looks --- !

void CustomizerPopup::trackUndo(float dt)
{
  // Any change (a slider drag, a preset, Surprise, Reset) starts a burst; the look from before it
  // goes on the stack once, and after a short quiet moment the new look becomes the stable one
  unsigned const version = HairConfig::version();
  if (version != m_seenVersion)
  {
    m_seenVersion = version;
    m_quiet = 0.f;
    if (!m_changing)
    {
      m_undo.push_back(m_stable);
      if (m_undo.size() > kUndoSteps)
        m_undo.erase(m_undo.begin());
      m_changing = true;
    }
    return;
  }

  if (m_changing)
  {
    m_quiet += dt;
    if (m_quiet > kUndoQuiet)
    {
      m_stable = presets::capture("Undo");
      m_changing = false;
    }
  }
}

void CustomizerPopup::onUndo(CCObject *)
{
  if (m_undo.empty())
  {
    Notification::create("Nothing to undo", NotificationIcon::Info)->show();
    return;
  }

  confirmOnce("undo", "Undo", "Takes back your last change of the look. Tap it again to go further back",
              [self = Ref(this)]
              {
                if (self->m_undo.empty())
                  return;
                Preset const previous = self->m_undo.back();
                self->m_undo.pop_back();
                presets::apply(previous);

                // Going back is not a new change
                self->m_stable = previous;
                self->m_seenVersion = HairConfig::version();
                self->m_changing = false;
                self->m_rebuildPending = true;
              });
}

void CustomizerPopup::applyLook(Preset const &preset)
{
  presets::apply(preset);
  m_rebuildPending = true;
}

void CustomizerPopup::onSurprise(CCObject *)
{
  confirmOnce("surprise", "Surprise", "Puts a <cp>random cute look</c> on your icon: hairstyle, colors and accessories. "
                                      "Undo (the arrow) brings yours back, saving it first keeps it for sure",
              [self = Ref(this)]
              { self->applyLook(presets::surprise()); });
}

void CustomizerPopup::onColors(CCObject *)
{
  confirmOnce("colors", "Colors", "Gives your look a <cl>random palette</c>: the hair color and an accent for the accessories. "
                                  "The shapes stay. Undo (the arrow) brings your colors back",
              [self = Ref(this)]
              { self->applyLook(presets::surpriseColors()); });
}

void CustomizerPopup::onMatch(CCObject *)
{
  confirmOnce("match", "Match", "Colors every part of the hair (face locks, bangs, tails, ears) <cg>like the hair</c>. "
                                "Undo (the arrow) brings the old colors back",
              [self = Ref(this)]
              { self->applyLook(presets::matchColors()); });
}

void CustomizerPopup::onJump(CCObject *)
{
  if (m_jumpTime < 0.f)
    m_jumpTime = 0.f;
}

void CustomizerPopup::onWind(CCObject *sender)
{
  // The toggler flips its state after the callback
  m_showWind = !static_cast<CCMenuItemToggler *>(sender)->isToggled();
  Mod::get()->setSavedValue(kWindSave, m_showWind);
  if (!m_showWind)
  {
    m_streaks.clear();
    m_wind->clear();
    m_windFront->clear();
    m_windMeter->clear();
  }
}

void CustomizerPopup::updateWind(float dt)
{
  auto hair = getSimplePlayerHair(m_player).icon;
  m_windHint->setVisible(false);
  if (!m_showWind || !hair)
    return;

  auto const air = hair->airFlow();
  float const strength = std::clamp(air.strength, 0.f, 2.f);
  float const flow = std::min(strength, 1.f);
  CCPoint const toward = air.toward;
  CCPoint const across = {-toward.y, toward.x};
  m_windHint->setVisible(strength < .03f && m_streaks.empty());

  // The draw nodes blend premultiplied colors: the alpha has to be in the color too
  auto tint = [](float r, float g, float b, float a)
  { return ccColor4F{r * a, g * a, b * a, a}; };

  auto random = [this]
  {
    m_windSeed = m_windSeed * 1664525u + 1013904223u;
    return static_cast<float>(m_windSeed >> 8) / static_cast<float>(1u << 24);
  };

  // New air comes in on the windward edge, as much as the flow is strong: streaks at every
  // depth, and motes of dust it carries
  auto spawn = [&](bool mote)
  {
    if (m_streaks.size() >= kMaxStreaks)
      return;
    WindStreak streak;
    streak.depth = random();
    streak.length = mote ? 0.f : (10.f + 16.f * random() + 16.f * flow) * (.7f + .5f * streak.depth);
    float const y = 18.f + (kPreviewSize.height - 50.f) * random();
    float const margin = streak.length + 4.f;
    streak.position = CCPoint{toward.x < 0.f ? kPreviewSize.width + margin : -margin, y};
    streak.phase = random() * 6.28f;
    streak.speed = (.8f + .4f * random()) * (.65f + .55f * streak.depth);
    if (!mote && air.gust > kCurlGust && streak.depth > .55f && random() < .35f)
      streak.curl = random() < .5f ? -1.f : 1.f;
    m_streaks.push_back(streak);
  };
  m_windSpawn += strength * kStreakRate * dt;
  while (m_windSpawn >= 1.f)
  {
    m_windSpawn -= 1.f;
    spawn(random() < kMoteRate / kStreakRate);
  }

  // The air flows with the wind, faster when it is strong
  float const speed = kStreakSlow + kStreakFast * std::min(strength, 1.5f);
  for (auto &streak : m_streaks)
    streak.position = streak.position + toward * (speed * streak.speed * dt);
  std::erase_if(m_streaks, [](WindStreak const &streak)
                {
                  float const margin = streak.length * 2.f + 10.f;
                  return streak.position.x < -margin || streak.position.x > kPreviewSize.width + margin;
                });

  // Around the head: the air parts above and below it, behind it there is a calm wake
  CCPoint const head = m_stage->getPosition() + m_player->getPosition();
  float const reach = kHeadAir * 1.9f;
  auto around = [&](CCPoint const &point, float &shade, bool inFront)
  {
    CCPoint const r = point - head;
    float const distance = std::max(r.getLength(), 1.f);
    shade = 1.f;
    if (distance > reach * 1.6f)
      return point;
    // Like the flow around a ball: pushed out the more the closer, smoothly, and nothing on the
    // line through the middle (that air goes behind the icon, or fades in front of it)
    float const sideways = r.dot(across);
    float const falloff = std::clamp(1.f - distance / (reach * 1.6f), 0.f, 1.f);
    float const near2 = std::max(distance, kHeadAir) * std::max(distance, kHeadAir);
    float const push = std::clamp(sideways * kHeadAir * kHeadAir / near2 * falloff * falloff, -kHeadAir * .9f, kHeadAir * .9f);
    CCPoint const moved = point + across * push;
    // In the wake, right behind the head, the air is weaker
    float const behind = r.dot(toward);
    if (behind > 0.f && std::abs(sideways) < kHeadAir)
      shade = std::clamp(.35f + behind / (kHeadAir * 5.f), .35f, 1.f);
    if (inFront && (moved - head).getLength() < kHeadAir * .95f)
      shade *= .25f;
    return moved;
  };

  m_wind->clear();
  m_windFront->clear();
  float const wave = air.flutter * kFlutterWave * flow;
  float const bright = std::clamp(.22f + .3f * strength * air.gust, .12f, .65f);
  for (auto const &streak : m_streaks)
  {
    auto node = streak.depth > .6f ? m_windFront : m_wind;
    float const alpha = bright * (.45f + .55f * streak.depth) * (streak.depth > .6f ? .8f : 1.f);
    float const width = .35f + .75f * streak.depth;
    auto edgeFade = [](CCPoint const &point)
    {
      float const edge = std::min(std::min(point.x, kPreviewSize.width - point.x), std::min(point.y, kPreviewSize.height - point.y));
      return std::clamp(edge / 14.f, 0.f, 1.f);
    };

    if (streak.length <= 0.f)
    {
      // A mote of dust: tumbling with the flutter
      float shade = 1.f;
      CCPoint const wobble = across * (std::sin(streak.phase + air.time * 11.f) * (1.5f + wave * .6f));
      CCPoint const point = around(streak.position + wobble, shade, streak.depth > .6f);
      float const fade = edgeFade(point) * shade;
      if (fade > 0.f)
        node->drawDot(point, .6f + .7f * streak.depth, tint(1.f, .97f, .92f, alpha * 1.3f * fade));
      continue;
    }

    // A streak in pieces: thin at both ends, waving with the flutter, bent around the head
    constexpr int kPieces = 8;
    std::array<CCPoint, kPieces + 1> points;
    std::array<float, kPieces + 1> shades;
    for (int i = 0; i <= kPieces; ++i)
    {
      float const t = static_cast<float>(i) / kPieces;
      float const offset = std::sin(streak.phase + air.time * 9.f + t * 3.2f) * wave * (.3f + t);
      points[i] = around(streak.position - toward * (streak.length * t) + across * offset, shades[i], streak.depth > .6f);
    }
    for (int i = 1; i <= kPieces; ++i)
    {
      float const t = (static_cast<float>(i) - .5f) / kPieces;
      float const taper = std::sin(t * 3.14159f);
      float const fade = edgeFade(points[i]) * shades[i] * (1.f - t * .35f);
      if (fade <= 0.f)
        continue;
      // A soft glow under the near ones
      if (streak.depth > .6f)
        node->drawSegment(points[i - 1], points[i], width * 2.4f * taper, tint(.75f, .9f, 1.f, alpha * .18f * fade));
      node->drawSegment(points[i - 1], points[i], std::max(width * taper, .2f), tint(1.f, 1.f, 1.f, alpha * fade));
    }

    // A strong gust curls the front of some near streaks, like wind in anime
    if (streak.curl != 0.f)
    {
      float const radius = 3.f + 3.f * streak.depth;
      CCPoint const center = points[0] + across * (streak.curl * radius);
      CCPoint previous = points[0];
      for (int i = 1; i <= 6; ++i)
      {
        float const angle = static_cast<float>(i) / 6.f * 4.4f;
        CCPoint const radial = across * (-streak.curl * std::cos(angle)) + toward * std::sin(angle);
        CCPoint const point = center + radial * (radius * (1.f - .12f * static_cast<float>(i) / 6.f));
        float const fade = edgeFade(point) * (1.f - static_cast<float>(i) / 8.f);
        if (fade > 0.f)
          node->drawSegment(previous, point, width * .8f, tint(1.f, 1.f, 1.f, alpha * fade));
        previous = point;
      }
    }
  }

  // The meter: how strong the flow is, the tick where it combs the hair fully
  m_windMeter->clear();
  float const half = kMeterWidth / 2.f;
  std::array<CCPoint, 4> back = {CCPoint{-half, -1.5f}, CCPoint{half, -1.5f}, CCPoint{half, 1.5f}, CCPoint{-half, 1.5f}};
  m_windMeter->drawPolygon(back.data(), 4, tint(0.f, 0.f, 0.f, .45f), 0.f, tint(0.f, 0.f, 0.f, 0.f));
  float const filled = -half + kMeterWidth * std::clamp(strength / 1.5f, 0.f, 1.f);
  if (filled > -half + .5f)
  {
    std::array<CCPoint, 4> bar = {CCPoint{-half, -1.f}, CCPoint{filled, -1.f}, CCPoint{filled, 1.f}, CCPoint{-half, 1.f}};
    auto const color = strength >= 1.f ? tint(.55f, 1.f, .65f, .95f) : tint(.45f, .9f, 1.f, .95f);
    m_windMeter->drawPolygon(bar.data(), 4, color, 0.f, color);
  }
  float const tick = -half + kMeterWidth / 1.5f;
  m_windMeter->drawSegment(CCPoint{tick, -2.5f}, CCPoint{tick, 2.5f}, .4f, tint(1.f, 1.f, 1.f, .8f));
}

void CustomizerPopup::onRun(CCObject *)
{
  m_running = !m_running;
}

void CustomizerPopup::onHitboxes(CCObject *sender)
{
  // The toggler flips its state after the callback
  m_showHitboxes = !static_cast<CCMenuItemToggler *>(sender)->isToggled();
  Mod::get()->setSavedValue(kHitboxesSave, m_showHitboxes);
  this->applyHitboxes();
}

void CustomizerPopup::applyHitboxes()
{
  if (m_legend)
    m_legend->setVisible(m_showHitboxes);

  auto const hair = getSimplePlayerHair(m_player);
  for (auto node : {hair.icon, hair.robot, hair.spider})
  {
    if (node)
      node->setDebugDraw(m_showHitboxes);
  }
}

void CustomizerPopup::onPresets(CCObject *)
{
  auto popup = PresetsPopup::create([self = Ref(this)]
                                    {
                                      self->m_rebuildPending = true;
                                    });
  if (popup)
    popup->show();
}

void CustomizerPopup::refreshLookSwitch()
{
  if (!m_lookSwitch)
    return;
  auto const mode = gameModeOf(kModes[m_mode].type);
  std::string const iconLook = looks::linkedLook(mode);
  m_lookSwitch->setVisible(m_editingIcon || (!iconLook.empty() && presets::find(iconLook)));
  m_mainSprite->updateBGImage(m_editingIcon ? "GJ_button_04.png" : "GJ_button_01.png");
  m_iconSprite->updateBGImage(m_editingIcon ? "GJ_button_01.png" : "GJ_button_04.png");
}

void CustomizerPopup::onLookSwitch(CCObject *sender)
{
  bool const icon = static_cast<CCNode *>(sender)->getTag() == 1;
  if (icon == m_editingIcon)
    return;
  if (!icon)
  {
    this->endIconEdit(nullptr);
    return;
  }

  this->startIconEdit(true);
}

bool CustomizerPopup::startIconEdit(bool tell)
{
  auto const mode = gameModeOf(kModes[m_mode].type);
  std::string const name = looks::linkedLook(mode);
  auto preset = name.empty() ? std::nullopt : presets::find(name);
  if (!preset)
  {
    if (tell)
      Notification::create("This icon has no look of its own", NotificationIcon::Info)->show();
    return false;
  }

  // The main look waits aside, the icon look goes in to be edited: Save keeps it in its preset
  presets::stashMainLook();
  presets::load(*preset);
  m_editingIcon = true;
  m_iconLook = name;
  m_undo.clear();
  m_stable = presets::capture("Undo");
  m_seenVersion = HairConfig::version();
  m_rebuildPending = true;
  this->refreshLookSwitch();
  this->refreshLookLabel();
  if (tell)
    Notification::create(fmt::format("Editing the look of this icon, \"{}\"", name), NotificationIcon::Info)->show();
  return true;
}

void CustomizerPopup::endIconEdit(std::function<void()> then)
{
  auto finish = [self = Ref(this), then]
  {
    presets::restoreMainLook();
    self->m_editingIcon = false;
    self->m_iconLook.clear();
    self->m_undo.clear();
    self->m_stable = presets::capture("Undo");
    self->m_seenVersion = HairConfig::version();
    self->m_rebuildPending = true;
    self->refreshLookSwitch();
    self->refreshLookLabel();
    if (then)
      then();
  };

  if (!presets::modified())
  {
    finish();
    return;
  }
  // Save first: closing the question keeps the changes too
  createQuickPopup("Icon look", fmt::format("Save the changes to <cy>{}</c>?", m_iconLook), "Save", "Discard",
                   [finish, name = m_iconLook](FLAlertLayer *, bool discard)
                   {
                     if (!discard)
                     {
                       if (auto result = presets::save(presets::capture(name)); !result)
                         Notification::create(fmt::format("Unable to save: {}", result.unwrapErr()), NotificationIcon::Error)->show();
                       else
                         Notification::create(fmt::format("Saved \"{}\"", name), NotificationIcon::Success)->show();
                     }
                     finish();
                   });
}

void CustomizerPopup::onClose(CCObject *sender)
{
  // The main look comes back before the customizer goes
  if (m_editingIcon)
  {
    this->endIconEdit([self = Ref(this)]
                      { self->Popup::onClose(nullptr); });
    return;
  }
  Popup::onClose(sender);
}

void CustomizerPopup::onSave(CCObject *)
{
  saveLook::quick([self = Ref(this)]
                  { self->refreshLookLabel(); });
}

void CustomizerPopup::refreshLookLabel()
{
  m_lookVersion = HairConfig::version();
  m_lookTimer = 0.f;
  bool const changed = presets::current().empty() || presets::modified();
  m_lookLabel->setString(fmt::format("{}{}", saveLook::currentName(), changed ? " *" : "").c_str());
  m_lookLabel->setColor(changed ? ccColor3B{255, 205, 80} : ccColor3B{130, 255, 140});
  m_lookLabel->limitLabelWidth(120.f, .32f, .1f);
}

void CustomizerPopup::onReset(CCObject *)
{
  // The settings on screen: the tab, or what the search found
  std::string const what = !m_query.empty() ? "the <cy>found</c> settings"
                           : m_onNow        ? "everything that is <cy>on</c> (it turns off)"
                                            : fmt::format("all <cy>{}</c> settings", customizerSections()[m_section].name);

  createQuickPopup(
      "Reset",
      fmt::format("Reset {} to their defaults?", what),
      "Cancel", "Reset",
      [self = Ref(this)](FLAlertLayer *, bool confirmed)
      {
        if (!confirmed)
          return;

        for (auto key : self->m_shownKeys)
        {
          if (auto setting = Mod::get()->getSetting(key))
            setting->reset();
        }
        self->m_rebuildPending = true;
      });
}
