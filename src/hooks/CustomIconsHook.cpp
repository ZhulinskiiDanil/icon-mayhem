#include "Globed.hpp"
#include "../icons/CustomIcons.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// ! --- Custom icons on Globed players --- !
// Whenever the game or Globed sets the frames of an icon, a Globed player's icon gets their custom
// icon back on top. After More Icons, which leaves icons that are not ours alone

class $modify(CustomIconPlayerObject, PlayerObject)
{
  static void onModify(auto &self)
  {
    for (char const *name : {"PlayerObject::updatePlayerFrame", "PlayerObject::updatePlayerShipFrame", "PlayerObject::updatePlayerRollFrame",
                             "PlayerObject::updatePlayerBirdFrame", "PlayerObject::updatePlayerDartFrame", "PlayerObject::updatePlayerSwingFrame",
                             "PlayerObject::updatePlayerJetpackFrame", "PlayerObject::createRobot", "PlayerObject::createSpider",
                             "PlayerObject::toggleRobotMode", "PlayerObject::toggleSpiderMode"})
      (void)self.setHookPriorityAfterPost(name, "hiimjustin000.more_icons");
  }

  void dress()
  {
    if (int const account = globedAccountOf(this))
      custom_icons::apply(this, account);
  }

  void updatePlayerFrame(int frame)
  {
    PlayerObject::updatePlayerFrame(frame);
    this->dress();
  }
  void updatePlayerShipFrame(int frame)
  {
    PlayerObject::updatePlayerShipFrame(frame);
    this->dress();
  }
  void updatePlayerRollFrame(int frame)
  {
    PlayerObject::updatePlayerRollFrame(frame);
    this->dress();
  }
  void updatePlayerBirdFrame(int frame)
  {
    PlayerObject::updatePlayerBirdFrame(frame);
    this->dress();
  }
  void updatePlayerDartFrame(int frame)
  {
    PlayerObject::updatePlayerDartFrame(frame);
    this->dress();
  }
  void updatePlayerSwingFrame(int frame)
  {
    PlayerObject::updatePlayerSwingFrame(frame);
    this->dress();
  }
  void updatePlayerJetpackFrame(int frame)
  {
    PlayerObject::updatePlayerJetpackFrame(frame);
    this->dress();
  }
  void createRobot(int frame)
  {
    PlayerObject::createRobot(frame);
    this->dress();
  }
  void createSpider(int frame)
  {
    PlayerObject::createSpider(frame);
    this->dress();
  }
  void toggleRobotMode(bool enable, bool noEffects)
  {
    PlayerObject::toggleRobotMode(enable, noEffects);
    this->dress();
  }
  void toggleSpiderMode(bool enable, bool noEffects)
  {
    PlayerObject::toggleSpiderMode(enable, noEffects);
    this->dress();
  }
};
