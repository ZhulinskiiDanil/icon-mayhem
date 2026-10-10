#pragma once

#include "SettingRow.hpp"
#include "Sections.hpp"
#include "../presets/Presets.hpp"

#include <set>
#include <string>

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>

class HairNode;

// ! --- Customizer popup --- !
// Icon preview on the left (switch game modes, jump, run), settings of the selected
// section on the right. Sections are plain lists of setting keys, see Sections.hpp.
// A tab is a list of blocks (Cape, Pet, Bangs...): each folds into one header with its switch,
// a block that is off shows nothing more, fine tuning waits behind "Fine tuning". "On now" lists
// the blocks that are on, from every tab.

class CustomizerPopup : public geode::Popup
{
public:
  static CustomizerPopup *create();

private:
  bool initCustomizer();

  void buildPreview();
  void buildTabs();
  void showSection(size_t index);
  // Everything that is on, from every tab
  void showOnNow();
  // The list again for what is shown now (a tab, the search, on now), `keepScroll` where it was
  void rebuildList(bool keepScroll);
  void addGroup(CustomizerGroup const &group, std::string const &title);
  cocos2d::CCNode *createGroupHeader(CustomizerGroup const &group, std::string const &title, bool open, bool expandable);
  cocos2d::CCNode *createFineRow(CustomizerGroup const &group, size_t count, bool open);
  cocos2d::CCNode *createLookFileRow(float width);
  bool isOpen(std::string const &id) const;
  void setOpen(std::string const &id, bool open);
  void onOnNow(cocos2d::CCObject *sender);
  // Every setting of every tab whose name or description has the query in it
  void showSearch(std::string const &query);
  void clearList();
  void addRow(char const *key);
  void updatePreviewIcon();
  // A tap on the preview: the part of the look under it opens its block
  void onPreviewTap(cocos2d::CCPoint const &world);
  // The part of the look at a point of the preview ("" for none) and the rig that drew it
  std::string partUnder(cocos2d::CCPoint const &world, HairNode **owner) const;
  // With a mouse: the part under the cursor glows and its block is named, before the click
  void updateHover(float dt);
  // No other window is open over this one
  bool isTopmost();
  // Shows the block `id` (its title key): its tab, unfolded, at the top of the list, flashing
  void jumpToGroup(std::string const &id);
  void onParts(cocos2d::CCObject *sender);
  // Players on Globed: their looks, likes and gifts
  void onPlayers(cocos2d::CCObject *sender);
  void onScale(cocos2d::CCObject *sender);
  void refreshScaleLabel();
  // Width to build a list row at, and adding it at the row scale
  float rowWidth() const;
  void addToList(cocos2d::CCNode *row);

  void update(float dt) override;
  // Streaks of air through the preview: as strong, gusty and fluttering as the air the hair feels
  void updateWind(float dt);
  void onWind(cocos2d::CCObject *sender);

  void onTab(cocos2d::CCObject *sender);
  void onMode(cocos2d::CCObject *sender);
  void onJump(cocos2d::CCObject *sender);
  void onSurprise(cocos2d::CCObject *sender);
  void onUndo(cocos2d::CCObject *sender);
  void onColors(cocos2d::CCObject *sender);
  void onMatch(cocos2d::CCObject *sender);
  // Applies a look from a button, the change lands on the undo stack like any other
  void applyLook(Preset const &preset);
  void trackUndo(float dt);
  void onRun(cocos2d::CCObject *sender);
  void onReset(cocos2d::CCObject *sender);
  void onHitboxes(cocos2d::CCObject *sender);
  void onPresets(cocos2d::CCObject *sender);
  // Main | Icon: edit the main look or the look of the icon shown in the preview
  void onLookSwitch(cocos2d::CCObject *sender);
  // Edits the preset linked to the icon in the preview, the main look waits aside. `tell` shows
  // a notification; false when there is no link
  bool startIconEdit(bool tell);
  void refreshLookSwitch();
  // Back to the main look, asking to save the icon look first; `then` runs after
  void endIconEdit(std::function<void()> then);
  void onClose(cocos2d::CCObject *sender) override;
  void onSave(cocos2d::CCObject *sender);
  // The preset name on top: green when saved, yellow with a star when changed
  void refreshLookLabel();
  void applyHitboxes();

  // Preview
  cocos2d::CCNode *m_stage = nullptr; // simulation space of the preview hair, moves while running
  SimplePlayer *m_player = nullptr;
  cocos2d::CCLabelBMFont *m_modeLabel = nullptr;
  cocos2d::CCNode *m_legend = nullptr;
  bool m_showHitboxes = false;

  // Wind view
  struct WindStreak
  {
    cocos2d::CCPoint position; // the leading end, in the preview panel
    float length = 0.f;        // 0 for a mote of dust
    float phase = 0.f;         // of the flutter wave
    float speed = 0.f;         // of this streak, a bit different for each
    float depth = 0.f;         // 0 far behind the icon .. 1 near, in front of it
    float curl = 0.f;          // a curl at the end in a strong gust, its side; 0 none
  };
  bool m_showWind = false;
  cocos2d::CCDrawNode *m_wind = nullptr;      // the far air, behind the icon
  cocos2d::CCDrawNode *m_windFront = nullptr; // the near air, in front of it
  cocos2d::CCDrawNode *m_windMeter = nullptr; // how strong the flow is, under the wind button
  cocos2d::CCLabelBMFont *m_windHint = nullptr;
  std::vector<WindStreak> m_streaks;
  float m_windSpawn = 0.f;   // streaks owed, spawned when it reaches 1
  unsigned m_windSeed = 1;
  cocos2d::CCLabelBMFont *m_lookLabel = nullptr;
  cocos2d::CCMenu *m_lookSwitch = nullptr;
  ButtonSprite *m_mainSprite = nullptr;
  ButtonSprite *m_iconSprite = nullptr;
  bool m_editingIcon = false;
  std::string m_iconLook; // the preset of the icon being edited
  unsigned m_lookVersion = 0;
  float m_lookTimer = 0.f;

  // Undo: every burst of changes pushes the look from before it
  std::vector<Preset> m_undo;
  Preset m_stable;            // the look after the last burst settled
  unsigned m_seenVersion = 0; // settings version last looked at
  float m_quiet = 0.f;        // s since the last change
  bool m_changing = false;    // a burst is going on
  size_t m_mode = 0;
  bool m_running = false;
  float m_runDistance = 0.f;
  float m_jumpTime = -1.f; // < 0 while on the ground
  float m_spin = 0.f;      // degrees, cube rotation that stays after a jump

  // Settings
  cocos2d::CCMenu *m_tabMenu = nullptr;
  geode::ScrollLayer *m_list = nullptr;
  std::vector<SettingRow *> m_rows;
  std::vector<char const *> m_shownKeys; // what Reset resets
  unsigned m_settingsRevision = 0;     // of look.json, edited by hand
  size_t m_section = 0;
  geode::TextInput *m_search = nullptr;
  std::string m_query;
  bool m_onNow = false;
  ButtonSprite *m_onNowSprite = nullptr;
  std::set<std::string> m_open; // open blocks, and "<block>+fine" for their fine tuning
  bool m_rebuildPending = false; // rebuilt next frame, never inside the touch of a row
  cocos2d::CCLabelBMFont *m_scaleLabel = nullptr;
  cocos2d::CCNode *m_panel = nullptr; // the preview
  geode::Ref<cocos2d::CCNode> m_hoverShape; // drawn into the texture, not in the scene
  cocos2d::CCRenderTexture *m_hoverTexture = nullptr;
  cocos2d::CCNode *m_hoverTag = nullptr; // the name of the part under the mouse
  cocos2d::CCLabelBMFont *m_hoverLabel = nullptr;
  float m_hoverTime = 0.f; // s over the same kind of thing, for the breathing
};
