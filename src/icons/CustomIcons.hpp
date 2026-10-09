#pragma once

#include <Geode/Geode.hpp>

#include <string>
#include <vector>

// ! --- Custom icons --- !
// Players see each other's More Icons icons on Globed, even without More Icons themselves.
// Ours: the icons we wear go up to the gallery server once (the picture and its frames); Globed
// carries only their hashes. Theirs: downloaded by hash (and kept on disk), then drawn on their
// icon in place of the game's frames, the way More Icons does it.

namespace custom_icons
{
  // ! --- Ours --- !

  // The text sent on Globed, "cube=<hash>;ship=<hash>": the icons that are up. "" when none (or
  // sharing is off)
  std::string const &payload();
  // Looks at the icons we wear and uploads the new ones; payload() grows as they get up
  void refresh();

  // ! --- Theirs --- !

  // What a player showed (their payload), by their account
  void setPlayerIcons(int account, std::string const &payload);
  void forgetPlayers();
  // Dresses a player's icon (a Globed copy) in their custom icons of what it shows now
  void apply(PlayerObject *player, int account);
  // What a player shows, for the Report button; a blocked hash is never drawn again
  std::vector<std::string> hashesOf(int account);
  void block(std::string const &hash);
  // True once after a download finished: the players should be dressed again
  bool takeDirty();
}
