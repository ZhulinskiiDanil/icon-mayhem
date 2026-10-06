#include "PresetsPopup.hpp"

#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Scrollbar.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/file.hpp>

using namespace geode::prelude;

// ! --- Constants --- !

namespace
{
  constexpr float kWidth = 360.f;
  constexpr float kHeight = 260.f;

  constexpr CCPoint kListOrigin = {20.f, 50.f};
  constexpr CCSize kListSize = {310.f, 140.f};
  constexpr float kRowHeight = 30.f;
  constexpr float kNameY = 212.f;
  constexpr size_t kMaxNameLength = 32;

  ButtonSprite *textButton(char const *text, int width, char const *texture = "GJ_button_01.png", float height = 24.f)
  {
    return ButtonSprite::create(text, width, true, "goldFont.fnt", texture, height, .6f);
  }

  void notify(std::string const &text, NotificationIcon icon)
  {
    Notification::create(text, icon)->show();
  }

  file::FilePickOptions jsonFiles(std::optional<std::filesystem::path> defaultPath = std::nullopt)
  {
    return file::FilePickOptions{
        .defaultPath = std::move(defaultPath),
        .filters = {file::FilePickOptions::Filter{
            .description = "Icon Mayhem presets",
            .files = {"*.json"},
        }},
    };
  }
}

// ! --- Creation --- !

PresetsPopup *PresetsPopup::create(std::function<void()> onApplied)
{
  auto popup = new PresetsPopup();
  if (popup->initPresets(std::move(onApplied)))
  {
    popup->autorelease();
    return popup;
  }
  delete popup;
  return nullptr;
}

bool PresetsPopup::initPresets(std::function<void()> onApplied)
{
  if (!Popup::init(kWidth, kHeight))
    return false;

  m_onApplied = std::move(onApplied);
  this->setID("presets-popup"_spr);
  this->setTitle("Presets");

  // List of saved presets
  auto listBg = NineSlice::create("square02b_001.png");
  listBg->setColor({0, 0, 0});
  listBg->setOpacity(80);
  listBg->setContentSize(kListSize);
  listBg->setAnchorPoint({0.f, 0.f});
  m_mainLayer->addChildAtPosition(listBg, Anchor::BottomLeft, kListOrigin);

  m_list = ScrollLayer::create(kListSize);
  m_list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(0.f));
  m_list->setTouchEnabled(true);
  m_mainLayer->addChildAtPosition(m_list, Anchor::BottomLeft, kListOrigin);

  auto scrollbar = Scrollbar::create(m_list);
  m_mainLayer->addChildAtPosition(scrollbar, Anchor::BottomLeft,
                                  {kListOrigin.x + kListSize.width + 8.f, kListOrigin.y + kListSize.height / 2.f});

  m_emptyLabel = CCLabelBMFont::create("No presets yet", "bigFont.fnt");
  m_emptyLabel->setScale(.4f);
  m_emptyLabel->setOpacity(150);
  m_mainLayer->addChildAtPosition(m_emptyLabel, Anchor::BottomLeft,
                                  {kListOrigin.x + kListSize.width / 2.f, kListOrigin.y + kListSize.height / 2.f});

  m_buttonMenu->setTouchPriority(m_list->getTouchPriority() - 1);

  // Save the current look
  m_nameInput = TextInput::create(220.f, "Preset name");
  m_nameInput->setMaxCharCount(kMaxNameLength);
  m_nameInput->setScale(.8f);
  m_mainLayer->addChildAtPosition(m_nameInput, Anchor::BottomLeft, {kListOrigin.x + 88.f, kNameY});

  auto save = CCMenuItemSpriteExtra::create(textButton("Save", 60), this, menu_selector(PresetsPopup::onSave));
  save->setID("save-button");
  m_buttonMenu->addChildAtPosition(save, Anchor::BottomLeft, {kListOrigin.x + kListSize.width - 35.f, kNameY});

  // Import
  auto paste = CCMenuItemSpriteExtra::create(textButton("Paste", 60), this, menu_selector(PresetsPopup::onPaste));
  paste->setID("paste-button");
  m_buttonMenu->addChildAtPosition(paste, Anchor::Bottom, {-95.f, 25.f});

  auto import = CCMenuItemSpriteExtra::create(textButton("Import", 70), this, menu_selector(PresetsPopup::onImportFile));
  import->setID("import-button");
  m_buttonMenu->addChildAtPosition(import, Anchor::Bottom, {0.f, 25.f});

  auto folder = CCMenuItemSpriteExtra::create(textButton("Folder", 70, "GJ_button_04.png"), this,
                                              menu_selector(PresetsPopup::onFolder));
  folder->setID("folder-button");
  m_buttonMenu->addChildAtPosition(folder, Anchor::Bottom, {100.f, 25.f});

  this->refreshList();
  return true;
}

// ! --- List --- !

void PresetsPopup::refreshList()
{
  m_list->m_contentLayer->removeAllChildren();

  // Shipped looks first, then the player's own
  auto const builtIn = presets::builtIn();
  for (auto const &preset : builtIn)
    m_list->m_contentLayer->addChild(this->createRow(preset));

  auto const saved = presets::list();
  for (auto const &preset : saved)
    m_list->m_contentLayer->addChild(this->createRow(preset));

  m_list->m_contentLayer->updateLayout();
  m_list->scrollToTop();
  m_emptyLabel->setVisible(builtIn.empty() && saved.empty());
}

CCNode *PresetsPopup::createRow(Preset const &preset)
{
  auto row = CCNode::create();
  row->setContentSize({kListSize.width, kRowHeight});
  row->setAnchorPoint({.5f, .5f});

  float const centerY = kRowHeight / 2.f;

  auto name = CCLabelBMFont::create(preset.name.c_str(), "bigFont.fnt");
  name->setAnchorPoint({0.f, .5f});
  name->limitLabelWidth(100.f, .45f, .1f);
  name->setPosition({28.f, centerY});
  row->addChild(name);

  auto menu = CCMenu::create();
  menu->setPosition({0.f, 0.f});
  menu->setContentSize(row->getContentSize());
  row->addChild(menu);

  auto addButton = [&](CCNode *sprite, float x, geode::Function<void(CCMenuItemSpriteExtra *)> callback)
  {
    auto button = CCMenuItemExt::createSpriteExtra(sprite, std::move(callback));
    button->setPosition({x, centerY});
    menu->addChild(button);
  };

  // Star: a favorite, switched through with the button in the pause menu
  bool const favorite = presets::isFavorite(preset.name);
  auto star = CCSprite::createWithSpriteFrameName("GJ_starsIcon_001.png");
  star->setScale(.7f);
  if (!favorite)
  {
    star->setColor({70, 70, 70});
    star->setOpacity(160);
  }
  addButton(star, 15.f, [this, name = preset.name, favorite](auto)
            {
              presets::setFavorite(name, !favorite);
              // Stay where the list was scrolled to
              float const scrolled = m_list->m_contentLayer->getPositionY();
              this->refreshList();
              m_list->m_contentLayer->setPositionY(scrolled);
            });

  float const right = kListSize.width;
  addButton(textButton("Load", 44, "GJ_button_01.png", 20.f), right - 170.f, [this, preset](auto)
            { this->load(preset); });
  addButton(textButton("Copy", 44, "GJ_button_02.png", 20.f), right - 118.f, [this, preset](auto)
            { this->copy(preset); });
  addButton(textButton("File", 40, "GJ_button_02.png", 20.f), right - 68.f, [this, preset](auto)
            { this->exportFile(preset); });

  if (preset.builtIn)
  {
    // Shipped with the mod: a mark instead of the delete button
    auto mark = CCLabelBMFont::create("Built-in", "goldFont.fnt");
    mark->setScale(.3f);
    mark->setPosition({right - 22.f, centerY});
    row->addChild(mark);
    return row;
  }

  auto deleteSprite = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");
  deleteSprite->setScale(.6f);
  addButton(deleteSprite, right - 22.f, [this, preset](auto)
            { this->remove(preset); });

  return row;
}

// ! --- Save --- !

void PresetsPopup::onSave(CCObject *)
{
  std::string name = m_nameInput->getString();
  if (name.empty())
  {
    notify("Enter a name for the preset", NotificationIcon::Warning);
    return;
  }

  auto doSave = [self = Ref(this), name]
  {
    if (auto result = presets::save(presets::capture(name)); !result)
    {
      notify(fmt::format("Unable to save: {}", result.unwrapErr()), NotificationIcon::Error);
      return;
    }
    notify(fmt::format("Saved \"{}\"", name), NotificationIcon::Success);
    self->m_nameInput->setString("");
    self->refreshList();
  };

  if (!presets::exists(name))
  {
    doSave();
    return;
  }

  createQuickPopup("Overwrite", fmt::format("A preset named <cy>{}</c> already exists. Replace it?", name),
                   "Cancel", "Replace",
                   [doSave](FLAlertLayer *, bool confirmed)
                   {
                     if (confirmed)
                       doSave();
                   });
}

// ! --- Preset actions --- !

void PresetsPopup::load(Preset const &preset)
{
  presets::apply(preset);
  notify(fmt::format("Loaded \"{}\"", preset.name), NotificationIcon::Success);
  if (m_onApplied)
    m_onApplied();
}

void PresetsPopup::copy(Preset const &preset)
{
  if (clipboard::write(presets::toJson(preset).dump(matjson::NO_INDENTATION)))
    notify("Copied to the clipboard, paste it to share", NotificationIcon::Success);
  else
    notify("Unable to copy to the clipboard", NotificationIcon::Error);
}

void PresetsPopup::exportFile(Preset const &preset)
{
  async::spawn(file::pick(file::PickMode::SaveFile, jsonFiles(presets::pathOf(preset.name))),
               [preset](file::PickResult result)
               {
                 if (!result)
                 {
                   notify(fmt::format("Unable to export: {}", result.unwrapErr()), NotificationIcon::Error);
                   return;
                 }
                 auto path = std::move(result).unwrap();
                 if (!path)
                   return;

                 if (auto written = presets::writeFile(preset, *path); !written)
                   notify(fmt::format("Unable to export: {}", written.unwrapErr()), NotificationIcon::Error);
                 else
                   notify("Exported", NotificationIcon::Success);
               });
}

void PresetsPopup::remove(Preset const &preset)
{
  createQuickPopup("Delete", fmt::format("Delete the preset <cy>{}</c>?", preset.name), "Cancel", "Delete",
                   [self = Ref(this), name = preset.name](FLAlertLayer *, bool confirmed)
                   {
                     if (!confirmed)
                       return;
                     if (auto result = presets::remove(name); !result)
                       notify(result.unwrapErr(), NotificationIcon::Error);
                     self->refreshList();
                   });
}

// ! --- Import --- !

void PresetsPopup::addImported(Preset preset)
{
  // Never overwrite on import: "Name", "Name 2", "Name 3"...
  std::string const base = preset.name;
  for (int i = 2; presets::exists(preset.name); ++i)
    preset.name = fmt::format("{} {}", base, i);

  if (auto result = presets::save(preset); !result)
  {
    notify(fmt::format("Unable to import: {}", result.unwrapErr()), NotificationIcon::Error);
    return;
  }
  notify(fmt::format("Imported \"{}\"", preset.name), NotificationIcon::Success);
  this->refreshList();
}

void PresetsPopup::onPaste(CCObject *)
{
  auto preset = presets::parse(clipboard::read());
  if (!preset)
  {
    notify(fmt::format("The clipboard has no preset: {}", preset.unwrapErr()), NotificationIcon::Error);
    return;
  }
  this->addImported(std::move(preset).unwrap());
}

void PresetsPopup::onImportFile(CCObject *)
{
  async::spawn(file::pick(file::PickMode::OpenFile, jsonFiles()),
               [self = Ref(this)](file::PickResult result)
               {
                 if (!result)
                 {
                   notify(fmt::format("Unable to import: {}", result.unwrapErr()), NotificationIcon::Error);
                   return;
                 }
                 auto path = std::move(result).unwrap();
                 if (!path)
                   return;

                 auto preset = presets::readFile(*path);
                 if (!preset)
                 {
                   notify(fmt::format("Unable to import: {}", preset.unwrapErr()), NotificationIcon::Error);
                   return;
                 }
                 self->addImported(std::move(preset).unwrap());
               });
}

void PresetsPopup::onFolder(CCObject *)
{
  (void)file::createDirectoryAll(presets::folder());
  file::openFolder(presets::folder());
}
