#include "PlayersPopup.hpp"

#include "../gallery/GalleryApi.hpp"
#include "../hooks/Globed.hpp"
#include "../icons/CustomIcons.hpp"
#include "../hooks/SimplePlayerHair.hpp"
#include "../presets/Looks.hpp"
#include "../presets/Presets.hpp"
#include "Buttons.hpp"

#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Scrollbar.hpp>

using namespace geode::prelude;

namespace
{
  constexpr float kWidth = 400.f;
  constexpr float kHeight = 270.f;
  constexpr CCPoint kListOrigin = {15.f, 18.f};
  constexpr CCSize kListSize = {360.f, 204.f};
  constexpr float kRowHeight = 54.f;
  constexpr float kCheckEvery = 1.f; // s between looking for players who came, left or sent a look

  void notify(std::string const &text, NotificationIcon icon)
  {
    Notification::create(text, icon)->show();
  }
}

PlayersPopup *PlayersPopup::create()
{
  auto popup = new PlayersPopup();
  if (popup->initPlayers())
  {
    popup->autorelease();
    return popup;
  }
  delete popup;
  return nullptr;
}

bool PlayersPopup::initPlayers()
{
  if (!Popup::init(kWidth, kHeight))
    return false;

  this->setID("players-popup"_spr);
  this->setTitle("Players");

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

  this->rebuild();
  this->scheduleUpdate();
  return true;
}

// ! --- List --- !

std::string PlayersPopup::signature() const
{
  std::string out = fmt::format("{}|{}|", globedGifts().size(), looks::tryOn());
  for (auto const &peer : globedPeers())
    out += fmt::format("{}:{}:{};", peer.id, peer.look, peer.liked);
  return out;
}

void PlayersPopup::update(float dt)
{
  // Players come and go and their looks arrive: the list follows
  m_sinceCheck += dt;
  if (m_sinceCheck < kCheckEvery)
    return;
  m_sinceCheck = 0.f;
  if (this->signature() != m_shown)
    this->rebuild();
}

CCNode *PlayersPopup::createPreview(int cube, ccColor3B color1, ccColor3B color2, bool glow, ccColor3B glowColor,
                                   std::string const &look)
{
  // Their icon, alive in the look; the rig's sim space is this holder
  auto holder = CCNode::create();
  holder->setContentSize({50.f, kRowHeight});
  auto player = SimplePlayer::create(cube);
  player->updatePlayerFrame(cube, IconType::Cube);
  player->setColor(color1);
  player->setSecondColor(color2);
  if (glow)
    player->setGlowOutline(glowColor);
  else
    player->disableGlowOutline();
  player->setScale(.8f);
  player->setPosition({25.f, kRowHeight / 2.f - 2.f});
  holder->addChild(player);
  attachSimplePlayerHair(player, PreviewPlace::Presets);
  if (auto hair = getSimplePlayerHair(player).icon)
  {
    hair->setLook([look]
                  { return look; });
  }
  return holder;
}

void PlayersPopup::rebuild()
{
  m_shown = this->signature();
  float const scrolled = m_list->m_contentLayer->getPositionY();
  auto content = m_list->m_contentLayer;
  content->removeAllChildren();

  auto gm = GameManager::get();
  auto row = [&](CCNode *preview, std::string const &title, std::string const &status, ccColor3B statusColor)
  {
    auto node = CCNode::create();
    node->setContentSize({kListSize.width, kRowHeight});
    node->setAnchorPoint({.5f, .5f});
    auto line = CCLayerColor::create({255, 255, 255, 18}, kListSize.width - 12.f, 1.f);
    line->setPosition({6.f, 0.f});
    node->addChild(line);
    preview->setPosition({8.f, 0.f});
    node->addChild(preview);

    auto name = CCLabelBMFont::create(title.c_str(), "bigFont.fnt");
    name->setAnchorPoint({0.f, .5f});
    name->limitLabelWidth(115.f, .45f, .1f);
    name->setPosition({66.f, kRowHeight / 2.f + 10.f});
    node->addChild(name);

    auto state = CCLabelBMFont::create(status.c_str(), "bigFont.fnt");
    state->setAnchorPoint({0.f, .5f});
    state->limitLabelWidth(115.f, .26f, .1f);
    state->setColor(statusColor);
    state->setPosition({66.f, kRowHeight / 2.f - 3.f});
    node->addChild(state);

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    menu->setContentSize(node->getContentSize());
    node->addChild(menu);
    return std::make_pair(node, menu);
  };
  auto button = [&](CCMenu *menu, CCNode *sprite, float x, std::function<void()> action)
  {
    auto item = CCMenuItemExt::createSpriteExtra(sprite, [action = std::move(action)](auto)
                                                 { action(); });
    item->setPosition({x, kRowHeight / 2.f});
    menu->addChild(item);
  };
  float const right = kListSize.width - 22.f;

  // Gifts first: try them on, keep them, or let them go
  auto &gifts = globedGifts();
  for (size_t i = 0; i < gifts.size(); ++i)
  {
    auto const gift = gifts[i];
    auto preview = this->createPreview(gm->getPlayerFrame(), gm->colorForIdx(gm->getPlayerColor()), gm->colorForIdx(gm->getPlayerColor2()),
                                       gm->getPlayerGlow(), gm->colorForIdx(gm->getPlayerGlowColor()), gift.look);
    bool const wearing = looks::tryOn() == gift.look;
    auto [node, menu] = row(preview, fmt::format("Gift from {}", gift.name), wearing ? "On you now, until the level ends" : "Their look, on your icon",
                            {255, 190, 220});
    button(menu, iconButton("trash", CircleBaseColor::Gray, 22.f, "Remove"), right, [this, i]
           {
             auto &list = globedGifts();
             if (i < list.size())
             {
               if (looks::tryOn() == list[i].look)
                 looks::setTryOn("");
               list.erase(list.begin() + static_cast<long>(i));
             }
             this->rebuildSoon();
           });
    button(menu, iconButton("save", CircleBaseColor::Blue, 22.f, "Save"), right - 32.f, [this, gift]
           { this->saveLook(gift.look, gift.name); });
    button(menu, textButton(wearing ? "Take off" : "Try on", 64.f, wearing ? "GJ_button_04.png" : "GJ_button_01.png", 22.f), right - 84.f,
           [this, gift]
           { this->toggleTryOn(gift.look); });
    content->addChild(node);
  }

  for (auto const &peer : globedPeers())
  {
    auto preview = this->createPreview(peer.cube, peer.color1, peer.color2, peer.glow, peer.glowColor, peer.look);
    bool const hasLook = !peer.look.empty();
    bool const wearing = hasLook && looks::tryOn() == peer.look;
    std::string const status = !hasLook ? "No Icon Mayhem look yet" : wearing ? "Their look is on you now" : "Icon Mayhem look";
    auto [node, menu] = row(preview, peer.name, status, hasLook ? ccColor3B{130, 255, 140} : ccColor3B{170, 170, 170});

    // Their custom icon can be reported: it turns plain here right away
    if (auto hashes = custom_icons::hashesOf(peer.account); !hashes.empty())
    {
      auto report = CCMenuItemExt::createSpriteExtra(textButton("Report icon", 52.f, "GJ_button_06.png", 14.f), [this, peer, hashes](auto)
                                                     {
                                                       createQuickPopup("Report icon",
                                                                        fmt::format("Report the custom icon of <cy>{}</c>? You won't see it again, and enough reports hide it for everyone",
                                                                                    peer.name),
                                                                        "Cancel", "Report",
                                                                        [self = Ref(this), peer, hashes](FLAlertLayer *, bool confirmed)
                                                                        {
                                                                          if (!confirmed)
                                                                            return;
                                                                          for (auto const &hash : hashes)
                                                                          {
                                                                            custom_icons::block(hash);
                                                                            gallery::reportIcon(hash, [](Result<bool, std::string> result)
                                                                                                {
                                                                                                  if (!result)
                                                                                                    log::warn("Custom icons: report failed: {}", result.unwrapErr());
                                                                                                });
                                                                          }
                                                                          globedRestoreIcons(peer.id);
                                                                          notify("Reported. Their icon is plain for you now", NotificationIcon::Success);
                                                                          self->rebuildSoon();
                                                                        });
                                                     });
      report->setPosition({92.f, 9.f});
      menu->addChild(report);
    }

    // Gift them yours, like theirs; with a look of their own: keep it, try it on
    button(menu, iconButton("gift", CircleBaseColor::Green, 22.f, "Gift yours"), right, [this, peer]
           {
             if (globedGift(peer))
               notify(fmt::format("Your look is on its way to {}", peer.name), NotificationIcon::Success);
             else
               notify("A gift a minute is enough", NotificationIcon::Info);
           });
    if (hasLook)
    {
      button(menu, iconButton("heart", peer.liked ? CircleBaseColor::Gray : CircleBaseColor::Pink, 22.f, peer.liked ? "Liked" : "Like"), right - 32.f, [this, peer]
             {
               if (globedLike(peer))
                 notify(fmt::format("You liked the look of {}", peer.name), NotificationIcon::Success);
               this->rebuildSoon();
             });
      button(menu, iconButton("save", CircleBaseColor::Blue, 22.f, "Save"), right - 64.f, [this, peer]
             { this->saveLook(peer.look, peer.name); });
      button(menu, textButton(wearing ? "Take off" : "Try on", 64.f, wearing ? "GJ_button_04.png" : "GJ_button_01.png", 22.f), right - 116.f,
             [this, peer]
             { this->toggleTryOn(peer.look); });
    }
    content->addChild(node);
  }

  if (content->getChildrenCount() == 0)
  {
    auto empty = CCNode::create();
    empty->setContentSize({kListSize.width, 60.f});
    auto label = CCLabelBMFont::create("Nobody else is here yet", "bigFont.fnt");
    label->setScale(.4f);
    label->setOpacity(150);
    label->setPosition({kListSize.width / 2.f, 30.f});
    empty->addChild(label);
    content->addChild(empty);
  }

  content->updateLayout();
  m_list->scrollToTop();
  float const lowest = m_list->getContentSize().height - content->getContentSize().height;
  content->setPositionY(std::clamp(scrolled, std::min(lowest, 0.f), 0.f));
}

// ! --- Actions --- !

void PlayersPopup::saveLook(std::string const &look, std::string const &owner)
{
  auto remote = looks::remoteLook(look);
  if (!remote)
  {
    notify("That look isn't here anymore", NotificationIcon::Error);
    return;
  }
  Preset preset;
  preset.name = presets::uniqueName(fmt::format("{}'s look", owner));
  preset.settings = presets::withDefaults(*remote);
  if (auto result = presets::save(preset); !result)
  {
    notify(fmt::format("Unable to save: {}", result.unwrapErr()), NotificationIcon::Error);
    return;
  }
  notify(fmt::format("Saved as \"{}\" in your presets", preset.name), NotificationIcon::Success);
}

void PlayersPopup::toggleTryOn(std::string const &look)
{
  looks::setTryOn(looks::tryOn() == look ? "" : look);
  this->rebuildSoon();
}

void PlayersPopup::rebuildSoon()
{
  // Never inside the touch of a row's own button: on the next frame
  m_shown.clear();
  m_sinceCheck = kCheckEvery;
}
