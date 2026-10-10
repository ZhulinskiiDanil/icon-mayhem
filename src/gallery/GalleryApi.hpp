#pragma once

#include <Geode/Geode.hpp>

#include <functional>
#include <string>
#include <vector>

// ! --- Gallery --- !
// The community gallery of looks on the Icon Mayhem server (github.com/ZhulinskiiDanil/icon-mayhem-server).
// Reading needs nothing. Sharing, likes and reports need the player's Geometry Dash account:
// Argon proves it once (no password leaves the game), the server gives back a session for 30 days.
// Every callback runs on the main thread.

namespace gallery
{
  struct Look
  {
    int id = 0;
    std::string name;
    int authorId = 0;
    std::string author;
    int likes = 0;
    bool liked = false;
    bool following = false; // we follow its author
    bool mine = false;
    bool hidden = false;
    matjson::Value look; // the settings that differ from the defaults
  };

  struct Page
  {
    std::vector<Look> looks;
    bool hasMore = false;
  };

  template <class T>
  using Done = std::function<void(geode::Result<T, std::string>)>;

  // The server is set up (a build before the server existed has none)
  bool available();
  // A moderator account: can hide and delete any look
  bool isModerator();

  // `sort`: "top", "new", "mine" or "following" (the authors you follow)
  void list(std::string const &sort, std::string const &query, int page, Done<Page> done);
  void publish(std::string const &name, matjson::Value const &look, Done<Look> done);
  // The like count after it
  void like(int id, bool on, Done<int> done);
  void report(int id, Done<bool> done);
  // Follow an author: their looks show under Following
  void follow(int accountId, bool on, Done<bool> done);
  void remove(int id, Done<bool> done);

  // Custom icons (see CustomIcons.hpp): `icon` is {type, quality, png (base64), frames}, the answer its hash
  void uploadIcon(matjson::Value const &icon, Done<std::string> done);
  void fetchIcon(std::string const &hash, Done<matjson::Value> done);
  void reportIcon(std::string const &hash, Done<bool> done);

  // The look worn in a level: Globed carries only its hash, the others fetch it here.
  // `look` is the settings that differ from the defaults, the answer its hash
  void uploadWorn(matjson::Value const &look, Done<std::string> done);
  void fetchWorn(std::string const &hash, Done<matjson::Value> done);
}
