#include "Globed.hpp"
#include "LevelHair.hpp"
#include "../presets/Looks.hpp"
#include "../gallery/GalleryApi.hpp"
#include "../icons/CustomIcons.hpp"
#include "../presets/Presets.hpp"
#include "../settings/Settings.hpp"

#include <dankmeme.globed2/include/globed/soft-link/API.hpp>
#include <dankmeme.globed2/include/globed/core/data/Messages.hpp>
#include <dankmeme.globed2/include/globed/core/game/VisualPlayer.hpp>

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Notification.hpp>

#include <chrono>
#include <algorithm>
#include <array>
#include <random>
#include <filesystem>
#include <functional>
#include <optional>
#include <map>
#include <set>
#include <unordered_map>

using namespace geode::prelude;

// ! --- Globed --- !
// Globed is an optional dependency, reached through its soft-link API: no Globed, no calls.
// Every player sends its look (the settings that differ from the defaults, as JSON) to the others
// in the level when they show up and whenever the look changes. The look goes up to the Icon Mayhem
// server and only its hash goes through Globed (its events are small and limited); the others
// fetch it once and keep it. When the server can't be reached the look goes through Globed in
// parts, as before. The other players' icons get a rig that wears the look they sent; players
// without the mod send nothing and stay plain.

namespace
{
  constexpr char const *kLookEvent = "zhulis.icon-mayhem/look";
  constexpr char const *kEmoteEvent = "zhulis.icon-mayhem/emote";
  constexpr char const *kLikeEvent = "zhulis.icon-mayhem/like";
  constexpr char const *kGiftEvent = "zhulis.icon-mayhem/gift";
  constexpr char const *kIconsEvent = "zhulis.icon-mayhem/icons";
  constexpr char const *kLookHashEvent = "zhulis.icon-mayhem/look-hash";
  constexpr char const *kGiftHashEvent = "zhulis.icon-mayhem/gift-hash";
  constexpr char const *kPairEvent = "zhulis.icon-mayhem/pair"; // "ask:#rrggbb", "yes:#rrggbb", "end"
  constexpr float kPairHeartsEvery = 4.f; // s between hearts while the pair is close
  constexpr float kPairClose = 120.f;     // level units: this close the pair sends hearts
  constexpr float kGiftEvery = 60.f; // s between two gifts to the same player
  constexpr float kLikeQuiet = 10.f; // s: likes from the same player closer than this are dropped
  constexpr float kScanEvery = .5f; // s between looking for new players and look changes
  constexpr size_t kCrowd = 6;      // players with us in the level from which their rigs get simpler

  // The Globed server drops events over 1024 bytes and counts every started 512 bytes as one more
  // event: a look goes in parts of at most this many bytes, each with a small header
  constexpr size_t kLookPart = 500;
  constexpr uint8_t kLookFormat = 1; // first header byte; a whole JSON look (v1.5.1) starts with '{'

  // Parts of a look still arriving, by sender (and whether it is a gift: sender * 2 + 1)
  struct PendingLook
  {
    uint8_t id = 0;
    std::vector<std::optional<std::string>> parts;
  };
  std::unordered_map<int, PendingLook> s_pendingLooks;

  // Friends: who we liked in this level, when we last gifted whom, gifts waiting for us
  std::set<int> s_liked;
  std::unordered_map<int, double> s_lastGift;
  std::unordered_map<int, double> s_lastLikeFrom;
  std::vector<GlobedGift> s_gifts;
  int s_giftCount = 0;

  double now()
  {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  // Rigs of the other players in the current level, by their player id
  std::unordered_map<int, std::vector<WeakRef<HairNode>>> s_remoteRigs;

  // Globed's API table, asked for directly. The soft-link wrappers remember the first failure for
  // good, and Globed may not answer yet when this mod loads: ask again until it does
  globed::RootApiTable *globedTable()
  {
    static globed::RootApiTable *table = nullptr;
    static int attempts = 0;
    if (table || !Loader::get()->isModLoaded("dankmeme.globed2"))
      return table;

    using GetTable = geode::Result<globed::RootApiTable *> (*)();
    GetTable getTable = nullptr;
    geode::Dispatch<GetTable *>("dankmeme.globed2/getRootTable").send(&getTable);
    ++attempts;
    if (!getTable)
    {
      if (attempts <= 3)
        log::warn("Globed: its API didn't answer (attempt {})", attempts);
      return nullptr;
    }
    auto result = getTable();
    if (!result)
    {
      log::warn("Globed: its API failed: {}", result.unwrapErr());
      return nullptr;
    }
    table = result.unwrap();
    log::info("Globed: API reached (attempt {})", attempts);
    return table;
  }

  bool s_registered = false;

  // Our events go into the dictionary Globed sends when it joins a level, so before that
  void registerEvents()
  {
    auto table = globedTable();
    if (s_registered || !table || !table->net)
      return;
    s_registered = true;
    table->net->registerEvent(kLookEvent, globed::EventServer::Game);
    table->net->registerEvent(kEmoteEvent, globed::EventServer::Game);
    table->net->registerEvent(kLikeEvent, globed::EventServer::Game);
    table->net->registerEvent(kGiftEvent, globed::EventServer::Game);
    table->net->registerEvent(kIconsEvent, globed::EventServer::Game);
    table->net->registerEvent(kLookHashEvent, globed::EventServer::Game);
    table->net->registerEvent(kGiftHashEvent, globed::EventServer::Game);
    table->net->registerEvent(kPairEvent, globed::EventServer::Game);
    log::info("Globed: looks, emotes, likes and gifts are shared with other players");
  }

  // The name of a player by their player or account id, as events give them
  std::string playerName(int sender)
  {
    auto table = globedTable();
    if (!table || !table->game || !table->player)
      return "A player";
    for (int id : table->game->getPlayerIds())
    {
      auto remote = table->game->getPlayer(id);
      if (remote && (id == sender || table->player->getAccountId(remote) == sender))
        return table->player->getUsername(remote);
    }
    return "A player";
  }

  bool globedActive()
  {
    auto table = globedTable();
    return table && table->game && table->game->isActive();
  }

  void send(char const *event, std::string const &payload, std::vector<int32_t> targets = {});

  // Header: format, look id, part index, part count; then the part of the JSON
  void sendLook(std::string const &look, char const *event = kLookEvent, std::vector<int32_t> targets = {})
  {
    static uint8_t lookId = 0;
    ++lookId;
    size_t const count = std::max<size_t>(1, (look.size() + kLookPart - 1) / kLookPart);
    for (size_t i = 0; i < count; ++i)
    {
      std::string part;
      part += static_cast<char>(kLookFormat);
      part += static_cast<char>(lookId);
      part += static_cast<char>(i);
      part += static_cast<char>(count);
      part += look.substr(i * kLookPart, kLookPart);
      send(event, part, targets);
    }
  }

  // A whole look when all of its parts are here
  std::optional<std::string> receiveLookPart(int sender, std::string const &payload)
  {
    if (!payload.empty() && payload.front() == '{')
      return payload; // one piece, from Icon Mayhem v1.5.1
    if (payload.size() < 4 || static_cast<uint8_t>(payload[0]) != kLookFormat)
      return std::nullopt;

    uint8_t const id = static_cast<uint8_t>(payload[1]);
    size_t const index = static_cast<uint8_t>(payload[2]);
    size_t const count = static_cast<uint8_t>(payload[3]);
    if (count == 0 || index >= count)
      return std::nullopt;

    auto &pending = s_pendingLooks[sender];
    if (pending.id != id || pending.parts.size() != count)
    {
      pending.id = id;
      pending.parts.assign(count, std::nullopt);
    }
    pending.parts[index] = payload.substr(4);

    std::string whole;
    for (auto const &part : pending.parts)
    {
      if (!part)
        return std::nullopt;
      whole += *part;
    }
    s_pendingLooks.erase(sender);
    return whole;
  }

  // ! --- Pairs --- !

  struct Pair
  {
    int id = 0;      // their player id (what events go to)
    int account = 0; // their account id (what events come from)
    std::string name;
    ccColor3B accent;
    bool second = false; // we accepted: our bow sits on the other side
  };
  std::optional<Pair> s_pair;
  std::unordered_map<int, Pair> s_pairAsks; // they asked us, by the id the ask came from
  std::set<int> s_pairAsked;                // we asked them, player and account ids

  constexpr std::array<ccColor3B, 5> kPairAccents{ccColor3B{255, 111, 145}, ccColor3B{120, 190, 255}, ccColor3B{255, 200, 90},
                                                  ccColor3B{150, 95, 220}, ccColor3B{126, 214, 105}};

  // A matching bow (on the other side for the second one) and the accent on every accessory color
  matjson::Value pairOverlay(ccColor3B accent, bool second)
  {
    std::string const hex = "#" + cc3bToHexString(accent);
    auto overlay = matjson::Value::object();
    overlay["head-bow"] = true;
    overlay["bow-color"] = "Custom";
    overlay["bow-custom-color"] = hex;
    overlay["bow-position"] = second ? 35.0 : -35.0;
    for (auto [source, custom] : {std::pair{"tie-color", "tie-custom-color"}, std::pair{"clip-color", "clip-custom-color"},
                                  std::pair{"flower-color", "flower-custom-color"}, std::pair{"headband-color", "headband-custom-color"},
                                  std::pair{"scarf-color", "scarf-custom-color"}, std::pair{"hat-color", "hat-custom-color"},
                                  std::pair{"earring-color", "earring-custom-color"}})
    {
      overlay[source] = "Custom";
      overlay[custom] = hex;
    }
    overlay["streak-color"] = hex;
    overlay["headphones-light"] = hex;
    return overlay;
  }

  std::optional<ccColor3B> parseAccent(std::string_view text)
  {
    if (text.size() != 7 || text.front() != '#')
      return std::nullopt;
    if (auto color = cc3bFromHexString(std::string(text.substr(1)), true))
      return color.unwrap();
    return std::nullopt;
  }

  void startPair(Pair pair)
  {
    looks::setPairOverlay(pairOverlay(pair.accent, pair.second));
    s_pair = std::move(pair);
    s_pairAsks.clear();
    s_pairAsked.clear();
  }

  void clearPair()
  {
    s_pair.reset();
    s_pairAsks.clear();
    s_pairAsked.clear();
    if (looks::pairOverlay().size() > 0)
      looks::setPairOverlay(matjson::Value::object());
  }

  bool isPeer(GlobedPeer const &peer, int sender)
  {
    return sender == peer.id || sender == peer.account;
  }

  // ! --- Looks through the server --- !

  std::unordered_map<std::string, std::string> s_uploaded; // our look (its JSON) -> hash
  std::set<std::string> s_uploading;
  std::set<std::string> s_uploadFailed; // these go through Globed in parts
  std::unordered_map<std::string, matjson::Value> s_fetched; // hash -> look
  std::set<std::string> s_fetching;
  std::set<std::string> s_fetchFailed;
  std::unordered_map<int, std::string> s_wantedLook; // sender -> the hash of the look they wear now

  bool isLookHash(std::string const &text)
  {
    return text.size() == 64 && text.find_first_not_of("0123456789abcdef") == std::string::npos;
  }

  std::filesystem::path lookCacheFile(std::string const &hash)
  {
    return Mod::get()->getSaveDir() / "look-cache" / fmt::format("{}.json", hash);
  }

  // The hash of our look once it is on the server; nothing yet (it goes up now) or when it can't
  // be (`failed`: send it through Globed instead)
  std::optional<std::string> uploadedHash(std::string const &payload, bool &failed)
  {
    failed = s_uploadFailed.contains(payload);
    if (auto known = s_uploaded.find(payload); known != s_uploaded.end())
      return known->second;
    if (failed || s_uploading.contains(payload))
      return std::nullopt;

    auto look = matjson::parse(payload);
    if (!look || !look.unwrap().isObject())
    {
      s_uploadFailed.insert(payload);
      failed = true;
      return std::nullopt;
    }
    s_uploading.insert(payload);
    gallery::uploadWorn(look.unwrap(), [payload](Result<std::string, std::string> result)
                        {
                          s_uploading.erase(payload);
                          if (result && isLookHash(result.unwrap()))
                          {
                            s_uploaded[payload] = result.unwrap();
                            log::info("Globed: our look is on the server ({})", result.unwrap().substr(0, 8));
                          }
                          else
                          {
                            s_uploadFailed.insert(payload);
                            log::warn("Globed: our look can't go to the server ({}), it goes through Globed",
                                      result ? std::string("no hash") : result.unwrapErr());
                          }
                        });
    return std::nullopt;
  }

  // A look by its hash: right away when it is here (memory or disk), else `then` gets it later
  void fetchLook(std::string const &hash, std::function<void(matjson::Value const &)> then)
  {
    if (auto known = s_fetched.find(hash); known != s_fetched.end())
    {
      then(known->second);
      return;
    }
    // Kept on disk: a look never changes under its hash
    if (auto cached = file::readJson(lookCacheFile(hash)); cached && cached.unwrap().isObject())
    {
      s_fetched[hash] = cached.unwrap();
      then(s_fetched[hash]);
      return;
    }
    if (s_fetchFailed.contains(hash) || s_fetching.contains(hash))
      return;

    s_fetching.insert(hash);
    gallery::fetchWorn(hash, [hash, then = std::move(then)](Result<matjson::Value, std::string> result)
                       {
                         s_fetching.erase(hash);
                         if (!result)
                         {
                           log::warn("Globed: the look {} isn't there ({})", hash.substr(0, 8), result.unwrapErr());
                           s_fetchFailed.insert(hash);
                           return;
                         }
                         s_fetched[hash] = result.unwrap();
                         (void)file::createDirectoryAll(lookCacheFile(hash).parent_path());
                         (void)file::writeStringSafe(lookCacheFile(hash), result.unwrap().dump(matjson::NO_INDENTATION));
                         then(s_fetched[hash]);
                       });
  }

  void send(char const *event, std::string const &payload, std::vector<int32_t> targets)
  {
    globed::EventOptions options;
    options.server = globed::EventServer::Game;
    options.targetPlayers = std::move(targets);
    if (auto table = globedTable(); table && table->net)
      table->net->sendEvent(event, std::vector<uint8_t>(payload.begin(), payload.end()), options);
  }

  // The look player 1 wears now: the main look with its preset on top, without the defaults.
  // Built again only when the look or the settings change, the check runs twice a second
  std::string const &currentLookPayload()
  {
    static std::string payload;
    static std::string payloadName;
    static unsigned payloadVersion = 0;
    static bool built = false;

    auto player = PlayLayer::get() ? PlayLayer::get()->m_player1 : nullptr;
    GameMode const mode = player ? gameModeOf(player) : GameMode::Cube;
    auto const name = looks::wornLook(false, mode);
    if (built && name == payloadName && payloadVersion == HairConfig::version())
      return payload;

    auto settings = presets::capture("").settings;
    bool enabled = settings::flag("enabled");
    if (auto remote = name.starts_with("@remote:") ? looks::remoteLook(name) : std::nullopt)
    {
      // A look tried on: the others see it too
      settings = presets::withDefaults(*remote);
      if (auto value = remote->get("enabled"); value && value.unwrap().isBool())
        enabled = value.unwrap().asBool().unwrap();
    }
    else if (!name.empty())
    {
      if (auto preset = looks::presetSettings(name))
      {
        for (auto const &[key, value] : *preset)
          settings[key] = value;
      }
    }

    auto look = presets::compact(settings);
    look["enabled"] = enabled;
    // Which look goes out: the main one, or the preset of the icon or the mode (its saved file)
    log::info("Globed: our look is {}", name.empty() ? "the main look" : fmt::format("the preset \"{}\" (as saved)", name));
    payload = look.dump(matjson::NO_INDENTATION);
    payloadName = name;
    payloadVersion = HairConfig::version();
    built = true;
    return payload;
  }

  void onEvents(globed::msg::EventsMessage &message)
  {
    for (auto const &event : message.events)
    {
      std::string_view const name = event.name;
      int const sender = event.options.sender;
      std::string const payload(event.data.begin(), event.data.end());

      if (name == kIconsEvent)
      {
        // Their custom icons: downloaded by hash, then drawn on their icon
        custom_icons::setPlayerIcons(sender, payload);
      }
      else if (name == kGiftEvent)
      {
        auto whole = receiveLookPart(sender * 2 + 1, payload);
        if (!whole)
          continue;
        auto look = matjson::parse(*whole);
        if (!look || !look.unwrap().isObject())
          continue;
        std::string const who = playerName(sender);
        auto const lookName = looks::setRemoteLook(fmt::format("gift-{}", ++s_giftCount), std::move(look).unwrap());
        s_gifts.push_back(GlobedGift{who, lookName});
        log::info("Globed: {} gifted us their look", who);
        Notification::create(fmt::format("{} gifted you their look! Players, in the customizer", who), NotificationIcon::Success)->show();
      }
      else if (name == kLikeEvent)
      {
        // Hearts on our icon, and who liked it
        double const at = now();
        auto &last = s_lastLikeFrom[sender];
        if (at - last < kLikeQuiet)
          continue;
        last = at;
        levelHairEmote(HairNode::Emote::Heart);
        Notification::create(fmt::format("{} likes your look!", playerName(sender)), NotificationIcon::Success)->show();
      }
      else if (name == kLookHashEvent && isLookHash(payload))
      {
        // Their look by its hash: fetched once, worn when it is here (if they still wear it)
        s_wantedLook[sender] = payload;
        fetchLook(payload, [sender, hash = payload](matjson::Value const &look)
                  {
                    if (s_wantedLook[sender] != hash)
                      return;
                    log::info("Globed: got the look of player {} ({})", sender, hash.substr(0, 8));
                    looks::setRemote(sender, look);
                  });
      }
      else if (name == kGiftHashEvent && isLookHash(payload))
      {
        std::string const who = playerName(sender);
        fetchLook(payload, [who](matjson::Value const &look)
                  {
                    auto const lookName = looks::setRemoteLook(fmt::format("gift-{}", ++s_giftCount), look);
                    s_gifts.push_back(GlobedGift{who, lookName});
                    log::info("Globed: {} gifted us their look", who);
                    Notification::create(fmt::format("{} gifted you their look! Players, in the customizer", who), NotificationIcon::Success)->show();
                  });
      }
      else if (name == kPairEvent)
      {
        std::string_view const text = payload;
        if (text.starts_with("ask:"))
        {
          auto accent = parseAccent(text.substr(4));
          if (!accent || s_pair)
            continue;
          std::string const who = playerName(sender);
          s_pairAsks[sender] = Pair{0, sender, who, *accent, true};
          Notification::create(fmt::format("{} wants to pair up: matching bows and colors! Players, in the customizer", who),
                               NotificationIcon::Info, 3.f)
              ->show();
        }
        else if (text.starts_with("yes:") && s_pairAsked.contains(sender))
        {
          auto accent = parseAccent(text.substr(4));
          if (!accent)
            continue;
          // The player id we asked goes with the account the answer came from
          int id = sender;
          for (auto const &peer : globedPeers())
          {
            if (isPeer(peer, sender))
              id = peer.id;
          }
          std::string const who = playerName(sender);
          startPair(Pair{id, sender, who, *accent, false});
          Notification::create(fmt::format("Paired with {}!", who), NotificationIcon::Success)->show();
        }
        else if (text == "end" && s_pair && (sender == s_pair->account || sender == s_pair->id))
        {
          std::string const who = s_pair->name;
          clearPair();
          Notification::create(fmt::format("{} ended the pair", who), NotificationIcon::Info)->show();
        }
      }
      else if (name == kLookEvent)
      {
        // In parts (their server was out of reach, or an older Icon Mayhem)
        s_wantedLook.erase(sender);
        auto whole = receiveLookPart(sender * 2, payload);
        if (!whole)
          continue;
        auto look = matjson::parse(*whole);
        if (!look || !look.unwrap().isObject())
        {
          log::warn("Globed: a look from player {} is not valid JSON ({} bytes)", sender, whole->size());
          continue;
        }
        log::info("Globed: got the look of player {} ({} bytes)", sender, whole->size());
        log::debug("Globed: the look of player {}: {}", sender, *whole);
        looks::setRemote(sender, std::move(look).unwrap());
      }
      else if (name == kEmoteEvent && payload.size() == 1)
      {
        auto const emote = static_cast<HairNode::Emote>(std::clamp<int>(payload[0], 0, 3));
        for (auto &rig : s_remoteRigs[sender])
        {
          if (auto hair = rig.lock())
            hair->emote(emote);
        }
      }
    }
  }
}

$on_mod(Loaded)
{
  globed::api::waitForGlobed([]
                             {
                               if (!globed::api::isAtLeast("v2.2.0"))
                               {
                                 log::warn("Globed: version 2.2.0 or newer is needed to share looks");
                                 return;
                               }
                               (void)globed::api::net::listenGlobal<globed::msg::EventsMessage>([](globed::msg::EventsMessage &message)
                                                                                               {
                                                                                                 onEvents(message);
                                                                                                 return false;
                                                                                               });
                               registerEvents();
                             });
}

void globedSendEmote(HairNode::Emote emote)
{
  if (!globedActive())
    return;
  send(kEmoteEvent, std::string(1, static_cast<char>(emote)));
}

// ! --- Friends --- !

std::vector<GlobedPeer> globedPeers()
{
  std::vector<GlobedPeer> out;
  auto table = globedTable();
  if (!globedActive() || !table->player)
    return out;

  int const self = GJAccountManager::get()->m_accountID;
  for (int id : table->game->getPlayerIds())
  {
    auto remote = table->game->getPlayer(id);
    if (!remote)
      continue;
    int const account = table->player->getAccountId(remote);
    if (id == self || account == self)
      continue;

    GlobedPeer peer;
    peer.id = id;
    peer.account = account;
    peer.name = table->player->getUsername(remote);
    if (looks::hasRemote(account))
      peer.look = looks::remoteName(account);
    else if (looks::hasRemote(id))
      peer.look = looks::remoteName(id);
    if (auto icons = table->player->getIcons(remote))
    {
      auto const &data = icons.unwrap();
      peer.cube = std::max<int>(1, data.cube);
      peer.color1 = data.color1.asColor();
      peer.color2 = data.color2.asColor();
      peer.glow = !data.glowColor.isNone();
      if (peer.glow)
        peer.glowColor = data.glowColor.asColor();
    }
    peer.liked = s_liked.contains(id);
    out.push_back(std::move(peer));
  }
  return out;
}

bool globedLike(GlobedPeer const &peer)
{
  if (!globedActive() || !s_liked.insert(peer.id).second)
    return false;
  send(kLikeEvent, "", {peer.id});
  // Hearts on their icon here too
  for (int key : {peer.id, peer.account})
  {
    for (auto &rig : s_remoteRigs[key])
    {
      if (auto hair = rig.lock())
        hair->emote(HairNode::Emote::Heart);
    }
  }
  return true;
}

bool globedGift(GlobedPeer const &peer)
{
  double const at = now();
  auto it = s_lastGift.find(peer.id);
  if (!globedActive() || (it != s_lastGift.end() && at - it->second < kGiftEvery))
    return false;
  s_lastGift[peer.id] = at;
  // By its hash when our look is on the server, else in parts
  bool failed = false;
  if (auto hash = uploadedHash(currentLookPayload(), failed))
    send(kGiftHashEvent, *hash, {peer.id});
  else
    sendLook(currentLookPayload(), kGiftEvent, {peer.id});
  log::info("Globed: gifted our look to {}", peer.name);
  return true;
}

std::vector<GlobedGift> &globedGifts()
{
  return s_gifts;
}

bool globedAskPair(GlobedPeer const &peer)
{
  if (!globedActive() || s_pair)
    return false;
  // Our accent: one of a few cute colors, the same for both once they say yes
  std::uniform_int_distribution<size_t> pick(0, kPairAccents.size() - 1);
  static std::mt19937 random(std::random_device{}());
  auto const accent = kPairAccents[pick(random)];
  send(kPairEvent, "ask:#" + cc3bToHexString(accent), {peer.id});
  s_pairAsked.insert(peer.id);
  s_pairAsked.insert(peer.account);
  return true;
}

bool globedAcceptPair(GlobedPeer const &peer)
{
  if (!globedActive())
    return false;
  for (auto const &[sender, ask] : s_pairAsks)
  {
    if (!isPeer(peer, sender))
      continue;
    Pair pair = ask;
    pair.id = peer.id;
    pair.account = peer.account;
    pair.name = peer.name;
    send(kPairEvent, "yes:#" + cc3bToHexString(pair.accent), {peer.id});
    startPair(std::move(pair));
    return true;
  }
  return false;
}

void globedEndPair()
{
  if (s_pair && globedActive())
    send(kPairEvent, "end", {s_pair->id});
  clearPair();
}

bool globedPairedWith(GlobedPeer const &peer)
{
  return s_pair && (s_pair->id == peer.id || s_pair->account == peer.account);
}

bool globedPairAsked(GlobedPeer const &peer)
{
  return std::any_of(s_pairAsks.begin(), s_pairAsks.end(), [&](auto const &ask)
                     { return isPeer(peer, ask.first); });
}

bool globedPairPending(GlobedPeer const &peer)
{
  return s_pairAsked.contains(peer.id) || s_pairAsked.contains(peer.account);
}

// ! --- Custom icons --- !

int globedAccountOf(PlayerObject *player)
{
  auto table = globedTable();
  if (!player || !table || !table->player || !globedActive() || !table->player->isGlobedPlayer(player))
    return 0;
  auto remote = table->player->getRemote(static_cast<globed::VisualPlayer *>(player));
  if (!remote)
    return 0;
  int const account = table->player->getAccountId(remote);
  return account == GJAccountManager::get()->m_accountID ? 0 : account;
}

void globedRestoreIcons(int playerId)
{
  auto table = globedTable();
  if (!globedActive() || !table->player)
    return;
  auto remote = table->game->getPlayer(playerId);
  if (!remote)
    return;
  auto icons = table->player->getIcons(remote);
  if (!icons)
    return;
  auto const &data = icons.unwrap();
  for (bool second : {false, true})
  {
    auto visual = second ? table->player->getSecond(remote) : table->player->getFirst(remote);
    if (!visual)
      continue;
    auto player = static_cast<PlayerObject *>(visual);
    player->updatePlayerFrame(std::max<int>(1, data.cube));
    player->updatePlayerShipFrame(std::max<int>(1, data.ship));
    player->updatePlayerRollFrame(std::max<int>(1, data.ball));
    player->updatePlayerBirdFrame(std::max<int>(1, data.ufo));
    player->updatePlayerDartFrame(std::max<int>(1, data.wave));
    player->updatePlayerSwingFrame(std::max<int>(1, data.swing));
    player->updatePlayerJetpackFrame(std::max<int>(1, data.jetpack));
  }
}

// ! --- Level --- !

class $modify(GlobedHairPlayLayer, PlayLayer)
{
  struct Fields
  {
    std::vector<Ref<HairNode>> m_remoteHair;
    // player id * 2 + second icon -> the icon we dressed: Globed can drop a player for a moment
    // and make a new icon for them, that one gets dressed again
    std::map<int, WeakRef<CCNode>> m_attached;
    std::set<int> m_seen;     // players we already sent our look for
    std::string m_sentLook;
    bool m_lookWaiting = false; // changed, not sent yet (going up to the server)
    float m_pairHearts = 0.f;   // s since the pair last sent hearts
    std::string m_sentIcons;
    std::map<int, int> m_iconModes; // player id * 2 + second icon -> what it showed when last dressed
    bool m_wasActive = false;
    size_t m_lastCount = 0;
  };

  bool init(GJGameLevel *level, bool useReplay, bool dontCreateObjects)
  {
    s_remoteRigs.clear();
    s_pendingLooks.clear();
    s_liked.clear();
    // A new level: the server may be back, what failed is tried again
    HairConfig::setCrowded(false);
    clearPair();
    s_wantedLook.clear();
    s_uploadFailed.clear();
    s_fetchFailed.clear();
    looks::clearRemotes();
    custom_icons::forgetPlayers();
    looks::setTryOn("");
    if (!PlayLayer::init(level, useReplay, dontCreateObjects))
      return false;

    // Late, but before Globed joins this level's session
    registerEvents();
    if (globedTable())
      this->schedule(schedule_selector(GlobedHairPlayLayer::scanPlayers), kScanEvery);
    else if (Loader::get()->isModLoaded("dankmeme.globed2"))
      log::warn("Globed: its API isn't available, other players stay plain");
    return true;
  }

  void scanPlayers(float)
  {
    auto fields = m_fields.self();
    bool const active = globedActive();
    if (active != fields->m_wasActive)
    {
      fields->m_wasActive = active;
      log::info("Globed: {} in this level", active ? "connected" : "not connected");
    }
    if (!active)
      return;

    auto table = globedTable();
    auto const ids = table->game->getPlayerIds();
    if (ids.size() != fields->m_lastCount)
    {
      fields->m_lastCount = ids.size();
      log::info("Globed: {} players in the session", ids.size());
    }

    // Players who left: when they come back they get our look again
    std::erase_if(fields->m_seen, [&](int id)
                  { return std::find(ids.begin(), ids.end(), id) == ids.end(); });

    // Globed can list us too (its own copy of our icon): our icon already has its rigs
    int const self = GJAccountManager::get()->m_accountID;
    bool newPlayer = false;
    for (int id : ids)
    {
      if (id == self)
        continue;
      auto remote = table->game->getPlayer(id);
      if (!remote)
        continue;
      int const account = table->player->getAccountId(remote);
      if (account == self)
        continue;
      newPlayer |= fields->m_seen.insert(id).second;

      for (bool second : {false, true})
      {
        int const key = id * 2 + (second ? 1 : 0);
        auto visual = second ? table->player->getSecond(remote) : table->player->getFirst(remote);
        if (!visual)
          continue;
        if (auto it = fields->m_attached.find(key); it != fields->m_attached.end())
        {
          auto dressed = it->second.lock();
          if (dressed && dressed.data() == static_cast<CCNode *>(visual) && dressed->getParent())
            continue;
          log::info("Globed: player {} has a new icon, dressing it again", id);
          fields->m_attached.erase(it);
        }

        // The sender of a look is an account id, Globed lists player ids: wear whichever has a look
        std::vector<Ref<HairNode>> rigs;
        if (!attachLevelHair(static_cast<PlayerObject *>(visual), second, rigs, [id, account]
                             { return looks::hasRemote(account) ? looks::remoteName(account) : looks::remoteName(id); },
                             m_player1))
        {
          log::warn("Globed: can't dress player {} (account {}), its icon isn't in the level yet", id, account);
          continue;
        }
        fields->m_attached[key] = WeakRef<CCNode>(static_cast<CCNode *>(visual));
        fields->m_iconModes.erase(key);
        log::info("Globed: dressed player {} (account {}, {} icon, {} rigs)", id, account, second ? "second" : "first", rigs.size());
        for (auto &rig : rigs)
        {
          s_remoteRigs[id].emplace_back(rig.data());
          if (account != id)
            s_remoteRigs[account].emplace_back(rig.data());
          fields->m_remoteHair.push_back(rig);
        }
      }
    }

    // Their custom icons: dressed again when one arrives or what their icon shows changes
    bool const dirty = custom_icons::takeDirty();
    for (int id : ids)
    {
      auto remote = table->game->getPlayer(id);
      if (!remote)
        continue;
      int const account = table->player->getAccountId(remote);
      for (bool second : {false, true})
      {
        auto visual = second ? table->player->getSecond(remote) : table->player->getFirst(remote);
        if (!visual)
          continue;
        auto player = static_cast<PlayerObject *>(visual);
        int const mode = player->m_isShip | player->m_isBird << 1 | player->m_isBall << 2 | player->m_isDart << 3 |
                         player->m_isRobot << 4 | player->m_isSpider << 5 | player->m_isSwing << 6;
        int const key = id * 2 + (second ? 1 : 0);
        auto known = fields->m_iconModes.find(key);
        if (!dirty && known != fields->m_iconModes.end() && known->second == mode)
          continue;
        fields->m_iconModes[key] = mode;
        if (account != GJAccountManager::get()->m_accountID)
          custom_icons::apply(player, account);
      }
    }

    // Ours go up once somebody is here to see them; their hashes go out like the look
    if (newPlayer)
      custom_icons::refresh();
    if (!fields->m_seen.empty() && (custom_icons::payload() != fields->m_sentIcons || newPlayer) && !custom_icons::payload().empty())
    {
      fields->m_sentIcons = custom_icons::payload();
      send(kIconsEvent, fields->m_sentIcons);
      log::info("Globed: sent our custom icons ({} bytes)", fields->m_sentIcons.size());
    }

    // A crowd: the other players' rigs get simpler (yours stays)
    HairConfig::setCrowded(fields->m_seen.size() >= kCrowd);

    // The pair: it ends when they leave; close together, hearts float on both now and then
    if (s_pair)
    {
      bool const here = std::find(ids.begin(), ids.end(), s_pair->id) != ids.end();
      if (!here)
      {
        Notification::create(fmt::format("{} left, the pair ended", s_pair->name), NotificationIcon::Info)->show();
        clearPair();
      }
      else
      {
        fields->m_pairHearts += kScanEvery;
        if (fields->m_pairHearts >= kPairHeartsEvery)
        {
          fields->m_pairHearts = 0.f;
          for (int key : {s_pair->id, s_pair->account})
          {
            for (auto &rig : s_remoteRigs[key])
            {
              auto hair = rig.lock();
              auto them = hair ? hair->player() : nullptr;
              if (!them || !m_player1)
                continue;
              CCPoint const a = m_player1->convertToWorldSpace(CCPointZero);
              CCPoint const b = them->convertToWorldSpace(CCPointZero);
              float const scale = std::max(m_objectLayer ? m_objectLayer->getScale() : 1.f, .01f);
              if (a.getDistance(b) / scale < kPairClose)
              {
                hair->emote(HairNode::Emote::Heart);
                levelHairEmote(HairNode::Emote::Heart);
              }
              break;
            }
          }
        }
      }
    }

    // Our look goes to everybody in the level when it changes and when somebody new shows up
    auto const &look = currentLookPayload();
    if (fields->m_seen.empty())
      return;
    if (look != fields->m_sentLook || newPlayer)
    {
      fields->m_sentLook = look;
      fields->m_lookWaiting = true;
    }
    if (!fields->m_lookWaiting)
      return;

    // Its hash once it is on the server (it goes up on the first try, then waits a moment);
    // in parts when the server can't take it
    bool failed = false;
    if (auto hash = uploadedHash(look, failed))
    {
      fields->m_lookWaiting = false;
      send(kLookHashEvent, *hash);
      log::info("Globed: sent our look to {} players ({})", fields->m_seen.size(), hash->substr(0, 8));
    }
    else if (failed)
    {
      fields->m_lookWaiting = false;
      sendLook(look);
      log::info("Globed: sent our look to {} players ({} bytes in {} parts)", fields->m_seen.size(), look.size(),
                (look.size() + kLookPart - 1) / kLookPart);
    }
  }
};

// ! --- Menu --- !
// The API is asked for again once the game is up, so the events are registered before any level

#include <Geode/modify/MenuLayer.hpp>

class $modify(GlobedHairMenuLayer, MenuLayer)
{
  bool init()
  {
    if (!MenuLayer::init())
      return false;
    registerEvents();
    return true;
  }
};
