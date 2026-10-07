#pragma once

#include "../hair/HairNode.hpp"

#include <Geode/Geode.hpp>

// ! --- SimplePlayer hair --- !
// Hair for icon previews (the garage, the customizer popup, your profile and the menu). Robot and
// spider hair is attached lazily, their sprites only exist after the preview switched to them once.

struct SimplePlayerHair
{
  HairNode *icon = nullptr; // cube, ball, wave, swing
  HairNode *robot = nullptr;
  HairNode *spider = nullptr;
};

enum class PreviewPlace
{
  Customizer, // wears the main look, the one being edited
  Garage,     // the look of the icon, follows "Show in garage"
  Menu,       // your profile and the menu: the look of the icon, follows "Show in menus"
};

void attachSimplePlayerHair(SimplePlayer *player, PreviewPlace place);
SimplePlayerHair getSimplePlayerHair(SimplePlayer *player);
