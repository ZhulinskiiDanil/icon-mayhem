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

  bool globedActive()
  {
    return globed::api::available() && globed::api::game::isActive();
  }

  void send(char const *event, std::string const &payload, std::vector<int32_t> targets = {})
  {
    globed::EventOptions options;
    options.server = globed::EventServer::Game;
    options.targetPlayers = std::move(targets);
    globed::api::net::sendEvent(event, std::vector<uint8_t>(payload.begin(), payload.end()), options);
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
        if (auto look = matjson::parse(payload); look && look.unwrap().isObject())
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
                                 return;
                               globed::api::net::registerEvent(kLookEvent, globed::EventServer::Game);
                               globed::api::net::registerEvent(kEmoteEvent, globed::EventServer::Game);
                               (void)globed::api::net::listenGlobal<globed::msg::EventsMessage>([](globed::msg::EventsMessage &message)
                                                                                               {
                                                                                                 onEvents(message);
                                                                                                 return false;
                                                                                               });
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
    std::set<int> m_greeted;  // players that got our look
    std::string m_sentLook;
    float m_scanTimer = 0.f;
  };

  bool init(GJGameLevel *level, bool useReplay, bool dontCreateObjects)
  {
    s_remoteRigs.clear();
    looks::clearRemotes();
    return PlayLayer::init(level, useReplay, dontCreateObjects);
  }

  void postUpdate(float dt)
  {
    PlayLayer::postUpdate(dt);

    auto fields = m_fields.self();
    fields->m_scanTimer -= dt;
    if (fields->m_scanTimer > 0.f || !globedActive())
      return;
    fields->m_scanTimer = kScanEvery;

    // Our look changed: everybody gets the new one
    auto const &look = currentLookPayload();
    if (look != fields->m_sentLook)
    {
      fields->m_sentLook = look;
      fields->m_greeted.clear();
    }

    // Globed can list us too (its own copy of our icon): our icon already has its rigs
    int const self = GJAccountManager::get()->m_accountID;
    for (int id : globed::api::game::getPlayerIds())
    {
      if (id == self)
        continue;
      auto remote = globed::api::game::getPlayer(id);
      if (!remote || globed::api::player::getAccountId(remote) == self)
        continue;

      // New players get our look
      if (fields->m_greeted.insert(id).second)
        send(kLookEvent, look, {id});

      for (bool second : {false, true})
      {
        auto visual = second ? globed::api::player::getSecond(remote) : globed::api::player::getFirst(remote);
        if (!visual || fields->m_attached.contains(id * 2 + (second ? 1 : 0)))
          continue;

        std::vector<Ref<HairNode>> rigs;
        if (!attachLevelHair(static_cast<PlayerObject *>(visual), second, rigs, [id]
                             { return looks::remoteName(id); }))
          continue;
        fields->m_attached.insert(id * 2 + (second ? 1 : 0));
        for (auto &rig : rigs)
        {
          s_remoteRigs[id].emplace_back(rig.data());
          fields->m_remoteHair.push_back(rig);
        }
      }
    }
  }
};
