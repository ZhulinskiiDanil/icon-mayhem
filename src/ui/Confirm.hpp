#pragma once

#include <Geode/Geode.hpp>

#include <functional>
#include <string>

// ! --- Explained once --- !
// An icon button that changes the look asks once: what it does and how to take it back. Someone
// who tapped it to find out what the icon means gets the answer and can cancel; after the first
// OK it just works.

inline void confirmOnce(char const *id, std::string const &title, std::string const &text, std::function<void()> action)
{
  std::string const key = fmt::format("explained-{}", id);
  if (geode::Mod::get()->getSavedValue<bool>(key, false))
  {
    action();
    return;
  }
  geode::createQuickPopup(title.c_str(), text + "\n<cy>Asked only this once.</c>", "Cancel", "OK",
                          [key, action = std::move(action)](FLAlertLayer *, bool confirmed)
                          {
                            if (!confirmed)
                              return;
                            geode::Mod::get()->setSavedValue(key, true);
                            action();
                          });
}
