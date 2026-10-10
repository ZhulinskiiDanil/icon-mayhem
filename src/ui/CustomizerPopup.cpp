#include "CustomizerPopup.hpp"
#include "GalleryPopup.hpp"
#include "LinkerPopup.hpp"
#include "LooksPopup.hpp"
#include "PartsPopup.hpp"
#include "PlayersPopup.hpp"

// More Icons is optional: its functions are reached through Geode events, without linking to it
#define MORE_ICONS_EVENTS
#include <hiimjustin000.more_icons/include/MoreIcons.hpp>

#include "../hair/HairConfig.hpp"
#include "../hooks/Globed.hpp"
#include "../hooks/LevelHair.hpp"
#include "../hooks/SimplePlayerHair.hpp"
#include "../presets/Looks.hpp"
#include "../settings/Settings.hpp"
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
  // The window takes the screen: the list gets the room, the preview keeps its size
  constexpr float kMaxWidth = 620.f;
  constexpr float kScreenMargin = 14.f;

  // Preview panel on the left
  constexpr CCSize kPreviewSize = {150.f, 190.f};
  constexpr CCPoint kStagePosition = {75.f, 112.f}; // in the preview panel
  constexpr float kPreviewScale = 1.3f;

  constexpr float kRunSpeed = 330.f;    // stage units / s, enough to comb the hair fully
  constexpr float kJumpDuration = .42f; // s, like a cube jump in the game
  constexpr float kJumpHeight = 45.f;   // stage units
  constexpr float kHeadRadius = 15.f;   // icon units, for the rolling ball
  constexpr float kModeY = 70.f;        // the game mode switcher under the ground
  constexpr float kLookSwitchY = 46.f;  // Main | Icon under it, above Jump and Run

  // Settings list on the right
  constexpr CCPoint kListOrigin = {180.f, 38.f};
  constexpr float kOnNowWidth = 58.f; // the "On now" button right of the search
  constexpr float kPartsWidth = 44.f; // "Parts" left of it

  // Rows of the list, smaller with − to see more settings at once
  constexpr char const *kRowScaleSave = "customizer-row-scale";
  constexpr float kRowScaleMin = .6f;
  constexpr float kRowScaleMax = 1.2f;
  constexpr float kRowScaleStep = .1f;

  struct Layout
  {
    float width = 0.f;
    float height = 0.f;
    CCPoint previewCenter;
    CCSize listSize;
    float searchY = 0.f;
    float searchWidth = 0.f;
    float tabsY = 0.f;
  };

  // From the screen size, once
  Layout const &layout()
  {
    static Layout const value = []
    {
      auto const screen = CCDirector::sharedDirector()->getWinSize();
      Layout out;
      out.width = std::clamp(screen.width - 2.f * kScreenMargin, 440.f, kMaxWidth);
      out.height = std::max(screen.height - kScreenMargin, 270.f);
      // The preview in the middle of the left column
      out.previewCenter = CCPoint{90.f, 113.f + (out.height - 270.f) / 2.f};
      out.tabsY = out.height - 42.f;
      out.searchY = out.height - 69.f;
      out.listSize = CCSize{out.width - kListOrigin.x - 20.f, out.searchY - 17.f - kListOrigin.y};
      out.searchWidth = out.listSize.width - kOnNowWidth - kPartsWidth - 8.f;
      return out;
    }();
    return value;
  }

  float rowScale()
  {
    return std::clamp(Mod::get()->getSavedValue<float>(kRowScaleSave, 1.f), kRowScaleMin, kRowScaleMax);
  }

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

// ! --- Tapping the preview --- !

namespace
{
  constexpr float kTapSlop = 8.f; // moved less than this: a tap, not a drag
  constexpr float kPartSlop = 4.f; // a thin lock this close to the finger counts
  // The part under the mouse brightens softly, breathing a little
  constexpr float kHoverOpacity = 70.f;
  constexpr float kHoverPulse = 25.f;
  constexpr float kHoverPulseSpeed = 5.f; // radians / s

  // Plain triangles in white, for the hover highlight. A draw node smooths the edges of every
  // polygon, which stretches thin pieces of hair into long spikes
  class FlatShape : public CCNode
  {
  public:
    static FlatShape *create()
    {
      auto node = new FlatShape();
      node->init();
      node->autorelease();
      return node;
    }

    void draw() override
    {
      if (m_triangles.size() < 3)
        return;
      static_assert(sizeof(CCPoint) == 2 * sizeof(GLfloat));
      auto shader = CCShaderCache::sharedShaderCache()->programForKey(kCCShader_Position_uColor);
      shader->use();
      shader->setUniformsForBuiltins();
      GLfloat white[4] = {1.f, 1.f, 1.f, 1.f};
      shader->setUniformLocationWith4fv(shader->getUniformLocationForName("u_color"), white, 1);
      shader->setUniformLocationWith1f(shader->getUniformLocationForName("u_pointSize"), 1.f);
      ccGLBindVAO(0);
      ccGLEnableVertexAttribs(kCCVertexAttribFlag_Position);
      glVertexAttribPointer(kCCVertexAttrib_Position, 2, GL_FLOAT, GL_FALSE, 0, m_triangles.data());
      glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_triangles.size()));
    }

    std::vector<CCPoint> m_triangles;
  };

  // A button (enabled and showing) somewhere under `node` is at the point
  bool isOnButton(CCNode *node, CCPoint const &world)
  {
    for (auto child : CCArrayExt<CCNode *>(node->getChildren()))
    {
      if (!child->isVisible())
        continue;
      if (auto item = typeinfo_cast<CCMenuItem *>(child))
      {
        if (item->isEnabled() && item->getParent() &&
            item->boundingBox().containsPoint(item->getParent()->convertToNodeSpace(world)))
          return true;
        continue;
      }
      if (isOnButton(child, world))
        return true;
    }
    return false;
  }

  // Reports taps on its area; buttons over it (registered first, same priority) win
  class TapArea : public CCLayer
  {
  public:
    static TapArea *create(CCSize const &size, int priority, std::function<void(CCPoint)> onTap)
    {
      auto layer = new TapArea();
      layer->init();
      layer->autorelease();
      layer->setContentSize(size);
      layer->m_priority = priority;
      layer->m_onTap = std::move(onTap);
      return layer;
    }

    void onEnter() override
    {
      CCLayer::onEnter();
      CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, m_priority, true);
    }

    void onExit() override
    {
      CCDirector::sharedDirector()->getTouchDispatcher()->removeDelegate(this);
      CCLayer::onExit();
    }

    bool ccTouchBegan(CCTouch *touch, CCEvent *) override
    {
      if (!nodeIsVisible(this))
        return false;
      CCPoint const local = this->convertToNodeSpace(touch->getLocation());
      auto const size = this->getContentSize();
      if (local.x < 0.f || local.y < 0.f || local.x > size.width || local.y > size.height)
        return false;
      // The buttons over the area keep their touches, whichever got registered first
      if (this->getParent() && isOnButton(this->getParent(), touch->getLocation()))
        return false;
      m_start = touch->getLocation();
      return true;
    }

    void ccTouchMoved(CCTouch *, CCEvent *) override {}
    void ccTouchCancelled(CCTouch *, CCEvent *) override {}

    void ccTouchEnded(CCTouch *touch, CCEvent *) override
    {
      if (m_onTap && touch->getLocation().getDistance(m_start) < kTapSlop)
        m_onTap(touch->getLocation());
    }

  private:
    int m_priority = 0;
    CCPoint m_start;
    std::function<void(CCPoint)> m_onTap;
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
  if (!Popup::init(layout().width, layout().height))
    return false;

  this->setID("customizer-popup"_spr);
  this->setTitle("Icon Mayhem");

  // Row size, right of the close button: smaller rows show more settings at once
  for (int dir : {-1, 1})
  {
    auto button = CCMenuItemSpriteExtra::create(textButton(dir < 0 ? "-" : "+", 20.f, "GJ_button_04.png", 18.f), this,
                                                menu_selector(CustomizerPopup::onScale));
    button->setTag(dir);
    button->setID(dir < 0 ? "smaller-button" : "bigger-button");
    m_buttonMenu->addChildAtPosition(button, Anchor::TopLeft, {dir < 0 ? 44.f : 122.f, -16.f});
  }
  m_scaleLabel = CCLabelBMFont::create("", "bigFont.fnt");
  m_scaleLabel->setID("scale-label");
  m_mainLayer->addChildAtPosition(m_scaleLabel, Anchor::TopLeft, {83.f, -16.f});
  this->refreshScaleLabel();

  // List first: every menu of the popup has to sit above it to receive touches
  auto listBg = NineSlice::create("square02b_001.png");
  listBg->setColor({0, 0, 0});
  listBg->setOpacity(80);
  listBg->setContentSize(layout().listSize);
  listBg->setAnchorPoint({0.f, 0.f});
  m_mainLayer->addChildAtPosition(listBg, Anchor::BottomLeft, kListOrigin);

  m_list = ScrollLayer::create(layout().listSize);
  m_list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(0.f));
  m_list->setTouchEnabled(true);
  m_mainLayer->addChildAtPosition(m_list, Anchor::BottomLeft, kListOrigin);

  auto scrollbar = Scrollbar::create(m_list);
  m_mainLayer->addChildAtPosition(scrollbar, Anchor::BottomLeft,
                                  {kListOrigin.x + layout().listSize.width + 8.f, kListOrigin.y + layout().listSize.height / 2.f});

  int const buttonPriority = m_list->getTouchPriority() - 1;
  m_buttonMenu->setTouchPriority(buttonPriority);

  this->buildPreview();
  this->buildTabs();

  // Search over all tabs, between the tabs and the list, "Parts" on its left
  auto partsButton = CCMenuItemSpriteExtra::create(textButton("Parts", kPartsWidth, "GJ_button_04.png", 22.f), this,
                                                   menu_selector(CustomizerPopup::onParts));
  partsButton->setID("parts-button");
  m_buttonMenu->addChildAtPosition(partsButton, Anchor::BottomLeft, {kListOrigin.x + kPartsWidth / 2.f, layout().searchY});

  m_search = TextInput::create(layout().searchWidth / .7f, "Search settings");
  m_search->setScale(.7f);
  m_search->setID("search");
  m_search->setCallback([this](std::string const &text)
                        {
                          m_query = text;
                          this->rebuildList(false);
                          this->buildTabs();
                        });
  m_mainLayer->addChildAtPosition(m_search, Anchor::BottomLeft,
                                  {kListOrigin.x + kPartsWidth + 4.f + layout().searchWidth / 2.f, layout().searchY});

  m_onNowSprite = textButton("On now", kOnNowWidth, "GJ_button_04.png", 22.f);
  auto onNow = CCMenuItemSpriteExtra::create(m_onNowSprite, this, menu_selector(CustomizerPopup::onOnNow));
  onNow->setID("on-now-button");
  m_buttonMenu->addChildAtPosition(onNow, Anchor::BottomLeft, {kListOrigin.x + layout().listSize.width - kOnNowWidth / 2.f, layout().searchY});

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

  // On Globed: everybody in the level, or a gift waiting
  bool const gifts = !globedGifts().empty();
  if (gifts || !globedPeers().empty())
  {
    auto players = CCMenuItemSpriteExtra::create(iconButton(gifts ? "gift" : "people", gifts ? CircleBaseColor::Green : CircleBaseColor::Pink,
                                                            24.f, gifts ? "Gift" : "Players"),
                                                 this, menu_selector(CustomizerPopup::onPlayers));
    players->setID("players-button");
    m_buttonMenu->addChildAtPosition(players, Anchor::BottomLeft, {kListOrigin.x + 132.f, 25.f});
  }

  // Hitbox helpers: gray off, cyan on
  m_showHitboxes = Mod::get()->getSavedValue<bool>(kHitboxesSave, false);
  auto hitboxes = CCMenuItemToggler::create(iconButton("hitboxes", CircleBaseColor::Gray, 24.f, "Hitboxes"),
                                            iconButton("hitboxes", CircleBaseColor::Cyan, 24.f, "Hitboxes"), this,
                                            menu_selector(CustomizerPopup::onHitboxes));
  hitboxes->toggle(m_showHitboxes);
  hitboxes->setID("hitboxes-toggle");
  m_buttonMenu->addChildAtPosition(hitboxes, Anchor::BottomLeft, {kListOrigin.x + layout().listSize.width - 50.f, 25.f});

  auto reset = CCMenuItemSpriteExtra::create(iconButton("reset", CircleBaseColor::Gray, 24.f, "Reset"), this,
                                             menu_selector(CustomizerPopup::onReset));
  reset->setID("reset-button");
  m_buttonMenu->addChildAtPosition(reset, Anchor::BottomLeft, {kListOrigin.x + layout().listSize.width - 14.f, 25.f});

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
  m_settingsRevision = settings::revision();

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
  m_mainLayer->addChildAtPosition(panel, Anchor::BottomLeft, layout().previewCenter);

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
  m_panel = panel;

#ifdef GEODE_IS_DESKTOP
  // With a mouse: what a click would open brightens under the cursor, its name in a little tag
  // above it. The shape goes into a texture first so overlapping pieces brighten evenly
  m_hoverShape = FlatShape::create();
  m_hoverTexture = CCRenderTexture::create(static_cast<int>(kPreviewSize.width), static_cast<int>(kPreviewSize.height));
  m_hoverTexture->setPosition(kPreviewSize / 2.f);
  // Its sprite blends as premultiplied (GL_ONE): with plain alpha blending the opacity fades it
  m_hoverTexture->getSprite()->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});
  m_hoverTexture->setVisible(false);
  m_hoverTexture->setID("hover-highlight");
  panel->addChild(m_hoverTexture);

  m_hoverTag = CCNode::create();
  m_hoverTag->setID("hover-tag");
  m_hoverTag->setAnchorPoint({.5f, 0.f});
  m_hoverTag->setVisible(false);
  auto tagBg = NineSlice::create("square02_small.png");
  tagBg->setColor({0, 0, 0});
  tagBg->setOpacity(170);
  tagBg->setID("background");
  m_hoverTag->addChild(tagBg);
  m_hoverLabel = CCLabelBMFont::create("", "bigFont.fnt");
  m_hoverTag->addChild(m_hoverLabel);
  panel->addChild(m_hoverTag, 10);
#endif

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
    sprite->setScale(.28f);
    sprite->setFlipX(dir < 0);
    auto arrow = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(CustomizerPopup::onMode));
    arrow->setTag(dir);
    arrow->setPosition({kPreviewSize.width / 2.f + dir * 55.f, kModeY});
    menu->addChild(arrow);
  }

  m_modeLabel = CCLabelBMFont::create("", "bigFont.fnt");
  m_modeLabel->setScale(.4f);
  m_modeLabel->setPosition({kPreviewSize.width / 2.f, kModeY});
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
  m_mainSprite = textButton("Main", 48.f, "GJ_button_01.png", 16.f);
  m_iconSprite = textButton("Icon", 48.f, "GJ_button_04.png", 16.f);
  for (int i = 0; i < 2; ++i)
  {
    auto segment = CCMenuItemSpriteExtra::create(i == 0 ? m_mainSprite : m_iconSprite, this, menu_selector(CustomizerPopup::onLookSwitch));
    segment->setTag(i);
    segment->setID(i == 0 ? "main-look-button" : "icon-look-button");
    segment->setPosition({kPreviewSize.width / 2.f + (i == 0 ? -26.f : 26.f), kLookSwitchY});
    m_lookSwitch->addChild(segment);
  }

  // Tapping a part of the look opens its settings
  auto tap = TapArea::create(kPreviewSize, m_list->getTouchPriority() - 1, [this](CCPoint world)
                             { this->onPreviewTap(world); });
  tap->setID("tap-area");
  panel->addChild(tap);

  this->updatePreviewIcon();
}

std::string CustomizerPopup::partUnder(CCPoint const &world, HairNode **owner) const
{
  auto const hair = getSimplePlayerHair(m_player);
  for (HairNode *node : {hair.icon, hair.robot, hair.spider})
  {
    if (!node || !nodeIsVisible(node))
      continue;
    if (auto part = node->partAt(world, kPartSlop); !part.empty())
    {
      if (owner)
        *owner = node;
      return part;
    }
  }
  return "";
}

void CustomizerPopup::onPreviewTap(CCPoint const &world)
{
  if (auto part = this->partUnder(world, nullptr); !part.empty())
  {
    this->jumpToGroup(part);
    return;
  }
  Notification::create("Tap a part of the look (hair, a bow, glasses...) to find its settings",
                       NotificationIcon::Info, 1.5f)
      ->show();
}

void CustomizerPopup::updateHover(float dt)
{
  if (!m_hoverTexture || !m_panel)
    return;

  // Over the preview, not over one of its buttons, and no other window on top
  CCPoint const mouse = getMousePos();
  CCPoint const local = m_panel->convertToNodeSpace(mouse);
  auto const size = m_panel->getContentSize();
  bool const inside = local.x >= 0.f && local.y >= 0.f && local.x <= size.width && local.y <= size.height;
  HairNode *owner = nullptr;
  std::string const part = inside && this->isTopmost() && !isOnButton(m_panel, mouse) ? this->partUnder(mouse, &owner) : "";

  if (part.empty() || !owner)
  {
    m_hoverTexture->setVisible(false);
    m_hoverTag->setVisible(false);
    m_hoverTime = 0.f;
    return;
  }

  auto shape = static_cast<FlatShape *>(m_hoverShape.data());
  shape->m_triangles.clear();
  owner->partShape(part, m_panel->worldToNodeTransform(), shape->m_triangles);
  m_hoverTexture->beginWithClear(0.f, 0.f, 0.f, 0.f);
  shape->visit();
  m_hoverTexture->end();

  m_hoverTime += dt;
  float const opacity = kHoverOpacity + kHoverPulse * std::sin(m_hoverTime * kHoverPulseSpeed);
  m_hoverTexture->getSprite()->setOpacity(static_cast<GLubyte>(std::clamp(opacity, 0.f, 255.f)));
  m_hoverTexture->setVisible(true);

  // The name of the block a click opens, in a tag above the cursor, inside the preview
  auto const &groups = customizerGroups();
  auto group = std::find_if(groups.begin(), groups.end(), [&](CustomizerGroup const &group)
                            { return group.id == part; });
  m_hoverLabel->setString(group != groups.end() ? group->name.c_str() : part.c_str());
  m_hoverLabel->limitLabelWidth(size.width - 24.f, .22f, .1f);
  CCSize const tag{m_hoverLabel->getScaledContentWidth() + 10.f, 13.f};
  m_hoverTag->setContentSize(tag);
  if (auto bg = static_cast<CCNode *>(m_hoverTag->getChildByID("background")))
  {
    bg->setContentSize(tag);
    bg->setPosition(tag / 2.f);
  }
  m_hoverLabel->setPosition(tag / 2.f);
  float const half = tag.width / 2.f;
  m_hoverTag->setPosition({std::clamp(local.x, half + 3.f, size.width - half - 3.f),
                           std::min(local.y + 12.f, size.height - tag.height - 3.f)});
  m_hoverTag->setVisible(true);
}

bool CustomizerPopup::isTopmost()
{
  // Another window (Parts, presets, a question) opened over this one
  auto parent = this->getParent();
  if (!parent)
    return false;
  bool above = false;
  for (auto child : CCArrayExt<CCNode *>(parent->getChildren()))
  {
    if (child == this)
      above = true;
    else if (above && child->isVisible() && typeinfo_cast<FLAlertLayer *>(child))
      return false;
  }
  return true;
}

void CustomizerPopup::jumpToGroup(std::string const &id)
{
  auto const &groups = customizerGroups();
  auto group = std::find_if(groups.begin(), groups.end(), [&](CustomizerGroup const &group)
                            { return group.id == id; });
  if (group == groups.end())
    return;

  // Like a tab: the search and "On now" end
  m_query.clear();
  m_search->setString("");
  m_onNow = false;
  m_onNowSprite->updateBGImage("GJ_button_04.png");
  this->setOpen(id, true);
  this->showSection(group->section);
  this->buildTabs();

  auto content = m_list->m_contentLayer;
  auto header = content->getChildByID(fmt::format("{}-header", id));
  if (!header)
    return;

  // The block on top of the list
  float const view = m_list->getContentHeight();
  float const lowest = std::min(view - content->getContentHeight(), 0.f);
  content->setPositionY(std::clamp(view - header->boundingBox().getMaxY(), lowest, 0.f));

  auto flash = CCLayerColor::create({255, 210, 90, 0}, header->getContentWidth(), header->getContentHeight());
  header->addChild(flash, 10);
  flash->runAction(CCSequence::create(CCFadeTo::create(.12f, 120), CCFadeTo::create(.7f, 0), CCRemoveSelf::create(), nullptr));
}

void CustomizerPopup::onPlayers(CCObject *)
{
  if (auto popup = PlayersPopup::create())
    popup->show();
}

void CustomizerPopup::onParts(CCObject *)
{
  if (auto popup = PartsPopup::create([self = Ref(this)](std::string const &id)
                                      { self->jumpToGroup(id); }))
    popup->show();
}

void CustomizerPopup::onScale(CCObject *sender)
{
  // Smaller rows: more settings on the screen at once
  float const step = static_cast<float>(static_cast<CCNode *>(sender)->getTag()) * kRowScaleStep;
  float const scale = std::round(std::clamp(rowScale() + step, kRowScaleMin, kRowScaleMax) / kRowScaleStep) * kRowScaleStep;
  Mod::get()->setSavedValue(kRowScaleSave, scale);
  this->refreshScaleLabel();
  m_rebuildPending = true;
}

void CustomizerPopup::refreshScaleLabel()
{
  if (!m_scaleLabel)
    return;
  m_scaleLabel->setString(fmt::format("Rows {}%", static_cast<int>(std::lround(rowScale() * 100.f))).c_str());
  // A gap on both sides, the buttons don't touch it
  m_scaleLabel->limitLabelWidth(40.f, .3f, .1f);
}

float CustomizerPopup::rowWidth() const
{
  return layout().listSize.width / rowScale();
}

void CustomizerPopup::addToList(CCNode *row)
{
  // Built as wide as the list is at this scale, so it fills the list when scaled
  row->setScale(rowScale());
  m_list->m_contentLayer->addChild(row);
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
  this->updateHover(dt);
  // look.json edited by hand: the rows show the new values
  if (m_settingsRevision != settings::revision())
  {
    m_settingsRevision = settings::revision();
    m_rebuildPending = true;
  }
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
  m_tabMenu->setContentSize({layout().listSize.width, 30.f});
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
                                  {kListOrigin.x + layout().listSize.width / 2.f, layout().tabsY});
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
    row->setContentSize({this->rowWidth(), 40.f});
    auto label = CCLabelBMFont::create("Nothing is on: turn things on in the tabs", "bigFont.fnt");
    label->limitLabelWidth(this->rowWidth() - 20.f, .35f, .1f);
    label->setOpacity(150);
    label->setPosition({this->rowWidth() / 2.f, 20.f});
    row->addChild(label);
    this->addToList(row);
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
  this->addToList(this->createGroupHeader(group, title, open, expandable));

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
  this->addToList(this->createFineRow(group, fine.size(), fineOpen));
  if (fineOpen)
  {
    for (auto key : fine)
      this->addRow(key);
  }
}

CCNode *CustomizerPopup::createGroupHeader(CustomizerGroup const &group, std::string const &title, bool open, bool expandable)
{
  float const width = this->rowWidth();
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
  float const width = this->rowWidth();
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

CCNode *CustomizerPopup::createLookFileRow(float width)
{
  auto row = CCNode::create();
  row->setContentSize({width, 30.f});

  auto label = CCLabelBMFont::create("look.json, edit by hand", "bigFont.fnt");
  label->limitLabelWidth(width - 80.f, .4f, .1f);
  label->setAnchorPoint({0.f, .5f});
  label->setPosition({8.f, 15.f});
  row->addChild(label);

  auto menu = CCMenu::create();
  menu->setPosition({0.f, 0.f});
  menu->setContentSize(row->getContentSize());
  row->addChild(menu);

  auto info = CCMenuItemExt::createSpriteExtraWithFrameName("GJ_infoIcon_001.png", .4f, [](auto)
                                                           {
                                                             FLAlertLayer::create(
                                                                 "look.json",
                                                                 "Your look is kept in <cy>look.json</c> in the config folder of the mod, "
                                                                 "only the values that differ from the defaults. Edit a value in a text editor and "
                                                                 "save the file: the game picks it up in a moment. Delete a line to put it back "
                                                                 "to its default.",
                                                                 "OK")
                                                                 ->show();
                                                           });
  info->setPosition({8.f + label->getScaledContentWidth() + 8.f, 15.f});
  menu->addChild(info);

  auto button = CCMenuItemExt::createSpriteExtra(iconButton("folder", CircleBaseColor::Gray, 24.f), [](auto)
                                                 {
                                                   settings::flush();
                                                   file::openFolder(settings::filePath().parent_path());
                                                 });
  button->setPosition({width - 22.f, 15.f});
  menu->addChild(button);
  return row;
}

void CustomizerPopup::addRow(char const *key)
{
  if (std::string_view(key) == "@looks")
  {
    this->addToList(createLooksRow(this->rowWidth()));
    return;
  }
  if (std::string_view(key) == "@gallery")
  {
    this->addToList(createGalleryRow(this->rowWidth()));
    return;
  }
  if (std::string_view(key) == "@linker")
  {
    this->addToList(createLinkerRow(this->rowWidth()));
    return;
  }
  if (std::string_view(key) == "@emote-keys")
  {
    this->addToList(createEmoteKeysRow(this->rowWidth()));
    return;
  }
  if (std::string_view(key) == "@look-file")
  {
    this->addToList(createLookFileRow(this->rowWidth()));
    return;
  }

  if (auto row = SettingRow::create(key, this->rowWidth()))
  {
    // A color picker opening under it: the list again, at the same place
    row->setOnResize([this]
                     { m_rebuildPending = true; });
    this->addToList(row);
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
  auto matches = [&](char const *key, settings::Def const &def)
  {
    return lower(key).find(needle) != std::string::npos ||
           lower(def.name).find(needle) != std::string::npos ||
           lower(def.description).find(needle) != std::string::npos;
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
      auto def = settings::def(key);
      if (!def)
        continue;
      if (def->type == settings::Type::Title)
      {
        title = def->name;
        titleMatches = matches(key, *def);
        continue;
      }
      if (!titleMatches && !matches(key, *def))
        continue;

      std::string const header = title.empty() ? section.name : fmt::format("{} > {}", section.name, title);
      if (header != lastHeader)
      {
        lastHeader = header;
        auto row = CCNode::create();
        row->setContentSize({this->rowWidth(), 18.f});
        auto label = CCLabelBMFont::create(header.c_str(), "goldFont.fnt");
        label->setScale(.45f);
        label->setAnchorPoint({0.f, .5f});
        label->setPosition({6.f, 9.f});
        row->addChild(label);
        this->addToList(row);
      }
      this->addRow(key);
    }
  }

  if (m_rows.empty())
  {
    auto row = CCNode::create();
    row->setContentSize({this->rowWidth(), 40.f});
    auto label = CCLabelBMFont::create("Nothing found", "bigFont.fnt");
    label->setScale(.4f);
    label->setOpacity(150);
    label->setPosition({this->rowWidth() / 2.f, 20.f});
    row->addChild(label);
    this->addToList(row);
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

  // The main look waits aside, the icon look goes in to be edited: Save keeps it in its preset.
  // Changes not saved last time come back on top of it
  presets::stashMainLook(name);
  presets::load(*preset);
  auto const unsaved = presets::draft(name);
  if (unsaved)
  {
    Preset changes;
    changes.settings = *unsaved;
    presets::apply(changes);
  }
  m_editingIcon = true;
  m_iconLook = name;
  m_undo.clear();
  m_stable = presets::capture("Undo");
  m_seenVersion = HairConfig::version();
  m_rebuildPending = true;
  this->refreshLookSwitch();
  this->refreshLookLabel();
  if (tell)
    Notification::create(fmt::format("Editing the look of this icon, \"{}\"{}", name, unsaved ? ", not saved yet" : ""),
                         NotificationIcon::Info)
        ->show();
  return true;
}

void CustomizerPopup::endIconEdit(std::function<void()> then)
{
  // No question: what isn't saved stays as a draft of the preset (restoreMainLook keeps it), worn
  // in levels too, until Save or loading the preset again
  bool const unsaved = presets::modified();
  std::string const name = m_iconLook;
  presets::restoreMainLook();
  m_editingIcon = false;
  m_iconLook.clear();
  m_undo.clear();
  m_stable = presets::capture("Undo");
  m_seenVersion = HairConfig::version();
  m_rebuildPending = true;
  this->refreshLookSwitch();
  this->refreshLookLabel();
  if (unsaved && presets::draft(name))
    Notification::create(fmt::format("\"{}\" isn't saved yet: try it in a level, Save it in Icon", name), NotificationIcon::Info, 2.5f)->show();
  if (then)
    then();
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
          settings::reset(key);
        self->m_rebuildPending = true;
      });
}
