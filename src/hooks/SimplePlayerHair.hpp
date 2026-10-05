#pragma once

#include "../hair/HairNode.hpp"

#include <Geode/Geode.hpp>

// ! --- SimplePlayer hair --- !
// Hair for icon previews (the garage, the customizer popup). Robot and spider hair
// is attached lazily, their sprites only exist after the preview switched to them once.

struct SimplePlayerHair
{
  HairNode *icon = nullptr; // cube, ball, wave, swing
  HairNode *robot = nullptr;
  HairNode *spider = nullptr;
};

// `garage` makes the hair follow the "Show in garage" setting
void attachSimplePlayerHair(SimplePlayer *player, bool garage);
SimplePlayerHair getSimplePlayerHair(SimplePlayer *player);
