#include "SimplePlayerHair.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/ProfilePage.hpp>

using namespace geode::prelude;

// ! --- Helpers --- !

namespace
{
  // Every icon preview under `node`, they wear the look of their icon
  void attachToIcons(CCNode *node)
  {
    if (!node)
      return;
    if (auto player = typeinfo_cast<SimplePlayer *>(node))
    {
      attachSimplePlayerHair(player, PreviewPlace::Menu);
      return;
    }
    for (auto child : CCArrayExt<CCNode *>(node->getChildren()))
      attachToIcons(child);
  }
}

// ! --- Menu --- !
// The icon on the profile button in the bottom left corner

class $modify(HairMenuLayer, MenuLayer)
{
  bool init()
  {
    if (!MenuLayer::init())
      return false;

    attachToIcons(m_profileButton);
    return true;
  }
};

// ! --- Profile --- !
// Only your own profile: other players' icons don't wear your look

class $modify(HairProfilePage, ProfilePage)
{
  void loadPageFromUserInfo(GJUserScore *score)
  {
    ProfilePage::loadPageFromUserInfo(score);

    if (m_ownProfile)
      attachToIcons(m_mainLayer);
  }
};
