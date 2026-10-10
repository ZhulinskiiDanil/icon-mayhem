#pragma once

#include "../hair/HairNode.hpp"

// ! --- Globed --- !
// With Globed installed, the players in the same level see each other's looks and emotes.
// Without it these do nothing.

void globedSendEmote(HairNode::Emote emote);

// ! --- Friends --- !
// The other players in the level, for the Players window of the pause menu

struct GlobedPeer
{
  int id = 0;          // in the session, what events are sent to
  int account = 0;
  std::string name;
  std::string look;    // their look name ("@remote:..."), "" while they sent none (no Icon Mayhem)
  int cube = 1;        // their icon, for the preview
  cocos2d::ccColor3B color1 = {255, 255, 255};
  cocos2d::ccColor3B color2 = {255, 255, 255};
  bool glow = false;
  cocos2d::ccColor3B glowColor = {255, 255, 255};
  bool liked = false;  // we liked their look in this level
};
std::vector<GlobedPeer> globedPeers();
// Sends a like: hearts burst on their icon and they are told who liked it. Once per player per level
bool globedLike(GlobedPeer const &peer);
// Sends them our look: they can try it on or save it. Once a minute per player
bool globedGift(GlobedPeer const &peer);

// Looks other players gifted us, waiting in the Players window
struct GlobedGift
{
  std::string name; // who gifted it
  std::string look; // worn as this look name ("@remote:gift-...")
};
std::vector<GlobedGift> &globedGifts();

// ! --- Custom icons --- !
// The account of a Globed player's icon (its copy in our level), 0 for any other icon
int globedAccountOf(PlayerObject *player);
// Their own game icons back on a player (after reporting their custom icon)
void globedRestoreIcons(int playerId);

// Pairs: two players on Globed wear a matching bow and colors until the level ends
bool globedAskPair(GlobedPeer const &peer);
bool globedAcceptPair(GlobedPeer const &peer);
void globedEndPair();
// With them now / they asked us / we asked them
bool globedPairedWith(GlobedPeer const &peer);
bool globedPairAsked(GlobedPeer const &peer);
bool globedPairPending(GlobedPeer const &peer);
