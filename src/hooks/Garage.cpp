#include "SimplePlayerHair.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GJGarageLayer.hpp>

using namespace geode::prelude;

// ! --- Garage --- !

class $modify(HairGarageLayer, GJGarageLayer)
{
  bool init()
  {
    if (!GJGarageLayer::init())
      return false;

    attachSimplePlayerHair(m_playerObject, true);

    return true;
  }
};
