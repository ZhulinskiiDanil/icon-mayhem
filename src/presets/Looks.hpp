#pragma once

#include "../hair/HairConfig.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

// ! --- Looks --- !
// A different look (a preset) for each icon and each game mode, for player 1 and player 2
// separately. Empty means "not chosen". Player 1 wears the look of its icon, then the look of the
// mode. Player 2 its own look of the mode, then its look for all modes, then the look of the icon,
// then player 1's look of the mode. Nothing chosen: the main look, the mod settings the customizer
// edits.
// The assignments are saved values; the rigs in a level ask lookFor() every frame and reload
// their config when the answer changes.

namespace looks
{
  char const *modeName(GameMode mode);

  std::string forMode(GameMode mode, bool playerTwo);
  void setForMode(GameMode mode, bool playerTwo, std::string const &name);
  // Player 2's look for every mode without a look of its own
  std::string forPlayerTwo();
  void setForPlayerTwo(std::string const &name);

  // A look for one icon of a game mode (the icon number in the garage)
  std::string forIcon(GameMode mode, int icon);
  void setForIcon(GameMode mode, int icon, std::string const &name);
  // Icons that have a look, for the list in the popup
  std::vector<std::pair<GameMode, int>> iconsWithLooks();
  // The icon of this mode the player has on now
  int equippedIcon(GameMode mode);

  // Icons of More Icons, by their name: a look of their own the same way
  std::string forCustomIcon(GameMode mode, std::string const &name);
  void setForCustomIcon(GameMode mode, std::string const &name, std::string const &preset);
  // The More Icons icon worn in a mode, "" for an icon of the game (or without More Icons)
  std::string equippedCustomIcon(GameMode mode, bool dual = false);
  // The preset linked to the icon worn in a mode: its More Icons icon, or its game icon
  std::string linkedLook(GameMode mode);
  // Ships, UFOs and jetpacks carry the cube: the hair sits on it, so its link counts there
  GameMode linkModeOf(GameMode mode);
  // The look a player wears now in a mode: the link of the icon (the cube in a vehicle), then the
  // look of the mode (see lookFor)
  std::string wornLook(bool playerTwo, GameMode mode);
  IconType iconTypeOf(GameMode mode);

  // `icon` < 0: unknown, icon looks are skipped. `custom`: the More Icons icon worn, its link
  // counts instead of the game icon under it
  std::string lookFor(bool playerTwo, GameMode mode, int icon = -1, std::string const &custom = "");

  // Config of a look: the preset on top of the mod settings, the mod settings alone for the main
  // look or a preset that doesn't exist anymore. Cached per settings version
  HairConfig configFor(std::string const &name);
  // Settings of a preset by name, read from disk once (until invalidate())
  std::optional<matjson::Value> presetSettings(std::string const &name);
  // Presets were saved, deleted or imported: read them again
  void invalidate();
  // A preset was renamed: the modes, player 2 and icons that wore it wear the new name
  void renamePreset(std::string const &from, std::string const &to);
  // The look name of a built-in preset, even when a saved preset has the same name
  std::string builtInLook(std::string const &name);

  // Looks of other players on Globed, by their player id. configFor() reads them as "@remote:<id>",
  // a player without a look (no mod) wears nothing
  std::string remoteName(int player);
  bool hasRemote(int player);
  void setRemote(int player, matjson::Value look);
  void clearRemotes();
}
