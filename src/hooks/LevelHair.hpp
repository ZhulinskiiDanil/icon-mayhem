#pragma once

#include "../hair/HairNode.hpp"

#include <Geode/Geode.hpp>
#include <functional>
#include <string>
#include <vector>

// ! --- Level hair --- !
// Attaches the rigs of a player in a level (the icon, the robot and the spider head) and adds them
// to `nodes`. `look` picks what they wear (a look name, see Looks.hpp), by default the player's own
// looks. False while the player isn't in the scene yet.

// The game mode a player is in; the jetpack is the ship of platformer levels
GameMode gameModeOf(PlayerObject *player);

// How hard the moment is for a player, 0..1: faster speed portals and busy clicking
float focusSignal(PlayerObject *player);

// An emote on your own icon in the level (a like from another player: hearts)
void levelHairEmote(HairNode::Emote emote);

// `focusOf` is the player whose moment drives the focus mode (other players' rigs follow yours)
bool attachLevelHair(PlayerObject *player, bool playerTwo, std::vector<geode::Ref<HairNode>> &nodes,
                     std::function<std::string()> look = {}, PlayerObject *focusOf = nullptr);
