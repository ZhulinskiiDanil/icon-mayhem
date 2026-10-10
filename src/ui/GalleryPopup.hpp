#pragma once

#include "../gallery/GalleryApi.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <optional>
#include <string>
#include <vector>

// ! --- Gallery --- !
// Looks shared by everyone: the most liked, the newest, yours. Each on your icon, alive. Wear one,
// keep it in your presets, like it, report it; share your own look. Opened from the presets
// window and the General tab.

class GalleryPopup : public geode::Popup
{
public:
  static GalleryPopup *create();

private:
  bool initGallery();
  void update(float dt) override;

  void buildTabs();
  void buildDetail();
  void load();
  void showPage();
  void refreshDetail();
  cocos2d::CCNode *createCard(gallery::Look const &look, size_t index);
  // On the icon of the gallery view (the author's, yours or the plain one), `look` gives the author's
  cocos2d::CCNode *createPreview(std::string const &lookName, float scale, gallery::Look const *look);
  std::string lookName(gallery::Look const &look) const;

  void onShare(cocos2d::CCObject *sender);
  void onPage(cocos2d::CCObject *sender);
  void wear(gallery::Look const &look);
  void keep(gallery::Look const &look);
  void toggleLike(size_t index);
  void toggleFollow(size_t index);
  void report(gallery::Look const &look);
  void remove(gallery::Look const &look);

  std::string m_sort = "top";
  std::string m_query;
  int m_page = 0;
  bool m_hasMore = false;
  bool m_loading = false;
  unsigned m_request = 0; // answers of older requests are dropped
  std::vector<gallery::Look> m_looks;
  std::optional<size_t> m_selected;
  float m_searchWait = -1.f; // s until the search runs, after the last key

  cocos2d::CCMenu *m_tabMenu = nullptr;
  cocos2d::CCNode *m_grid = nullptr;
  cocos2d::CCLabelBMFont *m_status = nullptr;
  cocos2d::CCLabelBMFont *m_pageLabel = nullptr;
  geode::TextInput *m_search = nullptr;

  cocos2d::CCNode *m_detail = nullptr;
  cocos2d::CCMenu *m_detailMenu = nullptr;
  CCMenuItemSpriteExtra *m_iconToggle = nullptr; // your icon or the vanilla one
};

// The General tab row that opens the gallery
cocos2d::CCNode *createGalleryRow(float width);

// Shares a look in the gallery on a cube (the game's number, and a More Icons icon by its name or
// ""): asks for its name first, `suggestion` filled in; `done` after it is in
void shareToGallery(std::string const &suggestion, matjson::Value look, int cube, std::string const &customCube, std::function<void()> done);
