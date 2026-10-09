#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>

#include <functional>
#include <string>

// ! --- Preset picker --- !
// Picks a preset by name: search on top, your presets (the latest first) then the built-in ones,
// and "nothing" on the very top (the main look, not linked...). One tap picks and closes.

class PresetPicker : public geode::Popup
{
public:
  // `current` is marked, `none` names the empty choice ("" when there is none to pick)
  static PresetPicker *create(std::string const &title, std::string const &current, std::string const &none,
                              std::function<void(std::string const &)> picked);

private:
  bool initPicker(std::string const &title, std::string const &current, std::string const &none,
                  std::function<void(std::string const &)> picked);
  void refreshList();
  cocos2d::CCNode *createRow(std::string const &name, std::string const &shown, bool builtIn);
  cocos2d::CCNode *createLabelRow(char const *text);
  void pick(std::string const &name);

  std::function<void(std::string const &)> m_picked;
  std::string m_current;
  std::string m_none;
  std::string m_query;
  std::vector<std::string> m_mine;
  std::vector<std::string> m_builtIn;
  geode::ScrollLayer *m_list = nullptr;
};
