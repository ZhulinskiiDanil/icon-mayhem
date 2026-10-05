#include "hair/HairConfig.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

// ! --- Settings --- !

$on_mod(Loaded)
{
  listenForAllSettingChanges([](std::string_view, std::shared_ptr<SettingV3>)
                             { HairConfig::bumpVersion(); });
}
