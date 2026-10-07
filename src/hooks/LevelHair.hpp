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

bool attachLevelHair(PlayerObject *player, bool playerTwo, std::vector<geode::Ref<HairNode>> &nodes,
                     std::function<std::string()> look = {});
