#include "CustomizerPopup.hpp"

#include "../hooks/SimplePlayerHair.hpp"

#include <Geode/ui/NineSlice.hpp>
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
  constexpr CCSize kListSize = {240.f, 170.f};
  constexpr float kTabsY = 228.f;

  // Hitbox helpers, remembered between openings
  constexpr char const *kHitboxesSave = "customizer-hitboxes";

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

  // ! --- Sections --- !
  // A new customization only needs its settings in mod.json and a section here

  struct Section
  {
    char const *name;
    std::vector<char const *> keys;
  };

  std::vector<Section> const &sections()
  {
    static std::vector<Section> const list{
        {"Hair",
         {
             "enabled",
             "show-in-garage",
             "style",
             "spin-with-icon",
             "density",
             "hair-length",
             "segments",
             "lock-width",
             "physics-title",
             "volume",
             "hitbox-multiplier",
             "wind-multiplier",
             "gravity",
             "damping",
             "look-title",
             "color-source",
             "custom-color",
             "outline",
         }},
        {"Front",
         {
             "face-locks-title",
             "face-locks",
             "face-lock-left",
             "face-lock-right",
             "face-lock-length",
             "face-lock-width",
             "face-lock-inset-x",
             "face-lock-inset-y",
             "face-lock-color",
             "face-lock-custom-color",
             "bangs-title",
             "bangs",
             "bangs-length",
             "bangs-density",
             "bangs-spread",
             "bangs-arc-size",
             "bangs-arc-softness",
             "bangs-inset-x",
             "bangs-inset-y",
             "bangs-color",
             "bangs-custom-color",
         }},
    };
    return list;
  }

  ButtonSprite *textButton(char const *text, int width, char const *texture = "GJ_button_01.png")
  {
    return ButtonSprite::create(text, width, true, "goldFont.fnt", texture, 24.f, .6f);
  }
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

  auto reset = CCMenuItemSpriteExtra::create(textButton("Reset", 60, "GJ_button_04.png"), this,
                                             menu_selector(CustomizerPopup::onReset));
  reset->setID("reset-button");
  m_buttonMenu->addChildAtPosition(reset, Anchor::BottomRight, {-50.f, 19.f});

  // Hitbox helpers toggle next to the reset button
  m_showHitboxes = Mod::get()->getSavedValue<bool>(kHitboxesSave, false);
  auto hitboxes = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(CustomizerPopup::onHitboxes), .5f);
  hitboxes->toggle(m_showHitboxes);
  hitboxes->setID("hitboxes-toggle");
  m_buttonMenu->addChildAtPosition(hitboxes, Anchor::BottomRight, {-160.f, 19.f});

  auto hitboxesLabel = CCLabelBMFont::create("Hitboxes", "bigFont.fnt");
  hitboxesLabel->setScale(.4f);
  hitboxesLabel->setAnchorPoint({0.f, .5f});
  m_mainLayer->addChildAtPosition(hitboxesLabel, Anchor::BottomRight, {-146.f, 19.f});
  this->applyHitboxes();

  this->showSection(0);
  this->scheduleUpdate();
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

  m_stage = CCNode::create();
  m_stage->setPosition(kStagePosition);
  m_stage->setID("stage");
  panel->addChild(m_stage);

  // What the hitbox helper colors mean
  m_legend = CCNode::create();
  m_legend->setID("hitboxes-legend");
  for (size_t i = 0; i < kLegend.size(); ++i)
  {
    auto label = CCLabelBMFont::create(kLegend[i].name, "bigFont.fnt");
    label->setScale(.28f);
    label->setColor(kLegend[i].color);
    label->setAnchorPoint({0.f, 1.f});
    label->setPosition({6.f, kPreviewSize.height - 5.f - static_cast<float>(i) * 9.f});
    m_legend->addChild(label);
  }
  panel->addChild(m_legend);

  m_player = SimplePlayer::create(0);
  m_player->setScale(kPreviewScale);
  m_stage->addChild(m_player);

  attachSimplePlayerHair(m_player, false);
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
    arrow->setPosition({kPreviewSize.width / 2.f + dir * 55.f, 58.f});
    menu->addChild(arrow);
  }

  m_modeLabel = CCLabelBMFont::create("", "bigFont.fnt");
  m_modeLabel->setScale(.45f);
  m_modeLabel->setPosition({kPreviewSize.width / 2.f, 58.f});
  panel->addChild(m_modeLabel);

  auto jump = CCMenuItemSpriteExtra::create(textButton("Jump", 50), this, menu_selector(CustomizerPopup::onJump));
  jump->setPosition({kPreviewSize.width / 2.f - 34.f, 24.f});
  menu->addChild(jump);

  m_runSprite = textButton("Run", 50);
  auto run = CCMenuItemSpriteExtra::create(m_runSprite, this, menu_selector(CustomizerPopup::onRun));
  run->setPosition({kPreviewSize.width / 2.f + 34.f, 24.f});
  menu->addChild(run);

  this->updatePreviewIcon();
}

void CustomizerPopup::updatePreviewIcon()
{
  auto gm = GameManager::get();
  auto const &mode = kModes[m_mode];

  m_player->updatePlayerFrame(gm->activeIconForType(mode.type), mode.type);
  m_player->setColor(gm->colorForIdx(gm->getPlayerColor()));
  m_player->setSecondColor(gm->colorForIdx(gm->getPlayerColor2()));
  if (gm->getPlayerGlow())
    m_player->setGlowOutline(gm->colorForIdx(gm->getPlayerGlowColor()));
  else
    m_player->disableGlowOutline();

  m_modeLabel->setString(mode.name);

  // Robot and spider hair appears on the first switch to them
  this->applyHitboxes();
  m_jumpTime = -1.f;
  m_spin = 0.f;
  m_player->setRotation(0.f);
}

void CustomizerPopup::update(float dt)
{
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
  m_tabMenu->setLayout(RowLayout::create()->setGap(6.f));
  m_tabMenu->setTouchPriority(m_list->getTouchPriority() - 1);
  m_tabMenu->setID("tabs");

  auto const &list = sections();
  for (size_t i = 0; i < list.size(); ++i)
  {
    auto sprite = textButton(list[i].name, 60, i == m_section ? "GJ_button_01.png" : "GJ_button_04.png");
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
  auto const &list = sections();
  if (index >= list.size())
    return;
  m_section = index;

  m_list->m_contentLayer->removeAllChildren();
  m_rows.clear();

  for (auto key : list[index].keys)
  {
    if (auto row = SettingRow::create(key, kListSize.width))
    {
      m_list->m_contentLayer->addChild(row);
      m_rows.push_back(row);
    }
  }

  m_list->m_contentLayer->updateLayout();
  m_list->scrollToTop();
}

// ! --- Callbacks --- !

void CustomizerPopup::onTab(CCObject *sender)
{
  auto const index = static_cast<size_t>(static_cast<CCNode *>(sender)->getTag());
  if (index == m_section)
    return;

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

void CustomizerPopup::onJump(CCObject *)
{
  if (m_jumpTime < 0.f)
    m_jumpTime = 0.f;
}

void CustomizerPopup::onRun(CCObject *)
{
  m_running = !m_running;
  m_runSprite->setString(m_running ? "Stop" : "Run");
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

void CustomizerPopup::onReset(CCObject *)
{
  auto const &section = sections()[m_section];

  createQuickPopup(
      "Reset",
      fmt::format("Reset all <cy>{}</c> settings to their defaults?", section.name),
      "Cancel", "Reset",
      [self = Ref(this)](FLAlertLayer *, bool confirmed)
      {
        if (!confirmed)
          return;

        for (auto key : sections()[self->m_section].keys)
        {
          if (auto setting = Mod::get()->getSetting(key))
            setting->reset();
        }
        for (auto row : self->m_rows)
          row->refresh();
      });
}
