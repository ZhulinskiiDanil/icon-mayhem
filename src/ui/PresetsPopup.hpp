#pragma once

#include "../presets/Presets.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>

#include <functional>

// ! --- Presets popup --- !
// Save the current look under a name, load, share (clipboard or file), import and delete presets.
// `onApplied` runs after a preset was loaded, so the customizer can refresh its rows.

class PresetsPopup : public geode::Popup
{
public:
  static PresetsPopup *create(std::function<void()> onApplied);

private:
  bool initPresets(std::function<void()> onApplied);

  void refreshList();
  cocos2d::CCNode *createRow(Preset const &preset);

  void onSave(cocos2d::CCObject *sender);
  void onPaste(cocos2d::CCObject *sender);
  void onImportFile(cocos2d::CCObject *sender);
  void onFolder(cocos2d::CCObject *sender);

  void load(Preset const &preset);
  void copy(Preset const &preset);
  void exportFile(Preset const &preset);
  void remove(Preset const &preset);
  // Saves an imported preset, renaming it if the name is taken
  void addImported(Preset preset);

  std::function<void()> m_onApplied;
  geode::TextInput *m_nameInput = nullptr;
  geode::ScrollLayer *m_list = nullptr;
  cocos2d::CCLabelBMFont *m_emptyLabel = nullptr;
};
