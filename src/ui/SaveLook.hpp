#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <functional>
#include <string>

// ! --- Saving the look --- !
// One tap saves the look into the preset it came from. A look from a built-in preset or from
// nothing asks for a name first.

// A small popup asking for a name, filled in with `initial`
class NamePrompt : public geode::Popup
{
public:
  static NamePrompt *create(std::string const &title, std::string const &initial, std::string const &action,
                            std::function<void(std::string const &)> done);

private:
  bool initPrompt(std::string const &title, std::string const &initial, std::string const &action,
                  std::function<void(std::string const &)> done);
  void onConfirm(cocos2d::CCObject *sender);

  geode::TextInput *m_input = nullptr;
  std::function<void(std::string const &)> m_done;
};

namespace saveLook
{
  // Saves the look into its own preset after a confirmation, or asks for a name (see above).
  // `saved` runs after it was saved
  void quick(std::function<void()> saved);
  // Asks for a name and saves the look as a preset, asking before replacing one
  void asNew(std::function<void()> saved);
  // The preset the look came from: "Kepochka", "Kepochka (built-in)" or "Not saved"
  std::string currentName();
}
