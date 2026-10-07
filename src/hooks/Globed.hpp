#pragma once

#include "../hair/HairNode.hpp"

// ! --- Globed --- !
// With Globed installed, the players in the same level see each other's looks and emotes.
// Without it these do nothing.

void globedSendEmote(HairNode::Emote emote);
