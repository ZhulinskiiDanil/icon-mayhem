#include "Globed.hpp"
#include "LevelHair.hpp"
#include "../presets/Looks.hpp"
#include "../presets/Presets.hpp"

#include <dankmeme.globed2/include/globed/soft-link/API.hpp>
#include <dankmeme.globed2/include/globed/core/data/Messages.hpp>
#include <dankmeme.globed2/include/globed/core/game/VisualPlayer.hpp>

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <set>
#include <unordered_map>

using namespace geode::prelude;

// ! --- Globed --- !
// Globed is an optional dependency, reached through its soft-link API: no Globed, no calls.
// Every player sends its look (the settings that differ from the defaults, as JSON) to the others
// in the level when they show up and whenever the look changes. The other players' icons get a rig
// that wears the look they sent; players without the mod send nothing and stay plain.

namespace
{
  constexpr char const *kLookEvent = "zhulis.icon-mayhem/look";
  constexpr char const *kEmoteEvent = "zhulis.icon-mayhem/emote";
  constexpr float kScanEvery = .5f; // s between looking for new players and look changes

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
    log::info("Globed: looks and emotes are shared with other players");
  }

  bool globedActive()
  {
    auto table = globedTable();
    return table && table->game && table->game->isActive();
  }

  void send(char const *event, std::string const &payload, std::vector<int32_t> targets = {})
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
    auto const name = looks::lookFor(false, mode, looks::equippedIcon(mode));
    if (built && name == payloadName && payloadVersion == HairConfig::version())
      return payload;

    auto settings = presets::capture("").settings;
    if (!name.empty())
    {
      if (auto preset = looks::presetSettings(name))
      {
        for (auto const &[key, value] : *preset)
          settings[key] = value;
      }
    }

    auto look = presets::compact(settings);
    look["enabled"] = Mod::get()->getSettingValue<bool>("enabled");
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

      if (name == kLookEvent)
      {
        auto look = matjson::parse(payload);
        if (!look || !look.unwrap().isObject())
        {
          log::warn("Globed: a look from player {} is not valid JSON ({} bytes)", sender, payload.size());
          continue;
        }
        log::info("Globed: got the look of player {} ({} bytes)", sender, payload.size());
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

// ! --- Level --- !

class $modify(GlobedHairPlayLayer, PlayLayer)
{
  struct Fields
  {
    std::vector<Ref<HairNode>> m_remoteHair;
    std::set<int> m_attached; // player id * 2 + second icon
    std::set<int> m_seen;     // players we already sent our look for
    std::string m_sentLook;
    bool m_wasActive = false;
    size_t m_lastCount = 0;
  };

  bool init(GJGameLevel *level, bool useReplay, bool dontCreateObjects)
  {
    s_remoteRigs.clear();
    looks::clearRemotes();
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
        if (!visual || fields->m_attached.contains(key))
          continue;

        // The sender of a look is an account id, Globed lists player ids: wear whichever has a look
        std::vector<Ref<HairNode>> rigs;
        if (!attachLevelHair(static_cast<PlayerObject *>(visual), second, rigs, [id, account]
                             { return looks::hasRemote(account) ? looks::remoteName(account) : looks::remoteName(id); }))
        {
          log::warn("Globed: can't dress player {} (account {}), its icon isn't in the level yet", id, account);
          continue;
        }
        fields->m_attached.insert(key);
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

    // Our look goes to everybody in the level when it changes and when somebody new shows up
    auto const &look = currentLookPayload();
    if (fields->m_seen.empty())
      return;
    if (look != fields->m_sentLook || newPlayer)
    {
      fields->m_sentLook = look;
      send(kLookEvent, look);
      log::info("Globed: sent our look to {} players ({} bytes)", fields->m_seen.size(), look.size());
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
