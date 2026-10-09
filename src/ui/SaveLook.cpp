#include "SaveLook.hpp"

#include "../presets/Presets.hpp"
#include "Buttons.hpp"

#include <Geode/ui/Notification.hpp>

using namespace geode::prelude;

namespace
{
  constexpr size_t kMaxNameLength = 32;

  void notify(std::string const &text, NotificationIcon icon)
  {
    Notification::create(text, icon)->show();
  }

  std::string trimmed(std::string text)
  {
    auto const first = text.find_first_not_of(' ');
    if (first == std::string::npos)
      return "";
    auto const last = text.find_last_not_of(' ');
    return text.substr(first, last - first + 1);
  }

  void saveAs(std::string const &name, std::function<void()> const &saved)
  {
    if (auto result = presets::save(presets::capture(name)); !result)
    {
      notify(fmt::format("Unable to save: {}", result.unwrapErr()), NotificationIcon::Error);
      return;
    }
    presets::markCurrent(name, false);
    notify(fmt::format("Saved \"{}\"", name), NotificationIcon::Success);
    if (saved)
      saved();
  }
}

// ! --- Name prompt --- !

NamePrompt *NamePrompt::create(std::string const &title, std::string const &initial, std::string const &action,
                               std::function<void(std::string const &)> done)
{
  auto popup = new NamePrompt();
  if (popup->initPrompt(title, initial, action, std::move(done)))
  {
    popup->autorelease();
    return popup;
  }
  delete popup;
  return nullptr;
}

bool NamePrompt::initPrompt(std::string const &title, std::string const &initial, std::string const &action,
                            std::function<void(std::string const &)> done)
{
  if (!Popup::init(260.f, 130.f))
    return false;

  m_done = std::move(done);
  this->setID("name-prompt"_spr);
  this->setTitle(title.c_str());

  m_input = TextInput::create(210.f, "Name");
  m_input->setMaxCharCount(kMaxNameLength);
  m_input->setString(initial);
  m_mainLayer->addChildAtPosition(m_input, Anchor::Center, {0.f, 5.f});

  auto confirm = CCMenuItemSpriteExtra::create(textButton(action.c_str(), 80.f, "GJ_button_01.png", 28.f, .7f),
                                               this, menu_selector(NamePrompt::onConfirm));
  confirm->setID("confirm-button");
  m_buttonMenu->addChildAtPosition(confirm, Anchor::Bottom, {0.f, 25.f});

  m_input->focus();
  return true;
}

void NamePrompt::onConfirm(CCObject *)
{
  std::string const name = trimmed(m_input->getString());
  if (name.empty())
  {
    notify("Enter a name", NotificationIcon::Warning);
    return;
  }
  auto done = std::move(m_done);
  this->onClose(nullptr);
  if (done)
    done(name);
}

// ! --- Saving --- !

void saveLook::asNew(std::function<void()> saved)
{
  // A built-in look saves as your own copy, a look from nothing gets a fresh name
  std::string const current = presets::current();
  std::string suggestion;
  if (current.empty())
    suggestion = presets::uniqueName("My look");
  else if (presets::currentIsBuiltIn())
    suggestion = presets::uniqueName(fmt::format("My {}", current));
  else
    suggestion = presets::uniqueName(current);

  auto prompt = NamePrompt::create("Save as", suggestion, "Save", [saved = std::move(saved)](std::string const &name)
                                   {
                                     if (!presets::exists(name))
                                     {
                                       saveAs(name, saved);
                                       return;
                                     }
                                     createQuickPopup("Replace", fmt::format("A preset named <cy>{}</c> already exists. Replace it with this look?", name),
                                                      "Cancel", "Replace",
                                                      [name, saved](FLAlertLayer *, bool confirmed)
                                                      {
                                                        if (confirmed)
                                                          saveAs(name, saved);
                                                      });
                                   });
  if (prompt)
    prompt->show();
}

void saveLook::quick(std::function<void()> saved)
{
  std::string const current = presets::current();
  if (current.empty() || presets::currentIsBuiltIn() || !presets::exists(current))
  {
    asNew(std::move(saved));
    return;
  }
  if (!presets::modified())
  {
    notify(fmt::format("\"{}\" is already saved", current), NotificationIcon::Info);
    return;
  }
  // The save button is a small icon: ask before writing over the preset
  createQuickPopup("Save", fmt::format("Save your changes into <cy>{}</c>?", current), "Cancel", "Save",
                   [current, saved = std::move(saved)](FLAlertLayer *, bool confirmed)
                   {
                     if (confirmed)
                       saveAs(current, saved);
                   });
}

std::string saveLook::currentName()
{
  std::string const current = presets::current();
  if (current.empty())
    return "Not saved";
  if (presets::currentIsBuiltIn())
    return fmt::format("{} (built-in)", current);
  return current;
}
