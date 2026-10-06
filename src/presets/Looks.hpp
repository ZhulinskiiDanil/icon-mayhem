#pragma once

#include "../hair/HairConfig.hpp"

#include <string>

// ! --- Looks --- !
// A different look (a preset) for each game mode, for player 1 and player 2 separately. Empty means
// "not chosen". Player 2 looks for its own look of the mode, then its look for all modes, then
// player 1's look of the mode; player 1 for its look of the mode. Nothing chosen: the main look,
// the mod settings the customizer edits.
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

  std::string lookFor(bool playerTwo, GameMode mode);

  // Config of a look: the preset on top of the mod settings, the mod settings alone for the main
  // look or a preset that doesn't exist anymore. Cached per settings version
  HairConfig configFor(std::string const &name);
  // Presets were saved, deleted or imported: read them again
  void invalidate();
}
