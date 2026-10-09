#include "GalleryApi.hpp"

#include <argon/argon.hpp>

#include <Geode/ui/Notification.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

#include <optional>

using namespace geode::prelude;

namespace
{
  // The gallery server; a saved value can point a build to another one (testing)
  constexpr char const *kServer = "https://icon-mayhem-server-production.up.railway.app";
  constexpr char const *kServerSave = "gallery-url";
  constexpr char const *kSessionSave = "gallery-session"; // per account: {"<account id>": "<session>"}
  constexpr char const *kModeratorSave = "gallery-moderator";

  std::string serverUrl()
  {
    std::string url = Mod::get()->getSavedValue<std::string>(kServerSave, kServer);
    while (!url.empty() && url.back() == '/')
      url.pop_back();
    return url;
  }

  int accountId()
  {
    return GJAccountManager::get()->m_accountID;
  }

  std::string savedSession()
  {
    auto const sessions = Mod::get()->getSavedValue<matjson::Value>(kSessionSave, matjson::Value::object());
    if (auto session = sessions.get(std::to_string(accountId())); session && session.unwrap().isString())
      return session.unwrap().asString().unwrap();
    return "";
  }

  void saveSession(std::string const &session)
  {
    auto sessions = Mod::get()->getSavedValue<matjson::Value>(kSessionSave, matjson::Value::object());
    sessions[std::to_string(accountId())] = session;
    Mod::get()->setSavedValue(kSessionSave, sessions);
  }

  // What came back: the JSON, or the server's message for the player and its code
  struct Reply
  {
    int status = 0;
    matjson::Value json;
    std::string error; // empty when it went fine
    std::string code;  // "session": log in again, "argon": the account check failed
  };

  void request(std::string const &method, std::string const &path, std::optional<matjson::Value> const &body, std::string const &session,
               std::function<void(Reply)> done)
  {
    web::WebRequest req;
    req.timeout(std::chrono::seconds(15));
    req.header("Accept", "application/json");
    if (!session.empty())
      req.header("Authorization", fmt::format("Bearer {}", session));
    if (body)
      req.bodyJSON(*body);

    async::spawn(req.send(method, serverUrl() + path), [done = std::move(done)](web::WebResponse response)
                 {
                   Reply reply;
                   reply.status = response.code();
                   auto json = response.json();
                   if (json)
                     reply.json = std::move(json).unwrap();
                   if (!response.ok())
                   {
                     if (auto error = reply.json.get("error"); error && error.unwrap().isString())
                       reply.error = error.unwrap().asString().unwrap();
                     else if (reply.status <= 0)
                       reply.error = "The gallery can't be reached, check your connection";
                     else
                       reply.error = fmt::format("The gallery answered {}", reply.status);
                     if (auto code = reply.json.get("code"); code && code.unwrap().isString())
                       reply.code = code.unwrap().asString().unwrap();
                   }
                   done(std::move(reply));
                 });
  }

  // Argon proves the account, the server gives a session for it
  void login(std::function<void(std::optional<std::string> error)> done)
  {
    if (accountId() <= 0)
    {
      done("Log in to your Geometry Dash account first (Settings, Account)");
      return;
    }
    Notification::create("Checking your Geometry Dash account...", NotificationIcon::Loading, 2.f)->show();
    async::spawn(argon::startAuth(), [done = std::move(done)](Result<std::string> token)
                 {
                   if (!token)
                   {
                     done(fmt::format("The account check failed: {}", token.unwrapErr()));
                     return;
                   }
                   auto body = matjson::Value::object();
                   body["accountId"] = accountId();
                   body["username"] = std::string(GJAccountManager::get()->m_username);
                   body["token"] = std::move(token).unwrap();
                   request("POST", "/v1/auth/login", body, "", [done](Reply reply)
                           {
                             if (!reply.error.empty())
                             {
                               // A token Argon doesn't take anymore: a fresh one next time
                               if (reply.code == "argon")
                                 argon::clearToken();
                               done(reply.error);
                               return;
                             }
                             auto session = reply.json.get("session");
                             if (!session || !session.unwrap().isString())
                             {
                               done("The gallery didn't log you in");
                               return;
                             }
                             saveSession(session.unwrap().asString().unwrap());
                             bool admin = false;
                             if (auto value = reply.json.get("admin"); value && value.unwrap().isBool())
                               admin = value.unwrap().asBool().unwrap();
                             Mod::get()->setSavedValue(kModeratorSave, admin);
                             done(std::nullopt);
                           });
                 });
  }

  // A request that needs the account: logs in first if needed, and once more if the session ran out
  void withAccount(std::string const &method, std::string const &path, std::optional<matjson::Value> const &body,
                   std::function<void(Reply)> done, bool retried = false)
  {
    std::string const session = savedSession();
    if (session.empty())
    {
      login([=](std::optional<std::string> error)
            {
              if (error)
              {
                Reply reply;
                reply.error = *error;
                done(std::move(reply));
                return;
              }
              withAccount(method, path, body, done, true);
            });
      return;
    }
    request(method, path, body, session, [=](Reply reply)
            {
              if (reply.code == "session" && !retried)
              {
                saveSession("");
                withAccount(method, path, body, done, true);
                return;
              }
              done(std::move(reply));
            });
  }

  gallery::Look parseLook(matjson::Value const &json)
  {
    gallery::Look look;
    look.id = json["id"].asInt().unwrapOr(0);
    look.name = json["name"].asString().unwrapOr("");
    look.authorId = json["author"]["accountId"].asInt().unwrapOr(0);
    look.author = json["author"]["username"].asString().unwrapOr("");
    look.likes = json["likes"].asInt().unwrapOr(0);
    look.liked = json["liked"].asBool().unwrapOr(false);
    look.mine = json["mine"].asBool().unwrapOr(false);
    look.hidden = json["hidden"].asBool().unwrapOr(false);
    look.look = json["look"].isObject() ? json["look"] : matjson::Value::object();
    return look;
  }
}

bool gallery::available()
{
  return !serverUrl().empty();
}

bool gallery::isModerator()
{
  return Mod::get()->getSavedValue<bool>(kModeratorSave, false);
}

void gallery::list(std::string const &sort, std::string const &query, int page, Done<Page> done)
{
  std::string path = fmt::format("/v1/looks?sort={}&page={}&limit=6", sort, page);
  if (!query.empty())
  {
    // Letters and digits only in the search: nothing to escape
    std::string clean;
    for (char c : query)
    {
      if (std::isalnum(static_cast<unsigned char>(c)) || c == ' ')
        clean += c == ' ' ? '+' : c;
    }
    path += fmt::format("&q={}", clean);
  }
  auto handle = [done](Reply reply)
  {
    if (!reply.error.empty())
    {
      done(Err(reply.error));
      return;
    }
    Page page;
    if (auto looks = reply.json.get("looks"); looks && looks.unwrap().isArray())
    {
      for (auto const &json : looks.unwrap())
        page.looks.push_back(parseLook(json));
    }
    page.hasMore = reply.json["hasMore"].asBool().unwrapOr(false);
    done(Ok(std::move(page)));
  };
  // Your own looks need the account, the gallery doesn't (the session only marks your likes)
  if (sort == "mine")
    withAccount("GET", path, std::nullopt, handle);
  else
    request("GET", path, std::nullopt, savedSession(), handle);
}

void gallery::publish(std::string const &name, matjson::Value const &look, Done<Look> done)
{
  auto body = matjson::Value::object();
  body["name"] = name;
  body["look"] = look;
  withAccount("POST", "/v1/looks", body, [done](Reply reply)
              {
                if (!reply.error.empty())
                  done(Err(reply.error));
                else
                  done(Ok(parseLook(reply.json)));
              });
}

void gallery::like(int id, bool on, Done<int> done)
{
  withAccount(on ? "POST" : "DELETE", fmt::format("/v1/looks/{}/like", id), std::nullopt, [done](Reply reply)
              {
                if (!reply.error.empty())
                  done(Err(reply.error));
                else
                  done(Ok(reply.json["likes"].asInt().unwrapOr(0)));
              });
}

void gallery::report(int id, Done<bool> done)
{
  withAccount("POST", fmt::format("/v1/looks/{}/report", id), matjson::Value::object(), [done](Reply reply)
              {
                if (!reply.error.empty())
                  done(Err(reply.error));
                else
                  done(Ok(true));
              });
}

void gallery::remove(int id, Done<bool> done)
{
  // A moderator removes through the admin route (it also works for looks that are not theirs)
  std::string const path = isModerator() ? fmt::format("/v1/admin/looks/{}/delete", id) : fmt::format("/v1/looks/{}", id);
  withAccount(isModerator() ? "POST" : "DELETE", path, std::nullopt, [done](Reply reply)
              {
                if (!reply.error.empty())
                  done(Err(reply.error));
                else
                  done(Ok(true));
              });
}
