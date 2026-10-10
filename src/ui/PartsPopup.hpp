#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

#include <functional>
#include <string>

// ! --- Parts --- !
// Every block of the customizer on one page, by tab: a chip each, green while it is on.
// A chip closes the page and goes to its block.

class PartsPopup : public geode::Popup
{
public:
  // `picked` gets the block id (its title key)
  static PartsPopup *create(std::function<void(std::string const &)> picked);

private:
  bool initParts(std::function<void(std::string const &)> picked);

  std::function<void(std::string const &)> m_picked;
};
