#include "hair/HairConfig.hpp"
#include "presets/Presets.hpp"
#include "settings/Settings.hpp"
#include "ui/CustomizerPopup.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

// ! --- Settings --- !

$on_mod(Loaded)
{
  // The look lives in look.json, the general settings on Geode's page
  settings::load();

  // The game closed while the customizer edited an icon look: the main look comes back
  if (presets::hasStashedLook())
    presets::restoreMainLook();

  listenForAllSettingChanges([](std::string_view, std::shared_ptr<SettingV3>)
                             { HairConfig::bumpVersion(); });

  // "Your look" on Geode's page: the customizer, or the folder with look.json
  ButtonSettingPressedEventV3(Mod::get(), "look-file")
      .listen([](std::string_view button)
              {
                if (button == "customizer")
                {
                  if (auto popup = CustomizerPopup::create())
                    popup->show();
                }
                else if (button == "folder")
                  file::openFolder(settings::filePath().parent_path());
                return false; })
      .leak();
}

$on_mod(DataSaved)
{
  settings::flush();
}
