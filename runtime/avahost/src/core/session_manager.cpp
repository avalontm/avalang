#include "core/session_manager.h"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace avahost {

SessionManager::SessionManager(int ttlSeconds) : ttlSeconds_(ttlSeconds) {}

std::string SessionManager::GenerateSessionId() {
    // rd_() returns one 32-bit word per call; four calls -> 128 bits.
    // Not seeding a PRNG with these -- each word is used directly, so
    // every bit comes straight from random_device's own source (see
    // the header comment on why that's the right entropy source here).
    std::array<uint32_t, 4> words{rd_(), rd_(), rd_(), rd_()};

    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (uint32_t w : words) {
        hex << std::setw(8) << w;
    }
    return hex.str();
}

std::string SessionManager::ResolveSession(const std::string& cookieSessionId, bool& outIsNew) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!cookieSessionId.empty()) {
        auto it = sessions_.find(cookieSessionId);
        if (it != sessions_.end()) {
            it->second.lastAccess = std::chrono::steady_clock::now();
            outIsNew = false;
            return cookieSessionId;
        }
        // Cookie present but names no live session (expired and
        // reaped, or a value nobody server-side ever issued, e.g. a
        // stale cookie from a previous run of the dev server). Falls
        // through to issuing a fresh one -- same "just start a new
        // session" behavior as no cookie at all, never an error.
    }

    std::string id;
    do {
        id = GenerateSessionId();
        // Collision odds against 128 bits of entropy are astronomically
        // low, but the check is nearly free and turns "astronomically
        // low" into "impossible" -- worth keeping.
    } while (sessions_.find(id) != sessions_.end());

    Session& session = sessions_[id];
    session.lastAccess = std::chrono::steady_clock::now();
    outIsNew = true;
    return id;
}

bool SessionManager::TryGetState(const std::string& sessionId, const std::string& routeFilePath,
                                  std::string& outState) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto sessionIt = sessions_.find(sessionId);
    if (sessionIt == sessions_.end()) return false;
    auto stateIt = sessionIt->second.stateCache.find(routeFilePath);
    if (stateIt == sessionIt->second.stateCache.end()) return false;
    outState = stateIt->second;
    return true;
}

void SessionManager::SetState(const std::string& sessionId, const std::string& routeFilePath,
                               const std::string& stateJson) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto sessionIt = sessions_.find(sessionId);
    if (sessionIt == sessions_.end()) {
        // Session expired/was reaped between ResolveSession and this
        // call (e.g. a very short TTL plus a slow render). Silently
        // dropping the write matches the old code's simplicity level --
        // the next request just starts a fresh session via
        // ResolveSession, same as any other first visit.
        return;
    }
    sessionIt->second.stateCache[routeFilePath] = stateJson;
}

void SessionManager::EraseStateForFile(const std::string& routeFilePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, session] : sessions_) {
        session.stateCache.erase(routeFilePath);
    }
}

void SessionManager::ReapExpired() {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();
    const auto ttl = std::chrono::seconds(ttlSeconds_);
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (now - it->second.lastAccess > ttl) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace avahost
