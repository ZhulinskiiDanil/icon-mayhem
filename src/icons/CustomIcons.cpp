#include "CustomIcons.hpp"

#include "../gallery/GalleryApi.hpp"
#include "../presets/Looks.hpp"

// More Icons is optional: its functions are reached through Geode events, without linking to it
#define MORE_ICONS_EVENTS
#include <geode.texture-loader/include/TextureLoader.hpp>
#include <hiimjustin000.more_icons/include/MoreIcons.hpp>

#include <Geode/binding/CCPartAnimSprite.hpp>
#include <Geode/binding/GJRobotSprite.hpp>
#include <Geode/binding/GJSpiderSprite.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/utils/base64.hpp>
#include <Geode/utils/file.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <unordered_map>
#include <vector>

using namespace geode::prelude;

namespace
{
  struct Kind
  {
    IconType type;
    GameMode mode;
    char const *name;
    char const *file; // the game's icon files: icons/<file><number>.png
  };
  constexpr std::array<Kind, 9> kKinds = {
      Kind{IconType::Cube, GameMode::Cube, "cube", "player_"},          Kind{IconType::Ship, GameMode::Ship, "ship", "ship_"},
      Kind{IconType::Ball, GameMode::Ball, "ball", "player_ball_"},     Kind{IconType::Ufo, GameMode::Ufo, "ufo", "bird_"},
      Kind{IconType::Wave, GameMode::Wave, "wave", "dart_"},            Kind{IconType::Robot, GameMode::Robot, "robot", "robot_"},
      Kind{IconType::Spider, GameMode::Spider, "spider", "spider_"},    Kind{IconType::Swing, GameMode::Swing, "swing", "swing_"},
      Kind{IconType::Jetpack, GameMode::Jetpack, "jetpack", "jetpack_"}};

  constexpr char const *kUploadsSave = "custom-icon-uploads"; // "<icon>|<file times>" -> hash
  constexpr char const *kBlockedSave = "custom-icons-blocked";
  constexpr size_t kMaxPng = 1024 * 1024;

  char const *kindName(IconType type)
  {
    for (auto const &kind : kKinds)
    {
      if (kind.type == type)
        return kind.name;
    }
    return "";
  }

  // ! --- Reading an icon sheet --- !
  // Frame names end in what the frame is ("_2" the second color, "_glow"...); the rules of More Icons

  std::optional<std::string> roleOf(std::string const &frameName, IconType type)
  {
    if (!frameName.ends_with("_001.png"))
      return std::nullopt;
    std::string_view const stem(frameName.data(), frameName.size() - 8);
    static constexpr std::array<std::string_view, 13> kRobot = {"_01_2", "_02_2", "_03_2", "_04_2", "_01_extra", "_01_glow", "_02_glow",
                                                                "_03_glow", "_04_glow", "_01", "_02", "_03", "_04"};
    static constexpr std::array<std::string_view, 4> kUfo = {"_2", "_3", "_extra", "_glow"};
    static constexpr std::array<std::string_view, 3> kCube = {"_2", "_extra", "_glow"};

    bool const robot = type == IconType::Robot || type == IconType::Spider;
    auto check = [&](auto const &endings) -> std::optional<std::string>
    {
      for (auto ending : endings)
      {
        if (stem.ends_with(ending))
          return std::string(ending.substr(1));
      }
      return std::nullopt;
    };
    if (robot)
      return check(kRobot);
    if (auto role = type == IconType::Ufo ? check(kUfo) : check(kCube))
      return role;
    return std::string();
  }

  float number(CCDictionary *dict, char const *key)
  {
    auto value = dict->valueForKey(key);
    return value ? value->floatValue() : 0.f;
  }

  // The frames of a sheet in pixels, with their roles
  std::optional<matjson::Value> readFrames(std::filesystem::path const &plist, IconType type)
  {
    auto dict = CCDictionary::createWithContentsOfFile(utils::string::pathToString(plist).c_str());
    if (!dict)
      return std::nullopt;
    auto frames = typeinfo_cast<CCDictionary *>(dict->objectForKey("frames"));
    if (!frames)
      return std::nullopt;
    int format = 0;
    if (auto metadata = typeinfo_cast<CCDictionary *>(dict->objectForKey("metadata")))
      format = metadata->valueForKey("format")->intValue();

    auto out = matjson::Value::array();
    for (auto [name, object] : CCDictionaryExt<std::string, CCObject *>(frames))
    {
      auto role = roleOf(name, type);
      auto frame = typeinfo_cast<CCDictionary *>(object);
      if (!role || !frame)
        continue;

      CCRect rect;
      CCPoint offset;
      CCSize original;
      bool rotated = false;
      if (format == 0)
      {
        rect = CCRect{number(frame, "x"), number(frame, "y"), number(frame, "width"), number(frame, "height")};
        offset = CCPoint{number(frame, "offsetX"), number(frame, "offsetY")};
        original = CCSize{std::abs(std::floor(number(frame, "originalWidth"))), std::abs(std::floor(number(frame, "originalHeight")))};
      }
      else if (format == 1 || format == 2)
      {
        rect = CCRectFromString(frame->valueForKey("frame")->getCString());
        offset = CCPointFromString(frame->valueForKey("offset")->getCString());
        original = CCSizeFromString(frame->valueForKey("sourceSize")->getCString());
        rotated = format == 2 && frame->valueForKey("rotated")->boolValue();
      }
      else if (format == 3)
      {
        rect.origin = CCRectFromString(frame->valueForKey("textureRect")->getCString()).origin;
        rect.size = CCSizeFromString(frame->valueForKey("spriteSize")->getCString());
        offset = CCPointFromString(frame->valueForKey("spriteOffset")->getCString());
        original = CCSizeFromString(frame->valueForKey("spriteSourceSize")->getCString());
        rotated = frame->valueForKey("textureRotated")->boolValue();
      }
      else
        return std::nullopt;

      auto entry = matjson::Value::object();
      entry["role"] = *role;
      entry["x"] = rect.origin.x;
      entry["y"] = rect.origin.y;
      entry["w"] = rect.size.width;
      entry["h"] = rect.size.height;
      entry["rotated"] = rotated;
      entry["ox"] = offset.x;
      entry["oy"] = offset.y;
      entry["sw"] = original.width;
      entry["sh"] = original.height;
      out.push(std::move(entry));
    }
    if (out.size() == 0)
      return std::nullopt;
    return out;
  }

  // ! --- Ours --- !

  std::string s_payload;
  bool s_uploading = false;

  bool sharing()
  {
    return Mod::get()->getSettingValue<bool>("share-icons");
  }

  // Where an icon we wear comes from: a More Icons icon, or a game icon a texture pack draws
  struct Source
  {
    IconType type;
    std::string name;
    std::filesystem::path png;
    std::filesystem::path plist;
    int quality = 4; // pixels per point of the sheet
  };

  // An icon is the same while its files are: name and file times
  std::string uploadKey(Source const &source)
  {
    std::error_code error;
    auto const texture = std::filesystem::last_write_time(source.png, error).time_since_epoch().count();
    auto const sheet = std::filesystem::last_write_time(source.plist, error).time_since_epoch().count();
    return fmt::format("{}|{}|{}", utils::string::pathToString(source.png), texture, sheet);
  }

  // A path to compare: forward slashes, lower case, no doubled or trailing slashes. Geode and cocos
  // write the same folder differently on Windows (\ and /)
  std::string comparablePath(std::string text)
  {
    std::replace(text.begin(), text.end(), '\\', '/');
    text = utils::string::toLower(text);
    std::string out;
    for (char c : text)
    {
      if (c == '/' && !out.empty() && out.back() == '/')
        continue;
      out += c;
    }
    while (!out.empty() && out.back() == '/')
      out.pop_back();
    return out;
  }

  // The file the game draws an icon from, in the texture quality it uses. Asked with the quality in
  // the name like the game does: a pack that only has the "-uhd" file is found that way (asked
  // without it, the game finds its own plain file first and adds "-uhd" there)
  // The quality endings of the game's files, the one it draws with first
  std::vector<char const *> qualitySuffixes()
  {
    float const scale = CCDirector::sharedDirector()->getContentScaleFactor();
    if (scale >= 4.f)
      return {"-uhd", "-hd", ""};
    if (scale >= 2.f)
      return {"-hd", ""};
    return {""};
  }

  // The file of the texture packs applied in Texture Loader, the first pack that has it wins
  // (the top of its list). Asked from the packs themselves: the game's own file lookup doesn't
  // always see them
  std::string appliedPackFile(std::string const &base, char const *extension)
  {
    if (!texture_loader::isLoaded())
      return "";
    for (auto const &pack : texture_loader::getAppliedPacks())
    {
      for (auto suffix : qualitySuffixes())
      {
        auto const path = pack.resourcesPath / fmt::format("{}{}{}", base, suffix, extension);
        std::error_code error;
        if (std::filesystem::is_regular_file(path, error))
          return utils::string::pathToString(path);
      }
    }
    return "";
  }

  std::string iconFile(std::string const &base, char const *extension)
  {
    auto files = CCFileUtils::sharedFileUtils();
    for (auto suffix : qualitySuffixes())
    {
      std::string const full = files->fullPathForFilename(fmt::format("{}{}{}", base, suffix, extension).c_str(), true);
      if (!full.empty() && files->isFileExist(full))
        return full;
    }
    return "";
  }

  // The game icon we wear, when a texture pack draws it (its files are not the game's own: everybody
  // else sees the game's icon of that number)
  std::optional<Source> packSource(Kind const &kind, int number)
  {
    if (number <= 0)
      return std::nullopt;
    std::string const base = fmt::format("icons/{}{:02}", kind.file, number);

    // A pack of Texture Loader that has it, else the file the game finds when it is in Geode's
    // folder (packs and mods live there, the game's own icons don't)
    std::string png = appliedPackFile(base, ".png");
    std::string plist = appliedPackFile(base, ".plist");
    if (png.empty() || plist.empty())
    {
      png = iconFile(base, ".png");
      plist = iconFile(base, ".plist");
      auto const geode = comparablePath(utils::string::pathToString(dirs::getGeodeDir())) + "/";
      if (png.empty() || plist.empty() || !comparablePath(png).starts_with(geode))
      {
        log::debug("Custom icons: {} {} is the game's own ({})", kind.name, number, png);
        return std::nullopt;
      }
    }

    Source source;
    source.type = kind.type;
    source.name = fmt::format("{} {}", kind.name, number);
    source.png = std::filesystem::path(png);
    source.plist = std::filesystem::path(plist);
    source.quality = png.find("-uhd.") != std::string::npos ? 4 : png.find("-hd.") != std::string::npos ? 2 : 1;
    return source;
  }

  // A More Icons icon, when its files are plain files that aren't the game's own
  std::optional<Source> moreIconsSource(Kind const &kind, IconInfo *info)
  {
    if (info)
    {
      // Icons from zipped packs stay ours: their files aren't plain files. An icon of a texture pack
      // named like a game icon counts as "vanilla" for More Icons but is the pack's picture: it is
      // shared when its file is not the game's own
      auto const geode = comparablePath(utils::string::pathToString(dirs::getGeodeDir())) + "/";
      bool const ownFile = !info->isVanilla() || comparablePath(utils::string::pathToString(info->getTexture())).starts_with(geode);
      std::error_code error;
      if (!info->isZipped() && ownFile && std::filesystem::is_regular_file(info->getTexture(), error) &&
          std::filesystem::is_regular_file(info->getSheet(), error))
      {
        Source source;
        source.type = kind.type;
        source.name = info->getName();
        source.png = info->getTexture();
        source.plist = info->getSheet();
        // More Icons' qualities are cocos' (1 low, 2 medium, 3 high), ours are pixels per point
        source.quality = info->getQuality() >= 3 ? 4 : info->getQuality() == 2 ? 2 : 1;
        return source;
      }
    }
    return std::nullopt;
  }

  // The icon we wear for a kind: More Icons first, then a texture pack
  std::optional<Source> sourceOf(Kind const &kind)
  {
    if (auto source = moreIconsSource(kind, more_icons::activeIcon(kind.type, false)))
      return source;
    return packSource(kind, looks::equippedIcon(kind.mode));
  }

  // The body the server takes for an icon
  std::optional<matjson::Value> iconBody(Source const &source)
  {
    auto png = file::readBinary(source.png);
    if (!png || png.unwrap().empty() || png.unwrap().size() > kMaxPng)
    {
      log::info("Custom icons: {} is not shared ({})", source.name, png ? "too big" : "no picture");
      return std::nullopt;
    }
    auto frames = readFrames(source.plist, source.type);
    if (!frames)
    {
      log::info("Custom icons: {} is not shared (its sheet can't be read)", source.name);
      return std::nullopt;
    }
    auto body = matjson::Value::object();
    body["type"] = kindName(source.type);
    body["quality"] = source.quality;
    body["png"] = utils::base64::encode(std::span<uint8_t const>(png.unwrap()), utils::base64::Base64Variant::Normal);
    body["frames"] = std::move(*frames);
    return body;
  }

  // ! --- Theirs --- !

  struct Loaded
  {
    Ref<CCTexture2D> texture;
    std::unordered_map<std::string, Ref<CCSpriteFrame>> frames; // by role
  };

  std::unordered_map<std::string, Loaded> s_loaded;
  std::set<std::string> s_fetching;
  std::set<std::string> s_failed;
  std::unordered_map<int, std::unordered_map<std::string, std::string>> s_players; // account -> kind -> hash
  bool s_dirty = false;

  std::set<std::string> &blocked()
  {
    static std::set<std::string> list = []
    {
      auto saved = Mod::get()->getSavedValue<std::vector<std::string>>(kBlockedSave, {});
      return std::set<std::string>(saved.begin(), saved.end());
    }();
    return list;
  }

  std::filesystem::path cacheFile(std::string const &hash)
  {
    return Mod::get()->getSaveDir() / "icon-cache" / fmt::format("{}.json", hash);
  }

  // The picture in the device's own resolution: a sheet made for another texture quality is scaled,
  // or the icon would come out twice too big or too small
  CCTexture2D *makeTexture(std::vector<uint8_t> &png, int quality)
  {
    auto image = new CCImage();
    if (!image->initWithImageData(png.data(), static_cast<int>(png.size()), CCImage::kFmtPng))
    {
      image->release();
      return nullptr;
    }
    float const device = CCDirector::sharedDirector()->getContentScaleFactor();
    float const factor = device / static_cast<float>(quality);
    auto texture = new CCTexture2D();
    bool ok = false;
    if (std::abs(factor - 1.f) < .01f)
      ok = texture->initWithImage(image);
    else
    {
      int const width = image->getWidth();
      int const height = image->getHeight();
      int const channels = image->hasAlpha() ? 4 : 3;
      int const newWidth = std::max(1, static_cast<int>(std::lround(width * factor)));
      int const newHeight = std::max(1, static_cast<int>(std::lround(height * factor)));
      unsigned char const *source = image->getData();
      std::vector<uint8_t> scaled(static_cast<size_t>(newWidth) * newHeight * 4);
      // Smaller: the average of each block; bigger: the nearest pixel
      int const block = factor < 1.f ? std::max(1, static_cast<int>(std::lround(1.f / factor))) : 1;
      for (int y = 0; y < newHeight; ++y)
      {
        for (int x = 0; x < newWidth; ++x)
        {
          std::array<int, 4> sum = {0, 0, 0, 0};
          int count = 0;
          int const startX = factor < 1.f ? x * block : static_cast<int>(x / factor);
          int const startY = factor < 1.f ? y * block : static_cast<int>(y / factor);
          for (int dy = 0; dy < block; ++dy)
          {
            for (int dx = 0; dx < block; ++dx)
            {
              int const sx = std::min(startX + dx, width - 1);
              int const sy = std::min(startY + dy, height - 1);
              unsigned char const *pixel = source + (static_cast<size_t>(sy) * width + sx) * channels;
              for (int c = 0; c < 3; ++c)
                sum[c] += pixel[c];
              sum[3] += channels == 4 ? pixel[3] : 255;
              ++count;
            }
          }
          uint8_t *out = scaled.data() + (static_cast<size_t>(y) * newWidth + x) * 4;
          for (int c = 0; c < 4; ++c)
            out[c] = static_cast<uint8_t>(sum[c] / count);
        }
      }
      ok = texture->initWithData(scaled.data(), kCCTexture2DPixelFormat_RGBA8888, newWidth, newHeight,
                                 CCSize(static_cast<float>(newWidth), static_cast<float>(newHeight)));
      texture->m_bHasPremultipliedAlpha = image->isPremultipliedAlpha();
    }
    image->release();
    if (!ok)
    {
      texture->release();
      return nullptr;
    }
    texture->autorelease();
    return texture;
  }

  bool load(std::string const &hash, matjson::Value const &json)
  {
    auto png = utils::base64::decode(json["png"].asString().unwrapOr(""), utils::base64::Base64Variant::Normal);
    int const quality = std::max<int>(1, static_cast<int>(json["quality"].asInt().unwrapOr(4)));
    if (!png || !json["frames"].isArray())
      return false;
    auto texture = makeTexture(png.unwrap(), quality);
    if (!texture)
      return false;

    // A sprite frame takes the pixels of its texture: the sheet's pixels scaled like the texture
    // (sheet quality to the device's), so the icon is the same size at any texture quality
    float const q = static_cast<float>(quality) / CCDirector::sharedDirector()->getContentScaleFactor();
    Loaded loaded;
    loaded.texture = texture;
    for (auto const &frame : json["frames"])
    {
      auto role = frame["role"].asString().unwrapOr("?");
      float const w = static_cast<float>(frame["w"].asDouble().unwrapOr(0.0));
      float const h = static_cast<float>(frame["h"].asDouble().unwrapOr(0.0));
      float const ox = static_cast<float>(frame["ox"].asDouble().unwrapOr(0.0));
      float const oy = static_cast<float>(frame["oy"].asDouble().unwrapOr(0.0));
      CCSize original = {static_cast<float>(frame["sw"].asDouble().unwrapOr(0.0)), static_cast<float>(frame["sh"].asDouble().unwrapOr(0.0))};
      // Sheets with an offset past their size, like More Icons fixes them
      original.width = std::max(original.width, w + std::abs(ox) * 2.f);
      original.height = std::max(original.height, h + std::abs(oy) * 2.f);
      CCRect const rect = {static_cast<float>(frame["x"].asDouble().unwrapOr(0.0)) / q, static_cast<float>(frame["y"].asDouble().unwrapOr(0.0)) / q,
                           w / q, h / q};
      auto sprite = CCSpriteFrame::createWithTexture(texture, rect, frame["rotated"].asBool().unwrapOr(false), CCPoint{ox / q, oy / q},
                                                     CCSize{original.width / q, original.height / q});
      if (sprite)
        loaded.frames[role] = sprite;
    }
    if (loaded.frames.empty())
      return false;

    // What came, to see why an icon looks wrong for someone
    std::string roles;
    for (auto const &[role, sprite] : loaded.frames)
    {
      auto const rect = sprite->getRect();
      roles += fmt::format("{}'{}' {:.0f},{:.0f} {:.0f}x{:.0f}", roles.empty() ? "" : ", ", role, rect.origin.x, rect.origin.y, rect.size.width,
                           rect.size.height);
    }
    log::info("Custom icons: {} is a {} at quality {}, picture {}x{} px drawn at {}x{} (device scale {}), frames {}", hash.substr(0, 8),
              json["type"].asString().unwrapOr("?"), quality, texture->getPixelsWide(), texture->getPixelsHigh(),
              texture->getContentSize().width, texture->getContentSize().height, CCDirector::sharedDirector()->getContentScaleFactor(), roles);
    s_loaded[hash] = std::move(loaded);
    return true;
  }

  // Previews waiting for a custom cube to download
  std::unordered_map<std::string, std::vector<WeakRef<SimplePlayer>>> s_waitingPreviews;
  void dressWaiting(std::string const &hash);

  void fetch(std::string const &hash)
  {
    if (s_loaded.contains(hash) || s_fetching.contains(hash) || s_failed.contains(hash) || blocked().contains(hash))
      return;

    // Kept on disk: an icon never changes under its hash
    if (auto cached = file::readJson(cacheFile(hash)))
    {
      if (load(hash, cached.unwrap()))
      {
        s_dirty = true;
        dressWaiting(hash);
        return;
      }
    }
    s_fetching.insert(hash);
    gallery::fetchIcon(hash, [hash](Result<matjson::Value, std::string> result)
                       {
                         s_fetching.erase(hash);
                         if (!result)
                         {
                           log::info("Custom icons: {} isn't there ({})", hash.substr(0, 8), result.unwrapErr());
                           s_failed.insert(hash);
                           return;
                         }
                         auto json = std::move(result).unwrap();
                         if (!load(hash, json))
                         {
                           s_failed.insert(hash);
                           return;
                         }
                         (void)file::createDirectoryAll(cacheFile(hash).parent_path());
                         (void)file::writeStringSafe(cacheFile(hash), json.dump(matjson::NO_INDENTATION));
                         s_dirty = true;
                         dressWaiting(hash);
                       });
  }

  Loaded const *loadedFor(int account, IconType type, std::string *why = nullptr)
  {
    auto player = s_players.find(account);
    if (player == s_players.end())
    {
      if (why)
        *why = "they sent no custom icons";
      return nullptr;
    }
    auto hash = player->second.find(kindName(type));
    if (hash == player->second.end())
    {
      if (why)
        *why = "they have none of this kind";
      return nullptr;
    }
    if (blocked().contains(hash->second))
    {
      if (why)
        *why = "you reported it";
      return nullptr;
    }
    auto loaded = s_loaded.find(hash->second);
    if (loaded == s_loaded.end())
    {
      if (why)
        *why = s_failed.contains(hash->second) ? "it couldn't be downloaded" : "it is still downloading";
      return nullptr;
    }
    return &loaded->second;
  }

  CCSpriteFrame *frameOf(Loaded const &icon, std::string const &role)
  {
    auto it = icon.frames.find(role);
    return it == icon.frames.end() ? nullptr : it->second.data();
  }

  // A preview of the gallery in a custom cube: the layers of the icon, like a player's
  void dressSimple(SimplePlayer *player, Loaded const &icon)
  {
    auto first = frameOf(icon, "");
    if (!player || !player->m_firstLayer || !first)
      return;
    player->m_firstLayer->setDisplayFrame(first);
    CCPoint const center = player->m_firstLayer->getContentSize() / 2.f;
    auto layer = [&](CCSprite *sprite, char const *role)
    {
      auto frame = frameOf(icon, role);
      if (!sprite)
        return;
      sprite->setVisible(frame != nullptr);
      if (!frame)
        return;
      sprite->setDisplayFrame(frame);
      sprite->setPosition(center);
    };
    layer(player->m_secondLayer, "2");
    layer(player->m_outlineSprite, "glow");
    layer(player->m_detailSprite, "extra");
  }

  void dressWaiting(std::string const &hash)
  {
    auto waiting = s_waitingPreviews.find(hash);
    auto loaded = s_loaded.find(hash);
    if (waiting == s_waitingPreviews.end() || loaded == s_loaded.end())
      return;
    for (auto &preview : waiting->second)
    {
      if (auto player = preview.lock())
        dressSimple(player, loaded->second);
    }
    s_waitingPreviews.erase(waiting);
  }

  // ! --- Drawing them, as More Icons does (MoreIconsAPI.cpp, updatePlayerObject / updateRobotSprite) --- !

  void dressLayer(PlayerObject *player, Loaded const &icon, bool vehicle, bool ufo)
  {
    auto first = frameOf(icon, "");
    if (!first)
      return;
    auto firstLayer = vehicle ? player->m_vehicleSprite : player->m_iconSprite;
    auto secondLayer = vehicle ? player->m_vehicleSpriteSecondary : player->m_iconSpriteSecondary;
    auto glow = vehicle ? player->m_vehicleGlow : player->m_iconGlow;
    auto detail = vehicle ? player->m_vehicleSpriteWhitener : player->m_iconSpriteWhitener;
    if (!firstLayer)
      return;

    firstLayer->setDisplayFrame(first);
    CCPoint const center = firstLayer->getContentSize() / 2.f;
    if (auto frame = frameOf(icon, "2"); frame && secondLayer)
    {
      secondLayer->setDisplayFrame(frame);
      secondLayer->setPosition(center);
    }
    if (ufo && player->m_birdVehicle)
    {
      if (auto frame = frameOf(icon, "3"))
      {
        player->m_birdVehicle->setDisplayFrame(frame);
        player->m_birdVehicle->setPosition(center);
      }
    }
    if (auto frame = frameOf(icon, "glow"); frame && glow)
      glow->setDisplayFrame(frame);
    if (detail)
    {
      auto extra = frameOf(icon, "extra");
      detail->setVisible(extra != nullptr);
      if (extra)
      {
        detail->setDisplayFrame(extra);
        detail->setPosition(center);
      }
    }
  }

  void dressRobot(GJRobotSprite *sprite, CCSpriteBatchNode *batch, Loaded const &icon)
  {
    if (!sprite || !sprite->m_paSprite)
      return;
    // Every part or none: a part left on the game's sheet would draw from the wrong texture
    for (auto const *role : {"01", "02", "03", "04"})
    {
      if (!frameOf(icon, role))
        return;
    }

    if (batch)
      batch->removeSpriteFromAtlas(sprite);
    sprite->setBatchNode(nullptr);
    sprite->setTexture(icon.texture);
    auto parts = sprite->m_paSprite;
    parts->setBatchNode(nullptr);
    parts->setTexture(icon.texture);

    // Some sprites of a robot may be missing: every array is checked before it is read
    auto at = [](CCArray *array, size_t i) -> CCSprite *
    { return array && i < array->count() ? typeinfo_cast<CCSprite *>(array->objectAtIndex(static_cast<unsigned>(i))) : nullptr; };
    CCArray *spriteParts = parts->m_spriteParts;
    CCArray *seconds = sprite->m_secondArray;
    CCArray *glows = sprite->m_glowSprite ? sprite->m_glowSprite->getChildren() : nullptr;
    for (size_t i = 0; spriteParts && i < spriteParts->count(); ++i)
    {
      auto part = at(spriteParts, i);
      if (!part)
        continue;
      std::string const tag = fmt::format("{:02}", part->getTag());
      part->setBatchNode(nullptr);
      if (auto frame = frameOf(icon, tag))
        part->setDisplayFrame(frame);
      if (auto second = at(seconds, i); second && frameOf(icon, tag + "_2"))
      {
        second->setBatchNode(nullptr);
        second->setDisplayFrame(frameOf(icon, tag + "_2"));
        second->setPosition(part->getContentSize() / 2.f);
      }
      if (auto glow = at(glows, i); glow && frameOf(icon, tag + "_glow"))
      {
        glow->setBatchNode(nullptr);
        glow->setDisplayFrame(frameOf(icon, tag + "_glow"));
      }
      if (part == sprite->m_headSprite)
      {
        auto extra = frameOf(icon, tag + "_extra");
        if (extra)
        {
          if (!sprite->m_extraSprite)
          {
            sprite->m_extraSprite = CCSprite::createWithSpriteFrame(extra);
            part->addChild(sprite->m_extraSprite, 2);
          }
          else
          {
            sprite->m_extraSprite->setBatchNode(nullptr);
            sprite->m_extraSprite->setDisplayFrame(extra);
          }
          sprite->m_extraSprite->setPosition(part->getContentSize() / 2.f);
        }
        if (sprite->m_extraSprite)
          sprite->m_extraSprite->setVisible(extra != nullptr);
      }
    }
    if (batch)
    {
      batch->setTexture(sprite->getTexture());
      batch->appendChild(sprite);
    }
  }
}

// ! --- Ours --- !

std::string const &custom_icons::payload()
{
  return s_payload;
}

void custom_icons::refresh()
{
  if (!sharing())
  {
    s_payload.clear();
    return;
  }
  if (s_uploading)
    return;

  auto uploads = Mod::get()->getSavedValue<matjson::Value>(kUploadsSave, matjson::Value::object());
  std::string payload;
  std::vector<Source> missing;
  for (auto const &kind : kKinds)
  {
    auto source = sourceOf(kind);
    if (!source)
      continue;
    if (auto hash = uploads.get(uploadKey(*source)); hash && hash.unwrap().isString())
    {
      // "" is an icon that can't be shared (too big, an unreadable sheet)
      if (auto const value = hash.unwrap().asString().unwrap(); !value.empty())
        payload += fmt::format("{}{}={}", payload.empty() ? "" : ";", kind.name, value);
      continue;
    }
    missing.push_back(std::move(*source));
  }
  s_payload = payload;
  if (missing.empty())
  {
    // Once a session: nothing to share, and where the cube comes from (to see why)
    static bool told = false;
    if (payload.empty() && !told)
    {
      told = true;
      int const cube = looks::equippedIcon(GameMode::Cube);
      std::string const file = iconFile(fmt::format("icons/player_{:02}", cube), ".png");
      std::string packs;
      if (texture_loader::isLoaded())
      {
        for (auto const &pack : texture_loader::getAppliedPacks())
          packs += fmt::format("{}{} ({})", packs.empty() ? "" : ", ", pack.name, utils::string::pathToString(pack.resourcesPath));
      }
      log::info("Custom icons: none of your icons is custom (the cube {} is {}; packs applied: {})", cube, file,
                packs.empty() ? "none" : packs);
    }
    return;
  }

  // One at a time, then a look again: each one that gets up joins the payload
  auto const &source = missing.front();
  auto body = iconBody(source);
  std::string const key = uploadKey(source);
  if (!body)
  {
    // Not shareable: remembered as nothing, so it isn't read again
    uploads[key] = "";
    Mod::get()->setSavedValue(kUploadsSave, uploads);
    return refresh();
  }
  s_uploading = true;
  log::info("Custom icons: sharing {} ({})", source.name, kindName(source.type));
  gallery::uploadIcon(*body, [key](Result<std::string, std::string> result)
                      {
                        s_uploading = false;
                        if (!result || result.unwrap().empty())
                        {
                          log::warn("Custom icons: can't share an icon: {}", result ? "no hash" : result.unwrapErr());
                          return;
                        }
                        auto saved = Mod::get()->getSavedValue<matjson::Value>(kUploadsSave, matjson::Value::object());
                        saved[key] = result.unwrap();
                        Mod::get()->setSavedValue(kUploadsSave, saved);
                        custom_icons::refresh();
                      });
}

// ! --- Theirs --- !

void custom_icons::setPlayerIcons(int account, std::string const &payload)
{
  if (!Mod::get()->getSettingValue<bool>("show-icons"))
    return;
  auto &icons = s_players[account];
  icons.clear();
  for (auto part : utils::string::split(payload, ";"))
  {
    auto const equals = part.find('=');
    if (equals == std::string::npos)
      continue;
    std::string kind = part.substr(0, equals);
    std::string hash = part.substr(equals + 1);
    // A hash is 64 hex characters; anything else is not an icon
    if (hash.size() != 64 || hash.find_first_not_of("0123456789abcdef") != std::string::npos)
      continue;
    icons[kind] = hash;
    fetch(hash);
  }
  s_dirty = true;
}

void custom_icons::forgetPlayers()
{
  s_players.clear();
  s_failed.clear();
}

void custom_icons::apply(PlayerObject *player, int account)
{
  if (!player || account <= 0 || !Mod::get()->getSettingValue<bool>("show-icons"))
    return;

  // What the icon shows now: the robot or the spider, or the icon sprite (the cube rides the vehicles)
  if (player->m_isRobot)
  {
    if (auto icon = loadedFor(account, IconType::Robot))
      dressRobot(player->m_robotSprite, player->m_robotBatchNode, *icon);
    return;
  }
  if (player->m_isSpider)
  {
    if (auto icon = loadedFor(account, IconType::Spider))
      dressRobot(player->m_spiderSprite, player->m_spiderBatchNode, *icon);
    return;
  }

  IconType const iconType = player->m_isBall ? IconType::Ball : player->m_isDart ? IconType::Wave : player->m_isSwing ? IconType::Swing : IconType::Cube;
  std::string why;
  if (auto icon = loadedFor(account, iconType, &why); !icon)
  {
    static std::set<std::string> toldWhy;
    if (toldWhy.insert(fmt::format("{}|{}|{}", account, kindName(iconType), why)).second)
      log::info("Custom icons: the {} of player {} stays the game's: {}", kindName(iconType), account, why);
  }
  else
  {
    dressLayer(player, *icon, false, false);
    // Once per player and kind: what their icon sprite is now, and where it lives
    static std::set<std::string> told;
    auto sprite = player->m_iconSprite;
    if (sprite && told.insert(fmt::format("{}|{}", account, kindName(iconType))).second)
    {
      auto parent = sprite->getParent();
      log::info("Custom icons: dressed the {} of player {}: sprite {:.0f}x{:.0f} at scale {:.2f}, visible {}, its parent is {}{}", kindName(iconType),
                account, sprite->getContentSize().width, sprite->getContentSize().height, sprite->getScale(), sprite->isVisible(),
                parent ? typeid(*parent).name() : "none", sprite->getBatchNode() ? " (in a batch node)" : "");
    }
  }

  if (player->m_isShip || player->m_isBird)
  {
    IconType const vehicle = player->m_isBird ? IconType::Ufo : player->m_isPlatformer ? IconType::Jetpack : IconType::Ship;
    if (auto icon = loadedFor(account, vehicle))
      dressLayer(player, *icon, true, vehicle == IconType::Ufo);
  }
}

bool custom_icons::isDressed(PlayerObject *player, int account)
{
  if (!player || player->m_isRobot || player->m_isSpider || !Mod::get()->getSettingValue<bool>("show-icons"))
    return true;
  IconType const iconType = player->m_isBall ? IconType::Ball : player->m_isDart ? IconType::Wave : player->m_isSwing ? IconType::Swing : IconType::Cube;
  auto icon = loadedFor(account, iconType);
  // Nothing of theirs to draw, or it is drawn (the game or Globed may put its own frame back)
  return !icon || !player->m_iconSprite || player->m_iconSprite->getTexture() == icon->texture.data();
}

std::vector<std::string> custom_icons::hashesOf(int account)
{
  std::vector<std::string> out;
  if (auto player = s_players.find(account); player != s_players.end())
  {
    for (auto const &[kind, hash] : player->second)
    {
      if (!blocked().contains(hash))
        out.push_back(hash);
    }
  }
  return out;
}

void custom_icons::block(std::string const &hash)
{
  blocked().insert(hash);
  Mod::get()->setSavedValue(kBlockedSave, std::vector<std::string>(blocked().begin(), blocked().end()));
  s_loaded.erase(hash);
  std::error_code error;
  std::filesystem::remove(cacheFile(hash), error);
}

bool custom_icons::takeDirty()
{
  bool const dirty = s_dirty;
  s_dirty = false;
  return dirty;
}

// ! --- Gallery previews --- !

void custom_icons::cubeIconHash(int number, std::string const &custom, std::function<void(std::string)> done)
{
  if (!sharing())
    return done("");
  // That cube: a More Icons icon by its name, else the game's number drawn by a texture pack
  auto source = custom.empty() ? std::nullopt : moreIconsSource(kKinds[0], more_icons::getIcon(custom, IconType::Cube));
  if (!source)
    source = packSource(kKinds[0], number);
  if (!source)
    return done("");
  auto const key = uploadKey(*source);
  auto uploads = Mod::get()->getSavedValue<matjson::Value>(kUploadsSave, matjson::Value::object());
  if (auto hash = uploads.get(key); hash && hash.unwrap().isString())
    return done(hash.unwrap().asString().unwrap());
  auto body = iconBody(*source);
  if (!body)
    return done("");
  log::info("Custom icons: sharing {} for the gallery", source->name);
  gallery::uploadIcon(*body, [key, done = std::move(done)](Result<std::string, std::string> result)
                      {
                        if (!result || result.unwrap().empty())
                        {
                          log::warn("Custom icons: can't share the cube: {}", result ? "no hash" : result.unwrapErr());
                          done("");
                          return;
                        }
                        auto saved = Mod::get()->getSavedValue<matjson::Value>(kUploadsSave, matjson::Value::object());
                        saved[key] = result.unwrap();
                        Mod::get()->setSavedValue(kUploadsSave, saved);
                        done(result.unwrap());
                      });
}

void custom_icons::dressPreview(SimplePlayer *player, std::string const &hash)
{
  if (!player || hash.empty() || blocked().contains(hash))
    return;
  if (auto loaded = s_loaded.find(hash); loaded != s_loaded.end())
  {
    dressSimple(player, loaded->second);
    return;
  }
  s_waitingPreviews[hash].emplace_back(player);
  fetch(hash);
}
