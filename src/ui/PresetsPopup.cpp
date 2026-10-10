#include "PresetsPopup.hpp"

// More Icons is optional: its functions are reached through Geode events, without linking to it
#define MORE_ICONS_EVENTS
#include <hiimjustin000.more_icons/include/MoreIcons.hpp>

#include "../hair/HairConfig.hpp"
#include "../hooks/SimplePlayerHair.hpp"
#include "../presets/Looks.hpp"
#include "Buttons.hpp"
#include "GalleryPopup.hpp"
#include "Confirm.hpp"
#include "SaveLook.hpp"

#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Scrollbar.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/file.hpp>

#include <algorithm>
#include <unordered_map>

using namespace geode::prelude;

// ! --- Constants --- !

namespace
{
  constexpr std::chrono::milliseconds kDoubleTap{400}; // two taps on a preset this quick load it
  constexpr float kWidth = 440.f;
  constexpr float kHeight = 290.f;

  // The current look bar under the title
  constexpr CCPoint kBarOrigin = {15.f, 224.f};
  constexpr CCSize kBarSize = {410.f, 32.f};

  // Left: tabs, search, the list, buttons to add looks
  constexpr float kTabsY = 205.f;
  constexpr float kSearchY = 182.f;
  constexpr CCPoint kListOrigin = {15.f, 46.f};
  constexpr CCSize kListSize = {205.f, 122.f};
  constexpr float kRowHeight = 24.f;
  constexpr float kAddY = 27.f;

  // Right: the selected preset
  constexpr CCPoint kDetailOrigin = {240.f, 15.f};
  constexpr CCSize kDetailSize = {185.f, 202.f};
  constexpr float kDetailX = kDetailOrigin.x + kDetailSize.width / 2.f;
  constexpr CCPoint kPreviewPosition = {kDetailX, 172.f};
  constexpr float kPreviewScale = 1.25f;

  constexpr ccColor3B kSavedColor = {130, 255, 140};
  constexpr ccColor3B kChangedColor = {255, 205, 80};
  constexpr ccColor3B kQuietColor = {200, 200, 200};


  void notify(std::string const &text, NotificationIcon icon)
  {
    Notification::create(text, icon)->show();
  }

  file::FilePickOptions jsonFiles(std::optional<std::filesystem::path> defaultPath = std::nullopt)
  {
    return file::FilePickOptions{
        .defaultPath = std::move(defaultPath),
        .filters = {file::FilePickOptions::Filter{
            .description = "Icon Mayhem presets",
            .files = {"*.json"},
        }},
    };
  }

  bool samePreset(Preset const &a, Preset const &b)
  {
    return a.name == b.name && a.builtIn == b.builtIn;
  }

  bool isCurrent(Preset const &preset)
  {
    return preset.name == presets::current() && preset.builtIn == presets::currentIsBuiltIn();
  }

  bool matches(std::string const &name, std::string const &query)
  {
    return query.empty() || utils::string::toLower(name).find(utils::string::toLower(query)) != std::string::npos;
  }
}

// ! --- Creation --- !

PresetsPopup *PresetsPopup::create(std::function<void()> onApplied)
{
  auto popup = new PresetsPopup();
  if (popup->initPresets(std::move(onApplied)))
  {
    popup->autorelease();
    return popup;
  }
  delete popup;
  return nullptr;
}

bool PresetsPopup::initPresets(std::function<void()> onApplied)
{
  if (!Popup::init(kWidth, kHeight))
    return false;

  m_onApplied = std::move(onApplied);
  this->setID("presets-popup"_spr);
  this->setTitle("Presets");

  // The list first: every menu has to sit above it to receive touches
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

  m_emptyLabel = CCLabelBMFont::create("", "bigFont.fnt");
  m_emptyLabel->setScale(.35f);
  m_emptyLabel->setOpacity(150);
  m_mainLayer->addChildAtPosition(m_emptyLabel, Anchor::BottomLeft,
                                  {kListOrigin.x + kListSize.width / 2.f, kListOrigin.y + kListSize.height / 2.f});

  int const priority = m_list->getTouchPriority() - 1;
  m_buttonMenu->setTouchPriority(priority);

  m_search = TextInput::create(kListSize.width / .7f, "Search presets");
  m_search->setScale(.7f);
  m_search->setID("search");
  m_search->setCallback([this](std::string const &text)
                        {
                          m_query = text;
                          this->refreshList(false);
                        });
  m_mainLayer->addChildAtPosition(m_search, Anchor::BottomLeft, {kListOrigin.x + kListSize.width / 2.f, kSearchY});

  // Add looks: from nothing, the clipboard, a file
  auto addButton = [&](char const *icon, char const *caption, char const *id, SEL_MenuHandler handler, float x, CircleBaseColor color)
  {
    auto button = CCMenuItemSpriteExtra::create(iconButton(icon, color, 24.f, caption), this, handler);
    button->setID(id);
    m_buttonMenu->addChildAtPosition(button, Anchor::BottomLeft, {x, kAddY});
  };
  // A new empty look, paste from the clipboard, import a file, the folder of the presets
  float const addX = kListOrigin.x + kListSize.width / 2.f;
  addButton("empty", "New empty", "empty-button", menu_selector(PresetsPopup::onEmpty), addX - 68.f, CircleBaseColor::Pink);
  addButton("paste", "Paste", "paste-button", menu_selector(PresetsPopup::onPaste), addX - 34.f, CircleBaseColor::Blue);
  addButton("import", "Import", "import-button", menu_selector(PresetsPopup::onImportFile), addX, CircleBaseColor::Blue);
  addButton("folder", "Folder", "folder-button", menu_selector(PresetsPopup::onFolder), addX + 34.f, CircleBaseColor::Gray);
  // Looks of everyone
  auto galleryButton = CCMenuItemExt::createSpriteExtra(iconButton("globe", CircleBaseColor::Cyan, 24.f, "Gallery"), [](auto)
                                                        {
                                                          if (auto popup = GalleryPopup::create())
                                                            popup->show();
                                                        });
  galleryButton->setID("gallery-button");
  m_buttonMenu->addChildAtPosition(galleryButton, Anchor::BottomLeft, {addX + 68.f, kAddY});

  this->buildCurrentBar();
  this->buildDetail();

  // Open on the preset the look came from
  presets::adoptMatchingPreset();
  std::string const current = presets::current();
  this->reload(current.empty() ? std::nullopt : std::optional(current), presets::currentIsBuiltIn());

  m_seenVersion = HairConfig::version();
  this->refreshCurrent();
  this->scheduleUpdate();
  return true;
}

// ! --- Current look --- !

void PresetsPopup::buildCurrentBar()
{
  auto bar = NineSlice::create("square02b_001.png");
  bar->setColor({0, 0, 0});
  bar->setOpacity(60);
  bar->setContentSize(kBarSize);
  bar->setAnchorPoint({0.f, 0.f});
  m_mainLayer->addChildAtPosition(bar, Anchor::BottomLeft, kBarOrigin);

  auto caption = CCLabelBMFont::create("Your look", "goldFont.fnt");
  caption->setScale(.4f);
  caption->setAnchorPoint({0.f, .5f});
  m_mainLayer->addChildAtPosition(caption, Anchor::BottomLeft, {kBarOrigin.x + 8.f, kBarOrigin.y + 22.f});

  m_currentName = CCLabelBMFont::create("", "bigFont.fnt");
  m_currentName->setAnchorPoint({0.f, .5f});
  m_mainLayer->addChildAtPosition(m_currentName, Anchor::BottomLeft, {kBarOrigin.x + 66.f, kBarOrigin.y + 22.f});

  m_currentStatus = CCLabelBMFont::create("", "bigFont.fnt");
  m_currentStatus->setScale(.28f);
  m_currentStatus->setAnchorPoint({0.f, .5f});
  m_mainLayer->addChildAtPosition(m_currentStatus, Anchor::BottomLeft, {kBarOrigin.x + 8.f, kBarOrigin.y + 9.f});

  float const y = kBarOrigin.y + kBarSize.height / 2.f;
  auto save = CCMenuItemSpriteExtra::create(iconButton("save", CircleBaseColor::Green, 24.f, "Save"), this,
                                            menu_selector(PresetsPopup::onSaveCurrent));
  save->setID("save-button");
  m_buttonMenu->addChildAtPosition(save, Anchor::BottomLeft, {kBarOrigin.x + kBarSize.width - 50.f, y});

  auto saveAs = CCMenuItemSpriteExtra::create(iconButton("save-as", CircleBaseColor::Blue, 24.f, "Save as"), this,
                                              menu_selector(PresetsPopup::onSaveAs));
  saveAs->setID("save-as-button");
  m_buttonMenu->addChildAtPosition(saveAs, Anchor::BottomLeft, {kBarOrigin.x + kBarSize.width - 18.f, y});
}

void PresetsPopup::refreshCurrent()
{
  m_currentName->setString(saveLook::currentName().c_str());
  m_currentName->limitLabelWidth(250.f, .45f, .1f);

  std::string const current = presets::current();
  bool const changed = presets::modified();
  char const *status = nullptr;
  ccColor3B color = kQuietColor;
  if (current.empty())
  {
    status = "Not in a preset yet: Save keeps it";
    color = kChangedColor;
  }
  else if (presets::currentIsBuiltIn())
  {
    status = changed ? "Changed: Save makes it your own copy" : "Built-in look, Save makes your own copy";
    color = changed ? kChangedColor : kQuietColor;
  }
  else
  {
    status = changed ? "Unsaved changes: Save puts them in this preset" : "Saved";
    color = changed ? kChangedColor : kSavedColor;
  }
  m_currentStatus->setString(status);
  m_currentStatus->setColor(color);
  m_currentStatus->limitLabelWidth(320.f, .28f, .1f);
}

void PresetsPopup::update(float dt)
{
  // The look changed (the customizer, Undo, a load): the bar says whether it is saved
  m_sinceRefresh += dt;
  if (m_seenVersion == HairConfig::version() || m_sinceRefresh < .2f)
    return;
  m_sinceRefresh = 0.f;
  m_seenVersion = HairConfig::version();
  this->refreshCurrent();
}

void PresetsPopup::onSaveCurrent(CCObject *)
{
  saveLook::quick([self = Ref(this)]
                  { self->afterSave(); });
}

void PresetsPopup::onSaveAs(CCObject *)
{
  saveLook::asNew([self = Ref(this)]
                  { self->afterSave(); });
}

void PresetsPopup::afterSave()
{
  m_showBuiltIn = false;
  this->reload(presets::current(), false);
  this->refreshCurrent();
}

// ! --- List --- !

void PresetsPopup::buildTabs()
{
  if (!m_tabMenu)
  {
    m_tabMenu = CCMenu::create();
    m_tabMenu->setPosition({0.f, 0.f});
    m_tabMenu->setTouchPriority(m_buttonMenu->getTouchPriority());
    m_mainLayer->addChild(m_tabMenu);
  }
  m_tabMenu->removeAllChildren();

  auto tab = [&](std::string const &text, bool builtIn, float x)
  {
    bool const on = m_showBuiltIn == builtIn;
    auto sprite = textButton(text.c_str(), 96, on ? "GJ_button_01.png" : "GJ_button_04.png", 22.f);
    auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(PresetsPopup::onTab));
    button->setTag(builtIn ? 1 : 0);
    button->setID(builtIn ? "built-in-tab" : "mine-tab");
    button->setPosition({x, kTabsY});
    m_tabMenu->addChild(button);
  };
  tab(fmt::format("Mine ({})", m_mine.size()), false, kListOrigin.x + 50.f);
  tab("Built-in", true, kListOrigin.x + kListSize.width - 50.f);
}

void PresetsPopup::onTab(CCObject *sender)
{
  bool const builtIn = static_cast<CCNode *>(sender)->getTag() == 1;
  if (builtIn == m_showBuiltIn)
    return;
  m_showBuiltIn = builtIn;
  this->buildTabs();
  this->refreshList(false);
}

void PresetsPopup::reload(std::optional<std::string> select, bool builtIn)
{
  // Yours: the latest loaded or saved first, then the rest by name
  m_mine = presets::list();
  std::unordered_map<std::string, size_t> order;
  auto const recent = presets::recent();
  for (size_t i = 0; i < recent.size(); ++i)
    order.emplace(recent[i], i);
  std::stable_sort(m_mine.begin(), m_mine.end(), [&](Preset const &a, Preset const &b)
                   {
                     auto const ia = order.find(a.name);
                     auto const ib = order.find(b.name);
                     size_t const ra = ia == order.end() ? recent.size() : ia->second;
                     size_t const rb = ib == order.end() ? recent.size() : ib->second;
                     return ra < rb;
                   });
  m_builtIn = presets::builtIn();

  // What to show: the preset asked for, the one shown before, or the first one
  std::optional<Preset> chosen;
  auto findIn = [](std::vector<Preset> const &list, std::string const &name) -> std::optional<Preset>
  {
    for (auto const &preset : list)
    {
      if (preset.name == name)
        return preset;
    }
    return std::nullopt;
  };
  if (select)
    chosen = findIn(builtIn ? m_builtIn : m_mine, *select);
  else if (m_selected)
    chosen = findIn(m_selected->builtIn ? m_builtIn : m_mine, m_selected->name);
  if (!chosen)
  {
    if (!m_mine.empty())
      chosen = m_mine.front();
    else if (!m_builtIn.empty())
      chosen = m_builtIn.front();
  }

  m_selected = chosen;
  if (m_selected)
    m_showBuiltIn = m_selected->builtIn;

  this->buildTabs();
  this->refreshList(false);
  this->refreshDetail();
}

void PresetsPopup::refreshList(bool keepScroll)
{
  float const scrolled = m_list->m_contentLayer->getPositionY();

  m_list->m_contentLayer->removeAllChildren();
  auto const &source = m_showBuiltIn ? m_builtIn : m_mine;
  size_t shown = 0;
  for (auto const &preset : source)
  {
    if (!matches(preset.name, m_query))
      continue;
    m_list->m_contentLayer->addChild(this->createRow(preset));
    ++shown;
  }
  m_list->m_contentLayer->updateLayout();
  m_list->scrollToTop();
  if (keepScroll)
  {
    float const lowest = m_list->getContentSize().height - m_list->m_contentLayer->getContentSize().height;
    m_list->m_contentLayer->setPositionY(std::clamp(scrolled, lowest, 0.f));
  }

  if (shown == 0)
  {
    m_emptyLabel->setString(!m_query.empty() ? "Nothing found" : m_showBuiltIn ? "No built-in looks" : "No looks yet: press Save");
    m_emptyLabel->limitLabelWidth(kListSize.width - 20.f, .35f, .1f);
  }
  m_emptyLabel->setVisible(shown == 0);
}

CCNode *PresetsPopup::createRow(Preset const &preset)
{
  auto row = CCNode::create();
  row->setContentSize({kListSize.width, kRowHeight});
  row->setAnchorPoint({.5f, .5f});
  float const centerY = kRowHeight / 2.f;

  bool const selected = m_selected && samePreset(*m_selected, preset);
  bool const current = isCurrent(preset);

  auto highlight = CCLayerColor::create(selected ? ccColor4B{255, 255, 255, 50} : ccColor4B{0, 0, 0, 0}, kListSize.width, kRowHeight);
  row->addChild(highlight);

  auto menu = CCMenu::create();
  menu->setPosition({0.f, 0.f});
  menu->setContentSize(row->getContentSize());
  row->addChild(menu);

  // The whole row selects it
  float const hitLeft = 0.f;
  auto hitArea = CCNode::create();
  hitArea->setContentSize({kListSize.width - hitLeft, kRowHeight});
  auto rowButton = CCMenuItemExt::createSpriteExtra(hitArea, [this, preset](auto)
                                                    { this->tapRow(preset); });
  rowButton->m_scaleMultiplier = 1.f;
  rowButton->setPosition({hitLeft + (kListSize.width - hitLeft) / 2.f, centerY});
  menu->addChild(rowButton);

  auto name = CCLabelBMFont::create(preset.name.c_str(), "bigFont.fnt");
  name->setAnchorPoint({0.f, .5f});
  name->limitLabelWidth(current ? 120.f : 165.f, .4f, .1f);
  name->setPosition({hitLeft + 10.f, centerY});
  if (current)
    name->setColor(kSavedColor);
  row->addChild(name);

  if (current)
  {
    auto mark = CCLabelBMFont::create(presets::modified() ? "on, changed" : "on", "bigFont.fnt");
    mark->setScale(.25f);
    mark->setAnchorPoint({1.f, .5f});
    mark->setColor(kSavedColor);
    mark->setPosition({kListSize.width - 6.f, centerY});
    row->addChild(mark);
  }
  return row;
}

void PresetsPopup::tapRow(Preset const &preset)
{
  auto const now = std::chrono::steady_clock::now();
  bool const twice = m_lastTap && samePreset(*m_lastTap, preset) && now - m_lastTapTime < kDoubleTap;
  m_lastTap = twice ? std::nullopt : std::optional(preset);
  m_lastTapTime = now;
  if (twice)
    this->load(preset);
  else
    this->select(preset);
}

void PresetsPopup::select(Preset const &preset)
{
  m_selected = preset;
  this->refreshList();
  this->refreshDetail();
}

// ! --- Selected preset --- !

void PresetsPopup::buildDetail()
{
  auto panel = NineSlice::create("square02b_001.png");
  panel->setColor({0, 0, 0});
  panel->setOpacity(80);
  panel->setContentSize(kDetailSize);
  panel->setAnchorPoint({0.f, 0.f});
  m_mainLayer->addChildAtPosition(panel, Anchor::BottomLeft, kDetailOrigin);

  // The selected look on your icon, alive
  auto gm = GameManager::get();
  m_preview = SimplePlayer::create(0);
  m_preview->setScale(kPreviewScale);
  m_mainLayer->addChildAtPosition(m_preview, Anchor::BottomLeft, kPreviewPosition);
  m_preview->updatePlayerFrame(gm->activeIconForType(IconType::Cube), IconType::Cube);
  more_icons::updateSimplePlayer(m_preview, IconType::Cube);
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

  m_detailName = CCLabelBMFont::create("", "bigFont.fnt");
  m_mainLayer->addChildAtPosition(m_detailName, Anchor::BottomLeft, {kDetailX, 128.f});

  m_detailTag = CCLabelBMFont::create("", "bigFont.fnt");
  m_detailTag->setScale(.28f);
  m_mainLayer->addChildAtPosition(m_detailTag, Anchor::BottomLeft, {kDetailX, 114.f});

  m_detailHint = CCLabelBMFont::create("Pick a look on the left", "bigFont.fnt");
  m_detailHint->setScale(.3f);
  m_detailHint->setOpacity(150);
  m_mainLayer->addChildAtPosition(m_detailHint, Anchor::BottomLeft, {kDetailX, kDetailOrigin.y + kDetailSize.height / 2.f});

  m_detailMenu = CCMenu::create();
  m_detailMenu->setPosition({0.f, 0.f});
  m_detailMenu->setTouchPriority(m_buttonMenu->getTouchPriority());
  m_mainLayer->addChild(m_detailMenu);
}

void PresetsPopup::refreshDetail()
{
  m_detailMenu->removeAllChildren();
  bool const has = m_selected.has_value();
  m_preview->setVisible(has);
  m_detailName->setVisible(has);
  m_detailTag->setVisible(has);
  m_detailHint->setVisible(!has);
  if (!has)
    return;

  auto const preset = *m_selected;
  m_previewLook = preset.builtIn ? looks::builtInLook(preset.name) : preset.name;

  m_detailName->setString(preset.name.c_str());
  m_detailName->limitLabelWidth(kDetailSize.width - 16.f, .5f, .1f);

  std::string tag = preset.builtIn ? "Built-in" : "Yours";
  if (isCurrent(preset))
    tag += presets::modified() ? ", on now (changed)" : ", on now";
  m_detailTag->setString(tag.c_str());
  m_detailTag->setColor(isCurrent(preset) ? kSavedColor : kQuietColor);
  m_detailTag->limitLabelWidth(kDetailSize.width - 16.f, .28f, .1f);

  auto button = [&](CCNode *sprite, float x, float y, char const *id, std::function<void()> action)
  {
    auto item = CCMenuItemExt::createSpriteExtra(sprite, [action = std::move(action)](auto)
                                                 { action(); });
    item->setID(id);
    item->setPosition({x, y});
    m_detailMenu->addChild(item);
  };

  button(textButton("Load", 150, "GJ_button_01.png", 28.f), kDetailX, 84.f, "load-button", [this, preset]
         { this->load(preset); });

  // One row of tools under Load: save the look here, rename, copy, export, delete (yours only)
  struct Tool
  {
    char const *icon;
    CircleBaseColor color;
    char const *caption;
    char const *id;
    std::function<void()> action;
  };
  std::vector<Tool> tools;
  if (!preset.builtIn)
  {
    tools.push_back({"save", CircleBaseColor::Blue, "Save here", "save-here-button", [this, preset]
                     { this->saveOver(preset); }});
    tools.push_back({"rename", CircleBaseColor::Gray, "Rename", "rename-button", [this, preset]
                     { this->rename(preset); }});
  }
  tools.push_back({"copy", CircleBaseColor::Gray, "Copy", "copy-button", [this, preset]
                   { this->copy(preset); }});
  tools.push_back({"export", CircleBaseColor::Gray, "To file", "file-button", [this, preset]
                   { this->exportFile(preset); }});
  if (!preset.builtIn)
  {
    tools.push_back({"trash", CircleBaseColor::Red, "Delete", "delete-button", [this, preset]
                     { this->remove(preset); }});
  }
  float const step = 35.f;
  float const first = kDetailX - step * static_cast<float>(tools.size() - 1) / 2.f;
  for (size_t i = 0; i < tools.size(); ++i)
    button(iconButton(tools[i].icon, tools[i].color, 24.f, tools[i].caption), first + step * static_cast<float>(i), 52.f, tools[i].id,
           std::move(tools[i].action));
}

// ! --- Preset actions --- !

void PresetsPopup::applied()
{
  if (m_onApplied)
    m_onApplied();
  m_seenVersion = HairConfig::version();
  this->refreshCurrent();
  this->refreshList();
  this->refreshDetail();
}

void PresetsPopup::load(Preset const &preset)
{
  presets::load(preset);
  notify(fmt::format("Loaded \"{}\"", preset.name), NotificationIcon::Success);
  this->applied();
}

void PresetsPopup::saveOver(Preset const &preset)
{
  auto doSave = [self = Ref(this), name = preset.name]
  {
    if (auto result = presets::save(presets::capture(name)); !result)
    {
      notify(fmt::format("Unable to save: {}", result.unwrapErr()), NotificationIcon::Error);
      return;
    }
    presets::markCurrent(name, false);
    notify(fmt::format("Saved \"{}\"", name), NotificationIcon::Success);
    self->reload(name, false);
    self->refreshCurrent();
  };

  // Into the preset the look came from it saves the changes, into another one it replaces it
  bool const own = isCurrent(preset);
  createQuickPopup("Save here",
                   own ? fmt::format("Save your changes into <cy>{}</c>?", preset.name)
                       : fmt::format("Replace <cy>{}</c> with your current look?", preset.name),
                   "Cancel", own ? "Save" : "Replace",
                   [doSave](FLAlertLayer *, bool confirmed)
                   {
                     if (confirmed)
                       doSave();
                   });
}

void PresetsPopup::rename(Preset const &preset)
{
  auto prompt = NamePrompt::create("Rename", preset.name, "Rename", [self = Ref(this), from = preset.name](std::string const &to)
                                   {
                                     if (auto result = presets::rename(from, to); !result)
                                     {
                                       notify(result.unwrapErr(), NotificationIcon::Error);
                                       return;
                                     }
                                     self->reload(to, false);
                                     self->refreshCurrent();
                                   });
  if (prompt)
    prompt->show();
}

void PresetsPopup::copy(Preset const &preset)
{
  confirmOnce("copy", "Copy", "Copies the preset as text to the clipboard: send it to a friend, they add it with Paste",
              [preset]
              {
                if (clipboard::write(presets::toJson(preset).dump(matjson::NO_INDENTATION)))
                  notify("Copied to the clipboard, paste it to share", NotificationIcon::Success);
                else
                  notify("Unable to copy to the clipboard", NotificationIcon::Error);
              });
}

void PresetsPopup::exportFile(Preset const &preset)
{
  async::spawn(file::pick(file::PickMode::SaveFile, jsonFiles(presets::pathOf(preset.name))),
               [preset](file::PickResult result)
               {
                 if (!result)
                 {
                   notify(fmt::format("Unable to export: {}", result.unwrapErr()), NotificationIcon::Error);
                   return;
                 }
                 auto path = std::move(result).unwrap();
                 if (!path)
                   return;

                 if (auto written = presets::writeFile(preset, *path); !written)
                   notify(fmt::format("Unable to export: {}", written.unwrapErr()), NotificationIcon::Error);
                 else
                   notify("Exported", NotificationIcon::Success);
               });
}

void PresetsPopup::remove(Preset const &preset)
{
  createQuickPopup("Delete", fmt::format("Delete the preset <cy>{}</c>?", preset.name), "Cancel", "Delete",
                   [self = Ref(this), name = preset.name](FLAlertLayer *, bool confirmed)
                   {
                     if (!confirmed)
                       return;
                     if (auto result = presets::remove(name); !result)
                       notify(result.unwrapErr(), NotificationIcon::Error);
                     self->m_selected.reset();
                     self->reload();
                     self->refreshCurrent();
                   });
}

void PresetsPopup::onEmpty(CCObject *)
{
  createQuickPopup(
      "Empty look",
      "Start from <cy>nothing</c>? Hair and every accessory and effect go off. Save your current look first to keep it",
      "Cancel", "Empty",
      [self = Ref(this)](FLAlertLayer *, bool confirmed)
      {
        if (!confirmed)
          return;
        presets::applyEmpty();
        notify("Empty look: build yours and save it", NotificationIcon::Success);
        self->applied();
      });
}

// ! --- Import --- !

void PresetsPopup::addImported(Preset preset)
{
  // Never overwrite on import: "Name", "Name 2", "Name 3"...
  preset.name = presets::uniqueName(preset.name);
  preset.builtIn = false;

  if (auto result = presets::save(preset); !result)
  {
    notify(fmt::format("Unable to import: {}", result.unwrapErr()), NotificationIcon::Error);
    return;
  }
  notify(fmt::format("Imported \"{}\"", preset.name), NotificationIcon::Success);
  this->reload(preset.name, false);
}

void PresetsPopup::onPaste(CCObject *)
{
  confirmOnce("paste", "Paste", "Adds a preset someone copied for you (it is in the clipboard) to your presets. "
                                "Your look doesn't change until you load it",
              [self = Ref(this)]
              {
                auto preset = presets::parse(clipboard::read());
                if (!preset)
                {
                  notify(fmt::format("The clipboard has no preset: {}", preset.unwrapErr()), NotificationIcon::Error);
                  return;
                }
                self->addImported(std::move(preset).unwrap());
              });
}

void PresetsPopup::onImportFile(CCObject *)
{
  async::spawn(file::pick(file::PickMode::OpenFile, jsonFiles()),
               [self = Ref(this)](file::PickResult result)
               {
                 if (!result)
                 {
                   notify(fmt::format("Unable to import: {}", result.unwrapErr()), NotificationIcon::Error);
                   return;
                 }
                 auto path = std::move(result).unwrap();
                 if (!path)
                   return;

                 auto preset = presets::readFile(*path);
                 if (!preset)
                 {
                   notify(fmt::format("Unable to import: {}", preset.unwrapErr()), NotificationIcon::Error);
                   return;
                 }
                 self->addImported(std::move(preset).unwrap());
               });
}

void PresetsPopup::onFolder(CCObject *)
{
  (void)file::createDirectoryAll(presets::folder());
  file::openFolder(presets::folder());
}
