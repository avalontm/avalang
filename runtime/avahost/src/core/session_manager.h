#pragma once
// AvaHost.Core -- per-browser session store.
//
// Before this file existed, app.h's `stateCache_` was a single
// `unordered_map<routeFilePath, stateJson>` shared by every client
// that ever hit the server -- explicitly "NOT per-session/per-user (no
// cookies involved)" per its old comment. That meant two different
// browsers (or two tabs in the same browser in a different profile,
// or a browser + curl) hitting the same page read and mutated the
// exact same state slot: page state leaked across unrelated visitors
// instead of being private to each one.
//
// SessionManager fixes that by keying state one level deeper, behind
// a per-browser session id carried in a cookie (see kSessionCookieName
// in app.cpp). Each session gets its own copy of the old
// routeFilePath -> stateJson map, so state changes in one browser
// never appear in another. Sessions are held in memory only (same
// "lost on process restart" tradeoff the old single-map cache had --
// no new persistence requirement introduced here) and expire on a
// sliding TTL: any access (GET or event dispatch) refreshes the
// session's lastAccess, and an unmodified TtlSeconds() thread-safe
// reap sweeps sessions nobody has touched in that long. This bounds
// memory from visitors who never come back, without depending on
// clients to ever explicitly log out.
#include <chrono>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>

namespace avahost {

class SessionManager {
public:
    // ttlSeconds: how long a session survives with no requests before
    // ReapExpired() may drop it. Sourced from HostOptions::
    // sessionTtlSeconds (itself overridable via appsettings.json's
    // "sessionTtlSeconds" -- see config/app_config.cpp), never
    // hardcoded past this one configurable default.
    explicit SessionManager(int ttlSeconds);

    // Looks up `cookieSessionId` (the value AvaHostApp parsed out of
    // the request's `avahost_session` cookie, or empty if the request
    // had none/an unrecognized one). If it names a live, unexpired
    // session, that session's access time is refreshed and its id is
    // returned with `outIsNew` set to false. Otherwise a brand new
    // session is created (fresh, empty state cache) and its id is
    // returned with `outIsNew` set to true -- the caller (app.cpp)
    // uses that to know it must send a Set-Cookie for this response so
    // the *next* request from this browser carries the same id.
    std::string ResolveSession(const std::string& cookieSessionId, bool& outIsNew);

    // Per-session equivalent of the old stateCache_[routeFilePath]
    // lookup/store. `sessionId` must be one already returned by
    // ResolveSession -- an unknown id (e.g. the session expired
    // between ResolveSession and this call, in a future concurrent
    // caller) is treated as "no cached state" / a silent no-op store,
    // never a crash.
    bool TryGetState(const std::string& sessionId, const std::string& routeFilePath,
                      std::string& outState) const;
    void SetState(const std::string& sessionId, const std::string& routeFilePath,
                  const std::string& stateJson);

    // Called by AvaHostApp's hot-reload watcher when a route file
    // changes on disk. The old code only had one shared slot to erase
    // (stateCache_.erase(path)); now the same edit has to be forgotten
    // in *every* live session, or a browser that edited the page's
    // state before the edit would keep seeing its stale pre-edit
    // values forever.
    void EraseStateForFile(const std::string& routeFilePath);

    // Drops every session whose lastAccess is older than TtlSeconds().
    // Cheap to call periodically (see app.cpp's session-reaper
    // background thread) -- a no-op pass over an empty/small map costs
    // nothing worth avoiding.
    void ReapExpired();

    int TtlSeconds() const { return ttlSeconds_; }

private:
    struct Session {
        std::unordered_map<std::string, std::string> stateCache; // routeFilePath -> state JSON
        std::chrono::steady_clock::time_point lastAccess;
    };

    // 128 bits from a CSPRNG-backed source, hex-encoded (32 chars).
    // std::random_device is implementation-defined by the standard,
    // but every mainstream libstdc++/libc++/MSVC implementation wires
    // it to the OS's actual entropy source (getrandom(2)/
    // /dev/urandom on Linux, CryptGenRandom/BCryptGenRandom on
    // Windows, arc4random on macOS) rather than a seeded PRNG, so it's
    // the right building block here without AvaHost needing its own
    // per-platform CSPRNG bindings. 128 bits keeps brute-force/
    // collision guessing infeasible for a session identifier (same
    // order of magnitude as a UUID v4's 122 random bits).
    std::string GenerateSessionId();

    int ttlSeconds_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Session> sessions_;
    std::random_device rd_;
};

} // namespace avahost
