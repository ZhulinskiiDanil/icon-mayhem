#include "GalleryPopup.hpp"

#include "../hooks/SimplePlayerHair.hpp"
#include "../presets/Looks.hpp"
#include "../presets/Presets.hpp"
#include "Buttons.hpp"
#include "SaveLook.hpp"

#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/Notification.hpp>

using namespace geode::prelude;

namespace
{
  constexpr float kWidth = 470.f;
  constexpr float kHeight = 290.f;

  constexpr float kTopY = 246.f;
  constexpr CCPoint kGridOrigin = {15.f, 40.f};
  constexpr CCSize kGridSize = {306.f, 194.f};
  constexpr int kColumns = 3;
  constexpr int kRows = 2;
  constexpr float kPagerY = 24.f;

  constexpr CCPoint kDetailOrigin = {330.f, 15.f};
  constexpr CCSize kDetailSize = {125.f, 219.f};
  constexpr float kDetailX = kDetailOrigin.x + kDetailSize.width / 2.f;

  constexpr float kSearchDelay = .6f; // s after the last key before the search runs

  void notify(std::string const &text, NotificationIcon icon)
  {
    Notification::create(text, icon)->show();
  }

  // Your own icon, for the previews: a look reads best on the icon you wear
  void dressAsYou(SimplePlayer *player)
  {
    auto gm = GameManager::get();
    player->updatePlayerFrame(gm->getPlayerFrame(), IconType::Cube);
    player->setColor(gm->colorForIdx(gm->getPlayerColor()));
    player->setSecondColor(gm->colorForIdx(gm->getPlayerColor2()));
    if (gm->getPlayerGlow())
      player->setGlowOutline(gm->colorForIdx(gm->getPlayerGlowColor()));
    else
      player->disableGlowOutline();
  }
}

GalleryPopup *GalleryPopup::create()
{
  auto popup = new GalleryPopup();
  if (popup->initGallery())
  {
    popup->autorelease();
    return popup;
  }
  delete popup;
  return nullptr;
}

bool GalleryPopup::initGallery()
{
  if (!Popup::init(kWidth, kHeight))
    return false;

  this->setID("gallery-popup"_spr);
  this->setTitle("Gallery");

  auto gridBg = NineSlice::create("square02b_001.png");
  gridBg->setColor({0, 0, 0});
  gridBg->setOpacity(80);
  gridBg->setContentSize(kGridSize);
  gridBg->setAnchorPoint({0.f, 0.f});
  m_mainLayer->addChildAtPosition(gridBg, Anchor::BottomLeft, kGridOrigin);

  m_grid = CCNode::create();
  m_grid->setContentSize(kGridSize);
  m_mainLayer->addChildAtPosition(m_grid, Anchor::BottomLeft, kGridOrigin);

  m_status = CCLabelBMFont::create("", "bigFont.fnt");
  m_status->setOpacity(170);
  m_mainLayer->addChildAtPosition(m_status, Anchor::BottomLeft,
                                  {kGridOrigin.x + kGridSize.width / 2.f, kGridOrigin.y + kGridSize.height / 2.f});

  // Pages
  float const pagerX = kGridOrigin.x + kGridSize.width / 2.f;
  for (int dir : {-1, 1})
  {
    auto sprite = CCSprite::createWithSpriteFrameName("navArrowBtn_001.png");
    sprite->setScale(.35f);
    sprite->setFlipX(dir < 0);
    auto arrow = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(GalleryPopup::onPage));
    arrow->setTag(dir);
    m_buttonMenu->addChildAtPosition(arrow, Anchor::BottomLeft, {pagerX + dir * 36.f, kPagerY});
  }
  m_pageLabel = CCLabelBMFont::create("", "bigFont.fnt");
  m_mainLayer->addChildAtPosition(m_pageLabel, Anchor::BottomLeft, {pagerX, kPagerY});

  // Search, on the right of the tabs
  m_search = TextInput::create(120.f / .65f, "Search looks");
  m_search->setScale(.65f);
  m_search->setCallback([this](std::string const &text)
                        {
                          m_query = text;
                          m_searchWait = kSearchDelay;
                        });
  m_mainLayer->addChildAtPosition(m_search, Anchor::BottomLeft, {kGridOrigin.x + kGridSize.width - 60.f, kTopY});

  // Share yours, over the detail panel
  auto share = CCMenuItemSpriteExtra::create(textButton("Share yours", kDetailSize.width, "GJ_button_01.png", 24.f), this,
                                             menu_selector(GalleryPopup::onShare));
  share->setID("share-button");
  m_buttonMenu->addChildAtPosition(share, Anchor::BottomLeft, {kDetailX, kTopY});

  this->buildTabs();
  this->buildDetail();
  this->scheduleUpdate();

  if (!gallery::available())
  {
    m_status->setString("The gallery opens soon!");
    m_status->limitLabelWidth(kGridSize.width - 20.f, .4f, .1f);
    return true;
  }
  this->load();
  return true;
}

void GalleryPopup::update(float dt)
{
  // The search runs a moment after the last key, not on every letter
  if (m_searchWait < 0.f)
    return;
  m_searchWait -= dt;
  if (m_searchWait < 0.f)
  {
    m_page = 0;
    this->load();
  }
}

// ! --- Tabs and pages --- !

void GalleryPopup::buildTabs()
{
  if (!m_tabMenu)
  {
    m_tabMenu = CCMenu::create();
    m_tabMenu->setPosition({0.f, 0.f});
    m_mainLayer->addChild(m_tabMenu);
  }
  m_tabMenu->removeAllChildren();

  struct Tab
  {
    char const *sort;
    char const *text;
  };
  constexpr std::array<Tab, 3> kTabs = {Tab{"top", "Top"}, Tab{"new", "New"}, Tab{"mine", "Mine"}};
  for (size_t i = 0; i < kTabs.size(); ++i)
  {
    bool const on = m_sort == kTabs[i].sort;
    auto tab = CCMenuItemExt::createSpriteExtra(textButton(kTabs[i].text, 50.f, on ? "GJ_button_01.png" : "GJ_button_04.png", 22.f),
                                                [this, sort = std::string(kTabs[i].sort)](auto)
                                                {
                                                  if (m_sort == sort)
                                                    return;
                                                  m_sort = sort;
                                                  m_page = 0;
                                                  this->buildTabs();
                                                  this->load();
                                                });
    tab->setPosition({kGridOrigin.x + 26.f + static_cast<float>(i) * 54.f, kTopY});
    m_tabMenu->addChild(tab);
  }
}

void GalleryPopup::onPage(CCObject *sender)
{
  int const dir = static_cast<CCNode *>(sender)->getTag();
  if (m_loading || (dir < 0 && m_page == 0) || (dir > 0 && !m_hasMore))
    return;
  m_page += dir;
  this->load();
}

void GalleryPopup::load()
{
  if (!gallery::available())
    return;
  m_loading = true;
  m_status->setString("Loading...");
  m_status->limitLabelWidth(kGridSize.width - 20.f, .4f, .1f);
  m_status->setVisible(true);

  unsigned const request = ++m_request;
  gallery::list(m_sort, m_query, m_page, [self = Ref(this), request](Result<gallery::Page, std::string> result)
                {
                  // The popup is gone, or another page was asked for meanwhile
                  if (!self->getParent() || request != self->m_request)
                    return;
                  self->m_loading = false;
                  if (!result)
                  {
                    self->m_looks.clear();
                    self->m_status->setString(result.unwrapErr().c_str());
                    self->m_status->limitLabelWidth(kGridSize.width - 20.f, .35f, .1f);
                    self->m_status->setVisible(true);
                    self->showPage();
                    return;
                  }
                  auto page = std::move(result).unwrap();
                  // The previews wear them by name
                  for (auto const &look : page.looks)
                    looks::setRemoteLook(fmt::format("gallery-{}", look.id), look.look);
                  self->m_looks = std::move(page.looks);
                  self->m_hasMore = page.hasMore;
                  self->m_status->setVisible(false);
                  self->m_selected = self->m_looks.empty() ? std::nullopt : std::optional<size_t>(0);
                  self->showPage();
                });
}

// ! --- Cards --- !

std::string GalleryPopup::lookName(gallery::Look const &look) const
{
  // Registered when the page arrives, see load()
  return fmt::format("@remote:gallery-{}", look.id);
}

CCNode *GalleryPopup::createPreview(std::string const &name, float scale)
{
  auto holder = CCNode::create();
  auto player = SimplePlayer::create(1);
  dressAsYou(player);
  player->setScale(scale);
  holder->addChild(player);
  attachSimplePlayerHair(player, PreviewPlace::Presets);
  if (auto hair = getSimplePlayerHair(player).icon)
  {
    hair->setLook([name]
                  { return name; });
  }
  return holder;
}

CCNode *GalleryPopup::createCard(gallery::Look const &look, size_t index)
{
  float const width = kGridSize.width / kColumns - 6.f;
  float const height = kGridSize.height / kRows - 6.f;
  bool const selected = m_selected == index;

  auto card = CCNode::create();
  card->setContentSize({width, height});

  auto bg = NineSlice::create("square02b_001.png");
  bg->setColor(selected ? ccColor3B{255, 255, 255} : ccColor3B{0, 0, 0});
  bg->setOpacity(selected ? 60 : 70);
  bg->setContentSize({width, height});
  bg->setAnchorPoint({0.f, 0.f});
  card->addChild(bg);

  auto preview = this->createPreview(this->lookName(look), .8f);
  preview->setPosition({width / 2.f, height / 2.f + 12.f});
  card->addChild(preview);

  auto name = CCLabelBMFont::create(look.name.c_str(), "bigFont.fnt");
  name->limitLabelWidth(width - 10.f, .32f, .1f);
  name->setPosition({width / 2.f, 20.f});
  card->addChild(name);

  auto by = CCLabelBMFont::create(fmt::format("by {}", look.author).c_str(), "bigFont.fnt");
  by->limitLabelWidth(width - 34.f, .22f, .05f);
  by->setAnchorPoint({0.f, .5f});
  by->setOpacity(190);
  by->setPosition({6.f, 8.f});
  card->addChild(by);

  // The likes in the corner, pink once you liked it
  auto likes = CCLabelBMFont::create(std::to_string(look.likes).c_str(), "bigFont.fnt");
  likes->setScale(.25f);
  likes->setAnchorPoint({1.f, .5f});
  likes->setColor(look.liked ? ccColor3B{255, 140, 180} : ccColor3B{255, 255, 255});
  likes->setPosition({width - 15.f, 8.f});
  card->addChild(likes);
  if (auto heart = CCSprite::create(Mod::get()->expandSpriteName("icon-heart.png").data()))
  {
    heart->setScale(9.f / heart->getContentWidth());
    heart->setPosition({width - 8.f, 8.f});
    card->addChild(heart);
  }
  if (look.hidden)
  {
    auto hidden = CCLabelBMFont::create("hidden", "bigFont.fnt");
    hidden->setScale(.22f);
    hidden->setColor({255, 120, 120});
    hidden->setPosition({width / 2.f, height - 8.f});
    card->addChild(hidden);
  }

  // The whole card selects it
  auto menu = CCMenu::create();
  menu->setPosition({0.f, 0.f});
  menu->setContentSize(card->getContentSize());
  card->addChild(menu);
  auto hitArea = CCNode::create();
  hitArea->setContentSize(card->getContentSize());
  auto button = CCMenuItemExt::createSpriteExtra(hitArea, [this, index](auto)
                                                 {
                                                   m_selected = index;
                                                   this->showPage();
                                                 });
  button->m_scaleMultiplier = 1.f;
  button->setPosition({width / 2.f, height / 2.f});
  menu->addChild(button);
  return card;
}

void GalleryPopup::showPage()
{
  m_grid->removeAllChildren();
  float const cellWidth = kGridSize.width / kColumns;
  float const cellHeight = kGridSize.height / kRows;
  for (size_t i = 0; i < m_looks.size() && i < static_cast<size_t>(kColumns * kRows); ++i)
  {
    auto card = this->createCard(m_looks[i], i);
    int const column = static_cast<int>(i) % kColumns;
    int const row = static_cast<int>(i) / kColumns;
    card->setPosition({cellWidth * column + 3.f, kGridSize.height - cellHeight * (row + 1) + 3.f});
    m_grid->addChild(card);
  }
  if (m_looks.empty() && !m_loading && !m_status->isVisible())
  {
    m_status->setString(m_sort == "mine" ? "You haven't shared a look yet" : !m_query.empty() ? "Nothing found" : "No looks yet: share yours!");
    m_status->limitLabelWidth(kGridSize.width - 20.f, .35f, .1f);
    m_status->setVisible(true);
  }
  m_pageLabel->setString(fmt::format("{}", m_page + 1).c_str());
  m_pageLabel->setScale(.4f);
  this->refreshDetail();
}

// ! --- The selected look --- !

void GalleryPopup::buildDetail()
{
  auto panel = NineSlice::create("square02b_001.png");
  panel->setColor({0, 0, 0});
  panel->setOpacity(80);
  panel->setContentSize(kDetailSize);
  panel->setAnchorPoint({0.f, 0.f});
  m_mainLayer->addChildAtPosition(panel, Anchor::BottomLeft, kDetailOrigin);

  m_detail = CCNode::create();
  m_mainLayer->addChild(m_detail);
  m_detailMenu = CCMenu::create();
  m_detailMenu->setPosition({0.f, 0.f});
  m_mainLayer->addChild(m_detailMenu);
}

void GalleryPopup::refreshDetail()
{
  m_detail->removeAllChildren();
  m_detailMenu->removeAllChildren();
  if (!m_selected || *m_selected >= m_looks.size())
    return;
  auto const look = m_looks[*m_selected];
  size_t const index = *m_selected;

  // The detail lives in m_mainLayer's space: the preview's rig simulates there
  auto preview = this->createPreview(this->lookName(look), 1.15f);
  preview->setPosition({kDetailX, 182.f});
  m_detail->addChild(preview);

  auto name = CCLabelBMFont::create(look.name.c_str(), "bigFont.fnt");
  name->limitLabelWidth(kDetailSize.width - 12.f, .42f, .1f);
  name->setPosition({kDetailX, 140.f});
  m_detail->addChild(name);
  auto by = CCLabelBMFont::create(fmt::format("by {}", look.author).c_str(), "bigFont.fnt");
  by->limitLabelWidth(kDetailSize.width - 12.f, .28f, .1f);
  by->setOpacity(200);
  by->setPosition({kDetailX, 126.f});
  m_detail->addChild(by);

  auto button = [&](CCNode *sprite, float x, float y, std::function<void()> action)
  {
    auto item = CCMenuItemExt::createSpriteExtra(sprite, [action = std::move(action)](auto)
                                                 { action(); });
    item->setPosition({x, y});
    m_detailMenu->addChild(item);
  };
  button(textButton("Wear", 110.f, "GJ_button_01.png", 26.f), kDetailX, 100.f, [this, look]
         { this->wear(look); });

  // Keep it, like it, report it (or delete yours)
  button(iconButton("save", CircleBaseColor::Blue, 24.f, "Keep"), kDetailX - 36.f, 70.f, [this, look]
         { this->keep(look); });
  button(iconButton("heart", look.liked ? CircleBaseColor::Pink : CircleBaseColor::Gray, 24.f, look.liked ? "Liked" : "Like"), kDetailX, 70.f,
         [this, index]
         { this->toggleLike(index); });
  if (look.mine || gallery::isModerator())
  {
    button(iconButton("trash", CircleBaseColor::Red, 24.f, "Delete"), kDetailX + 36.f, 70.f, [this, look]
           { this->remove(look); });
  }
  else
  {
    button(textButton("Report", 40.f, "GJ_button_06.png", 22.f), kDetailX + 38.f, 70.f, [this, look]
           { this->report(look); });
  }

  auto likes = CCLabelBMFont::create(fmt::format("{} {}", look.likes, look.likes == 1 ? "like" : "likes").c_str(), "bigFont.fnt");
  likes->setScale(.3f);
  likes->setColor(look.liked ? ccColor3B{255, 140, 180} : ccColor3B{230, 230, 230});
  likes->setPosition({kDetailX, 40.f});
  m_detail->addChild(likes);
}

// ! --- Actions --- !

void GalleryPopup::keep(gallery::Look const &look)
{
  Preset preset;
  preset.name = presets::uniqueName(look.name);
  preset.settings = presets::withDefaults(look.look);
  if (auto result = presets::save(preset); !result)
  {
    notify(fmt::format("Unable to save: {}", result.unwrapErr()), NotificationIcon::Error);
    return;
  }
  notify(fmt::format("\"{}\" is in your presets", preset.name), NotificationIcon::Success);
}

void GalleryPopup::wear(gallery::Look const &look)
{
  // Kept in the presets, then on: it is the look you edit now, Save keeps your changes in it
  Preset preset;
  preset.name = presets::uniqueName(look.name);
  preset.settings = presets::withDefaults(look.look);
  if (auto result = presets::save(preset); !result)
  {
    notify(fmt::format("Unable to save: {}", result.unwrapErr()), NotificationIcon::Error);
    return;
  }
  presets::load(preset);
  notify(fmt::format("Wearing \"{}\", it's in your presets too", preset.name), NotificationIcon::Success);
}

void GalleryPopup::toggleLike(size_t index)
{
  if (index >= m_looks.size())
    return;
  int const id = m_looks[index].id;
  bool const on = !m_looks[index].liked;
  gallery::like(id, on, [self = Ref(this), id, on](Result<int, std::string> result)
                {
                  if (!result)
                  {
                    notify(result.unwrapErr(), NotificationIcon::Error);
                    return;
                  }
                  for (auto &look : self->m_looks)
                  {
                    if (look.id == id)
                    {
                      look.liked = on;
                      look.likes = result.unwrap();
                    }
                  }
                  if (self->getParent())
                    self->showPage();
                });
}

void GalleryPopup::report(gallery::Look const &look)
{
  createQuickPopup("Report", fmt::format("Report <cy>{}</c> to the moderators? Looks with enough reports hide until they check them", look.name),
                   "Cancel", "Report",
                   [self = Ref(this), id = look.id](FLAlertLayer *, bool confirmed)
                   {
                     if (!confirmed)
                       return;
                     gallery::report(id, [](Result<bool, std::string> result)
                                     {
                                       if (!result)
                                         notify(result.unwrapErr(), NotificationIcon::Error);
                                       else
                                         notify("Reported, thank you", NotificationIcon::Success);
                                     });
                   });
}

void GalleryPopup::remove(gallery::Look const &look)
{
  createQuickPopup("Delete", fmt::format("Delete <cy>{}</c> from the gallery?", look.name), "Cancel", "Delete",
                   [self = Ref(this), id = look.id](FLAlertLayer *, bool confirmed)
                   {
                     if (!confirmed)
                       return;
                     gallery::remove(id, [self](Result<bool, std::string> result)
                                     {
                                       if (!result)
                                       {
                                         notify(result.unwrapErr(), NotificationIcon::Error);
                                         return;
                                       }
                                       notify("Deleted from the gallery", NotificationIcon::Success);
                                       if (self->getParent())
                                         self->load();
                                     });
                   });
}

void GalleryPopup::onShare(CCObject *)
{
  if (!gallery::available())
  {
    notify("The gallery opens soon!", NotificationIcon::Info);
    return;
  }
  // The look you wear: the settings that differ from the defaults, the hair on or off
  std::string const current = presets::current();
  std::string const suggestion = current.empty() ? "My look" : current;
  auto prompt = NamePrompt::create("Share your look", suggestion, "Share", [self = Ref(this)](std::string const &name)
                                   {
                                     auto look = presets::compact(presets::capture("").settings);
                                     look["enabled"] = Mod::get()->getSettingValue<bool>("enabled");
                                     notify("Sharing your look...", NotificationIcon::Loading);
                                     gallery::publish(name, look, [self](Result<gallery::Look, std::string> result)
                                                      {
                                                        if (!result)
                                                        {
                                                          notify(result.unwrapErr(), NotificationIcon::Error);
                                                          return;
                                                        }
                                                        notify(fmt::format("\"{}\" is in the gallery!", result.unwrap().name), NotificationIcon::Success);
                                                        if (!self->getParent())
                                                          return;
                                                        self->m_sort = "mine";
                                                        self->m_page = 0;
                                                        self->buildTabs();
                                                        self->load();
                                                      });
                                   });
  if (prompt)
    prompt->show();
}

// ! --- General tab row --- !

CCNode *createGalleryRow(float width)
{
  auto row = CCNode::create();
  row->setContentSize({width, 30.f});

  auto label = CCLabelBMFont::create("Looks of everyone", "bigFont.fnt");
  label->limitLabelWidth(width - 80.f, .4f, .1f);
  label->setAnchorPoint({0.f, .5f});
  label->setPosition({8.f, 15.f});
  row->addChild(label);

  auto menu = CCMenu::create();
  menu->setPosition({0.f, 0.f});
  menu->setContentSize(row->getContentSize());
  row->addChild(menu);

  auto button = CCMenuItemExt::createSpriteExtra(iconButton("globe", CircleBaseColor::Cyan, 24.f), [](auto)
                                                 {
                                                   if (auto popup = GalleryPopup::create())
                                                     popup->show();
                                                 });
  button->setPosition({width - 22.f, 15.f});
  menu->addChild(button);
  return row;
}
