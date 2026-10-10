#pragma once

#include <Geode/Geode.hpp>

#include <functional>

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
  // Its icon sprite draws their custom icon (when they have one for what it shows); false when the game
  // or Globed put its own frame back and it should be dressed again
  bool isDressed(PlayerObject *player, int account);
  // What a player shows, for the Report button; a blocked hash is never drawn again
  std::vector<std::string> hashesOf(int account);
  void block(std::string const &hash);
  // True once after a download finished: the players should be dressed again
  bool takeDirty();

  // The custom icon of a cube of ours (the game's number, or a More Icons icon by its name), for a
  // look shared in the gallery: its hash once it is on the server, "" when it is the game's own
  // cube (or custom icons aren't shared)
  void cubeIconHash(int number, std::string const &custom, std::function<void(std::string)> done);
  // A preview wears the custom cube of that hash, now or once it is downloaded
  void dressPreview(SimplePlayer *player, std::string const &hash);
}
