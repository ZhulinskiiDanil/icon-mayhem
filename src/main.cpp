#include "hair/HairConfig.hpp"
#include "presets/Presets.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

// ! --- Settings --- !

$on_mod(Loaded)
{
  // The game closed while the customizer edited an icon look: the main look comes back
  if (presets::hasStashedLook())
    presets::restoreMainLook();

  listenForAllSettingChanges([](std::string_view, std::shared_ptr<SettingV3>)
                             { HairConfig::bumpVersion(); });
}
