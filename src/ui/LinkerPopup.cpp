#include "LinkerPopup.hpp"

#include "../hooks/SimplePlayerHair.hpp"
#include "../presets/Looks.hpp"

// More Icons is optional: its functions are reached through Geode events, without linking to it
#define MORE_ICONS_EVENTS
#include <hiimjustin000.more_icons/include/MoreIcons.hpp>

#include "Buttons.hpp"
#include "PresetPicker.hpp"

#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/Notification.hpp>

#include <array>

using namespace geode::prelude;

// ! --- Constants --- !

namespace
{
  constexpr float kWidth = 440.f;
  constexpr float kHeight = 290.f;

  constexpr float kTabsY = 238.f;

  // The grid of icons on the left
  constexpr CCPoint kGridOrigin = {15.f, 44.f};
  constexpr CCSize kGridSize = {262.f, 158.f};
  constexpr float kSourceY = 213.f; // Game | More Icons over the grid
  constexpr int kColumns = 6;
  constexpr int kRows = 4;
  constexpr size_t kPerPage = kColumns * kRows;
  constexpr float kPagerY = 26.f;

  // The selected icon on the right
  constexpr CCPoint kDetailOrigin = {285.f, 15.f};
  constexpr CCSize kDetailSize = {140.f, 205.f};
  constexpr float kDetailX = kDetailOrigin.x + kDetailSize.width / 2.f;

  constexpr ccColor3B kLinkedColor = {130, 255, 140};

  struct ModeInfo
  {
    GameMode mode;
    IconType icon;
    UnlockType unlock;
    char const *name;
  };

  // The modes with a head of their own: ships, UFOs and jetpacks carry the cube and wear its link
  constexpr std::array<ModeInfo, 6> kModes{
      ModeInfo{GameMode::Cube, IconType::Cube, UnlockType::Cube, "Cube"},
      ModeInfo{GameMode::Ball, IconType::Ball, UnlockType::Ball, "Ball"},
      ModeInfo{GameMode::Wave, IconType::Wave, UnlockType::Dart, "Wave"},
      ModeInfo{GameMode::Robot, IconType::Robot, UnlockType::Robot, "Robot"},
      ModeInfo{GameMode::Spider, IconType::Spider, UnlockType::Spider, "Spider"},
      ModeInfo{GameMode::Swing, IconType::Swing, UnlockType::Swing, "Swing"},
  };

  ModeInfo const &infoOf(GameMode mode)
  {
    for (auto const &info : kModes)
    {
      if (info.mode == mode)
        return info;
    }
    return kModes[0];
  }
}

// ! --- Creation --- !

LinkerPopup *LinkerPopup::create(GameMode mode)
{
  auto popup = new LinkerPopup();
  if (popup->initLinker(mode))
  {
    popup->autorelease();
    return popup;
  }
  delete popup;
  return nullptr;
}

bool LinkerPopup::initLinker(GameMode mode)
{
  if (!Popup::init(kWidth, kHeight))
    return false;

  this->setID("linker-popup"_spr);
  this->setTitle("Linker");

  auto hint = CCLabelBMFont::create("A linked icon wears its preset everywhere, over the look of its mode. Ships, UFOs and jetpacks carry your cube with its preset", "chatFont.fnt");
  hint->limitLabelWidth(kWidth - 24.f, .5f, .1f);
  hint->setOpacity(200);
  m_mainLayer->addChildAtPosition(hint, Anchor::Top, {0.f, -38.f});

  auto gridBg = NineSlice::create("square02b_001.png");
  gridBg->setColor({0, 0, 0});
  gridBg->setOpacity(80);
  gridBg->setContentSize(kGridSize);
  gridBg->setAnchorPoint({0.f, 0.f});
  m_mainLayer->addChildAtPosition(gridBg, Anchor::BottomLeft, kGridOrigin);

  m_grid = CCMenu::create();
  m_grid->setPosition({0.f, 0.f});
  m_grid->setContentSize(kGridSize);
  m_grid->setID("icons");
  m_mainLayer->addChildAtPosition(m_grid, Anchor::BottomLeft, kGridOrigin);

  m_emptyLabel = CCLabelBMFont::create("No linked icons here yet", "bigFont.fnt");
  m_emptyLabel->setScale(.35f);
  m_emptyLabel->setOpacity(150);
  m_mainLayer->addChildAtPosition(m_emptyLabel, Anchor::BottomLeft,
                                  {kGridOrigin.x + kGridSize.width / 2.f, kGridOrigin.y + kGridSize.height / 2.f});

  // Under the grid: only linked icons, pages, back to the icon you wear
  auto linkedOnly = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(LinkerPopup::onLinkedOnly), .5f);
  linkedOnly->setID("linked-only-toggle");
  m_buttonMenu->addChildAtPosition(linkedOnly, Anchor::BottomLeft, {kGridOrigin.x + 12.f, kPagerY});
  auto linkedLabel = CCLabelBMFont::create("Linked only", "bigFont.fnt");
  linkedLabel->setScale(.3f);
  linkedLabel->setAnchorPoint({0.f, .5f});
  m_mainLayer->addChildAtPosition(linkedLabel, Anchor::BottomLeft, {kGridOrigin.x + 25.f, kPagerY});

  float const pagerX = kGridOrigin.x + 150.f;
  for (int dir : {-1, 1})
  {
    auto sprite = CCSprite::createWithSpriteFrameName("navArrowBtn_001.png");
    sprite->setScale(.35f);
    sprite->setFlipX(dir < 0);
    auto arrow = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(LinkerPopup::onPage));
    arrow->setTag(dir);
    m_buttonMenu->addChildAtPosition(arrow, Anchor::BottomLeft, {pagerX + dir * 32.f, kPagerY});
  }
  m_pageLabel = CCLabelBMFont::create("", "bigFont.fnt");
  m_mainLayer->addChildAtPosition(m_pageLabel, Anchor::BottomLeft, {pagerX, kPagerY});

  // Back to the icon you wear
  auto mine = CCMenuItemSpriteExtra::create(iconButton("target", CircleBaseColor::Gray, 22.f, "My icon"), this,
                                            menu_selector(LinkerPopup::onMyIcon));
  mine->setID("my-icon-button");
  m_buttonMenu->addChildAtPosition(mine, Anchor::BottomLeft, {kGridOrigin.x + kGridSize.width - 14.f, kPagerY});

  this->buildDetail();
  this->selectMode(mode);
  return true;
}

// ! --- Modes and icons --- !

void LinkerPopup::buildModeTabs()
{
  if (!m_modeMenu)
  {
    m_modeMenu = CCMenu::create();
    m_modeMenu->setPosition({0.f, 0.f});
    m_mainLayer->addChild(m_modeMenu);
  }
  m_modeMenu->removeAllChildren();

  float const width = 60.f;
  float const gap = 2.f;
  float const start = (kWidth - (width * kModes.size() + gap * (kModes.size() - 1))) / 2.f + width / 2.f;
  for (size_t i = 0; i < kModes.size(); ++i)
  {
    bool const on = kModes[i].mode == m_mode;
    auto tab = CCMenuItemExt::createSpriteExtra(textButton(kModes[i].name, width, on ? "GJ_button_01.png" : "GJ_button_04.png", 20.f),
                                                [this, mode = kModes[i].mode](auto)
                                                { this->selectMode(mode); });
    tab->setPosition({start + static_cast<float>(i) * (width + gap), kTabsY});
    m_modeMenu->addChild(tab);
  }
}

void LinkerPopup::buildSourceTabs()
{
  if (!m_sourceMenu)
  {
    m_sourceMenu = CCMenu::create();
    m_sourceMenu->setPosition({0.f, 0.f});
    m_mainLayer->addChild(m_sourceMenu);
  }
  m_sourceMenu->removeAllChildren();

  // Game | More Icons, only when More Icons has icons of this mode
  auto custom = more_icons::getIcons(infoOf(m_mode).icon);
  size_t const count = custom ? custom->size() : 0;
  if (count == 0)
    m_custom = false;

  // Search by name or pack: More Icons has names, the game icons only numbers
  if (!m_searchInput)
  {
    m_searchInput = TextInput::create(96.f / .6f, "Search");
    m_searchInput->setScale(.6f);
    m_searchInput->setCallback([this](std::string const &text)
                               {
                                 m_search = text;
                                 this->collectIcons();
                                 m_page = 0;
                                 this->buildGrid();
                                 this->refreshBrush();
                               });
    m_mainLayer->addChildAtPosition(m_searchInput, Anchor::BottomLeft, {kGridOrigin.x + kGridSize.width - 50.f, kSourceY});
  }
  m_searchInput->setVisible(m_custom);
  if (count == 0)
    return;

  auto tab = [&](std::string const &text, bool isCustom, float x)
  {
    auto sprite = textButton(text.c_str(), 76.f, m_custom == isCustom ? "GJ_button_01.png" : "GJ_button_04.png", 18.f);
    auto button = CCMenuItemExt::createSpriteExtra(sprite, [this, isCustom](auto)
                                                   {
                                                     if (m_custom == isCustom)
                                                       return;
                                                     m_custom = isCustom;
                                                     this->buildSourceTabs();
                                                     this->collectIcons();
                                                     this->showPageOf(m_selected);
                                                   });
    button->setPosition({x, kSourceY});
    m_sourceMenu->addChild(button);
  };
  tab("Game icons", false, kGridOrigin.x + 39.f);
  tab(fmt::format("More Icons ({})", count), true, kGridOrigin.x + 118.f);
}

LinkerPopup::IconRef LinkerPopup::wornIcon() const
{
  return IconRef{looks::equippedIcon(m_mode), looks::equippedCustomIcon(m_mode)};
}

std::string LinkerPopup::linkOf(IconRef const &icon) const
{
  return icon.custom.empty() ? looks::forIcon(m_mode, icon.id) : looks::forCustomIcon(m_mode, icon.custom);
}

void LinkerPopup::setLink(IconRef const &icon, std::string const &preset)
{
  if (icon.custom.empty())
    looks::setForIcon(m_mode, icon.id, preset);
  else
    looks::setForCustomIcon(m_mode, icon.custom, preset);
}

std::string LinkerPopup::nameOf(IconRef const &icon) const
{
  if (icon.custom.empty())
    return fmt::format("{} #{}", infoOf(m_mode).name, icon.id);
  // "pack:name" in More Icons: the short name reads better
  if (auto info = more_icons::getIcon(icon.custom, infoOf(m_mode).icon))
    return info->getShortName();
  return icon.custom;
}

void LinkerPopup::collectIcons()
{
  auto gm = GameManager::get();
  auto const &info = infoOf(m_mode);
  m_icons.clear();

  if (m_custom)
  {
    if (auto custom = more_icons::getIcons(info.icon))
    {
      auto const query = utils::string::toLower(m_search);
      for (auto const &icon : *custom)
      {
        if (!query.empty() && utils::string::toLower(icon.getName()).find(query) == std::string::npos &&
            utils::string::toLower(icon.getPackName()).find(query) == std::string::npos)
          continue;
        IconRef ref{0, icon.getName()};
        if (!m_linkedOnly || !this->linkOf(ref).empty())
          m_icons.push_back(std::move(ref));
      }
    }
    return;
  }

  int const count = gm->countForType(info.icon);
  for (int id = 1; id <= count; ++id)
  {
    IconRef ref{id, ""};
    bool const linked = !this->linkOf(ref).empty();
    if (m_linkedOnly ? linked : gm->isIconUnlocked(id, info.icon) || linked)
      m_icons.push_back(std::move(ref));
  }
}

void LinkerPopup::selectMode(GameMode mode)
{
  // Back to the icon you wear in that mode, in its list. A vehicle opens on its cube
  m_mode = looks::linkModeOf(mode);
  m_selected = this->wornIcon();
  m_custom = !m_selected.custom.empty();
  this->buildModeTabs();
  this->buildSourceTabs();
  this->collectIcons();
  this->showPageOf(m_selected);
}

void LinkerPopup::showPageOf(IconRef const &icon)
{
  auto it = std::find(m_icons.begin(), m_icons.end(), icon);
  m_page = it == m_icons.end() ? 0 : static_cast<size_t>(it - m_icons.begin()) / kPerPage;
  this->buildGrid();
  this->refreshDetail();
}

void LinkerPopup::buildGrid()
{
  m_grid->removeAllChildren();

  size_t const pages = std::max<size_t>(1, (m_icons.size() + kPerPage - 1) / kPerPage);
  m_page = std::min(m_page, pages - 1);
  m_pageLabel->setString(fmt::format("{} / {}", m_page + 1, pages).c_str());
  m_pageLabel->limitLabelWidth(44.f, .35f, .1f);
  m_emptyLabel->setVisible(m_icons.empty());

  auto gm = GameManager::get();
  auto const &info = infoOf(m_mode);
  float const cellWidth = kGridSize.width / kColumns;
  float const cellHeight = kGridSize.height / kRows;
  for (size_t i = 0; i < kPerPage; ++i)
  {
    size_t const index = m_page * kPerPage + i;
    if (index >= m_icons.size())
      break;
    auto const &ref = m_icons[index];
    int const column = static_cast<int>(i) % kColumns;
    int const row = static_cast<int>(i) / kColumns;
    CCPoint const center = {cellWidth * (column + .5f), kGridSize.height - cellHeight * (row + .5f)};

    // The cell: the icon, its preset under it, a light behind the selected one
    auto cell = CCNode::create();
    cell->setContentSize({cellWidth - 2.f, cellHeight - 2.f});
    CCPoint const iconPosition = {cell->getContentWidth() / 2.f, cell->getContentHeight() / 2.f + 5.f};
    if (ref == m_selected)
    {
      auto light = CCLayerColor::create({255, 255, 255, 55}, cellWidth - 2.f, cellHeight - 2.f);
      cell->addChild(light);
    }
    if (ref.custom.empty())
    {
      if (auto icon = GJItemIcon::createBrowserItem(info.unlock, ref.id))
      {
        icon->setScale(.7f);
        icon->setPosition(iconPosition);
        cell->addChild(icon);
      }
    }
    else if (auto custom = more_icons::getIcon(ref.custom, info.icon))
    {
      // A More Icons icon in your colors
      auto icon = SimplePlayer::create(1);
      icon->updatePlayerFrame(1, info.icon);
      more_icons::updateSimplePlayer(icon, custom);
      icon->setColor(gm->colorForIdx(gm->getPlayerColor()));
      icon->setSecondColor(gm->colorForIdx(gm->getPlayerColor2()));
      icon->setScale(.62f);
      icon->setPosition(iconPosition);
      cell->addChild(icon);
    }

    std::string const link = this->linkOf(ref);
    if (!link.empty())
    {
      auto name = CCLabelBMFont::create(link.c_str(), "bigFont.fnt");
      name->limitLabelWidth(cellWidth - 6.f, .22f, .05f);
      name->setColor(kLinkedColor);
      name->setPosition({cell->getContentWidth() / 2.f, 5.f});
      cell->addChild(name);
    }

    auto button = CCMenuItemExt::createSpriteExtra(cell, [this, ref](auto)
                                                   {
                                                     if (!m_brush.empty())
                                                       this->setLink(ref, this->linkOf(ref) == m_brush ? "" : m_brush);
                                                     this->select(ref);
                                                   });
    button->m_scaleMultiplier = 1.05f;
    button->setPosition(center);
    m_grid->addChild(button);
  }
}

void LinkerPopup::select(IconRef const &icon)
{
  m_selected = icon;
  this->buildGrid();
  this->refreshDetail();
}

void LinkerPopup::onPage(CCObject *sender)
{
  size_t const pages = std::max<size_t>(1, (m_icons.size() + kPerPage - 1) / kPerPage);
  int const dir = static_cast<CCNode *>(sender)->getTag();
  int const count = static_cast<int>(pages);
  m_page = static_cast<size_t>((static_cast<int>(m_page) + dir + count) % count);
  this->buildGrid();
}

void LinkerPopup::onLinkedOnly(CCObject *sender)
{
  // The toggler flips after the callback
  m_linkedOnly = !static_cast<CCMenuItemToggler *>(sender)->isToggled();
  this->collectIcons();
  this->showPageOf(m_selected);
}

void LinkerPopup::onMyIcon(CCObject *)
{
  m_selected = this->wornIcon();
  if (m_custom != !m_selected.custom.empty())
  {
    m_custom = !m_selected.custom.empty();
    this->buildSourceTabs();
    this->collectIcons();
  }
  this->showPageOf(m_selected);
}

// ! --- The selected icon --- !

void LinkerPopup::buildDetail()
{
  auto panel = NineSlice::create("square02b_001.png");
  panel->setColor({0, 0, 0});
  panel->setOpacity(80);
  panel->setContentSize(kDetailSize);
  panel->setAnchorPoint({0.f, 0.f});
  m_mainLayer->addChildAtPosition(panel, Anchor::BottomLeft, kDetailOrigin);

  m_iconName = CCLabelBMFont::create("", "bigFont.fnt");
  m_mainLayer->addChildAtPosition(m_iconName, Anchor::BottomLeft, {kDetailX, 128.f});

  auto caption = CCLabelBMFont::create("Linked to", "goldFont.fnt");
  caption->setScale(.4f);
  m_mainLayer->addChildAtPosition(caption, Anchor::BottomLeft, {kDetailX, 110.f});

  // The preset: one tap opens the list with search
  auto pick = CCMenuItemSpriteExtra::create(textButton("Choose", 120.f, "GJ_button_02.png", 22.f), this,
                                            menu_selector(LinkerPopup::onPreset));
  pick->setID("choose-button");
  m_buttonMenu->addChildAtPosition(pick, Anchor::BottomLeft, {kDetailX, 70.f});

  m_linkName = CCLabelBMFont::create("", "bigFont.fnt");
  m_mainLayer->addChildAtPosition(m_linkName, Anchor::BottomLeft, {kDetailX, 93.f});

  // Many at once: the brush links every tapped icon, Link all what is shown
  m_brushSprite = textButton("Brush: off", 120.f, "GJ_button_04.png", 20.f);
  auto brush = CCMenuItemSpriteExtra::create(m_brushSprite, this, menu_selector(LinkerPopup::onBrush));
  brush->setID("brush-button");
  m_buttonMenu->addChildAtPosition(brush, Anchor::BottomLeft, {kDetailX, 44.f});

  m_linkAll = CCMenuItemSpriteExtra::create(textButton("Link all shown", 120.f, "GJ_button_01.png", 18.f), this,
                                            menu_selector(LinkerPopup::onLinkAll));
  m_linkAll->setID("link-all-button");
  m_buttonMenu->addChildAtPosition(m_linkAll, Anchor::BottomLeft, {kDetailX, 24.f});

  m_fallback = CCLabelBMFont::create("", "chatFont.fnt");
  m_fallback->setScale(.45f);
  m_fallback->setOpacity(190);
  m_mainLayer->addChildAtPosition(m_fallback, Anchor::BottomLeft, {kDetailX, 24.f});
}

void LinkerPopup::rebuildPreview()
{
  // A fresh icon each time: a More Icons icon stays on a preview it was put on
  if (m_preview)
    m_preview->removeFromParent();

  auto gm = GameManager::get();
  auto const &info = infoOf(m_mode);
  m_preview = SimplePlayer::create(1);
  m_preview->setScale(1.1f);
  m_mainLayer->addChildAtPosition(m_preview, Anchor::BottomLeft, {kDetailX, 172.f});
  m_preview->updatePlayerFrame(m_selected.custom.empty() ? m_selected.id : 1, info.icon);
  if (!m_selected.custom.empty())
  {
    if (auto custom = more_icons::getIcon(m_selected.custom, info.icon))
      more_icons::updateSimplePlayer(m_preview, custom);
  }
  m_preview->setColor(gm->colorForIdx(gm->getPlayerColor()));
  m_preview->setSecondColor(gm->colorForIdx(gm->getPlayerColor2()));
  if (gm->getPlayerGlow())
    m_preview->setGlowOutline(gm->colorForIdx(gm->getPlayerGlowColor()));
  else
    m_preview->disableGlowOutline();

  attachSimplePlayerHair(m_preview, PreviewPlace::Presets);
  if (auto hair = getSimplePlayerHair(m_preview).icon)
  {
    hair->setLook([this]
                  { return m_previewLook; });
  }
}

void LinkerPopup::refreshDetail()
{
  m_previewLook = looks::lookFor(false, m_mode, m_selected.id, m_selected.custom);
  this->rebuildPreview();

  bool const worn = m_selected == this->wornIcon();
  m_iconName->setString(fmt::format("{}{}", this->nameOf(m_selected), worn ? " (yours)" : "").c_str());
  m_iconName->limitLabelWidth(kDetailSize.width - 12.f, .4f, .1f);

  std::string const link = this->linkOf(m_selected);
  m_linkName->setString(link.empty() ? "Nothing" : link.c_str());
  m_linkName->setColor(link.empty() ? ccColor3B{170, 170, 170} : kLinkedColor);
  m_linkName->limitLabelWidth(kDetailSize.width - 12.f, .45f, .1f);

  // What it wears without a link
  std::string const modeLook = looks::forMode(m_mode, false);
  m_fallback->setString(link.empty() ? fmt::format("Wears {} now", modeLook.empty() ? "the main look" : modeLook).c_str()
                                     : fmt::format("Without it: {}", modeLook.empty() ? "the main look" : modeLook).c_str());
  m_fallback->limitLabelWidth(kDetailSize.width - 10.f, .45f, .1f);
  this->refreshBrush();
}

void LinkerPopup::refreshBrush()
{
  bool const on = !m_brush.empty();
  m_brushSprite->setString(on ? fmt::format("Brush: {}", m_brush).c_str() : "Brush: off");
  m_brushSprite->updateBGImage(on ? "GJ_button_01.png" : "GJ_button_04.png");
  // With the brush on, the line under it links everything shown instead of telling the fallback
  m_linkAll->setVisible(on);
  m_fallback->setVisible(!on);
}

void LinkerPopup::onBrush(CCObject *)
{
  // A second tap puts the brush away
  if (!m_brush.empty())
  {
    m_brush.clear();
    this->refreshBrush();
    return;
  }
  auto picker = PresetPicker::create("Brush: tap icons to link", "", "", [self = Ref(this)](std::string const &name)
                                     {
                                       self->m_brush = name;
                                       self->refreshBrush();
                                       Notification::create(fmt::format("Tap icons to link them to \"{}\", tap again to unlink", name),
                                                            NotificationIcon::Info)
                                           ->show();
                                     });
  if (picker)
    picker->show();
}

void LinkerPopup::onLinkAll(CCObject *)
{
  if (m_brush.empty() || m_icons.empty())
    return;
  createQuickPopup("Link all", fmt::format("Link all <cy>{}</c> shown icons to <cg>{}</c>?", m_icons.size(), m_brush), "Cancel", "Link",
                   [self = Ref(this)](FLAlertLayer *, bool confirmed)
                   {
                     if (!confirmed)
                       return;
                     for (auto const &icon : self->m_icons)
                       self->setLink(icon, self->m_brush);
                     Notification::create(fmt::format("Linked {} icons", self->m_icons.size()), NotificationIcon::Success)->show();
                     self->buildGrid();
                     self->refreshDetail();
                   });
}

void LinkerPopup::onPreset(CCObject *)
{
  auto const icon = m_selected;
  auto picker = PresetPicker::create(fmt::format("Link {}", this->nameOf(icon)), this->linkOf(icon), "Nothing (unlink)",
                                     [self = Ref(this), icon](std::string const &name)
                                     {
                                       self->setLink(icon, name);
                                       self->collectIcons();
                                       self->buildGrid();
                                       self->refreshDetail();
                                     });
  if (picker)
    picker->show();
}


// ! --- General tab row --- !

CCNode *createLinkerRow(float width)
{
  auto row = CCNode::create();
  row->setContentSize({width, 30.f});

  auto label = CCLabelBMFont::create("Presets linked to icons", "bigFont.fnt");
  label->limitLabelWidth(width - 80.f, .4f, .1f);
  label->setAnchorPoint({0.f, .5f});
  label->setPosition({8.f, 15.f});
  row->addChild(label);

  auto menu = CCMenu::create();
  menu->setPosition({0.f, 0.f});
  menu->setContentSize(row->getContentSize());
  row->addChild(menu);

  auto button = CCMenuItemExt::createSpriteExtra(iconButton("link", CircleBaseColor::Pink, 24.f), [](auto)
                                                 {
                                                   if (auto popup = LinkerPopup::create())
                                                     popup->show();
                                                 });
  button->setPosition({width - 22.f, 15.f});
  menu->addChild(button);
  return row;
}
