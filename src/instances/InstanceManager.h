#pragma once

#include <windows.h>

#include <chrono>
#include <cstdint>
#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "accounts/AccountService.h"
#include "core/Error.h"
#include "core/Notice.h"
#include "core/ThreadPool.h"
#include "robloxapi/RobloxApi.h"

namespace rl::instances {

enum class InstanceStatus { Launching, Running, Ended, Failed };

struct Instance {
    int id = 0;
    std::string accountId;
    std::string accountLabel;
    std::int64_t placeId = 0;
    std::int64_t universeId = 0;
    std::string gameName;
    InstanceStatus status = InstanceStatus::Launching;
    DWORD pid = 0;
    std::time_t startedAt = 0;
    std::time_t endedAt = 0;
    ErrorCode error = ErrorCode::None;
};

struct LaunchRequest {
    std::string accountId;
    std::int64_t placeId = 0;
    std::int64_t universeId = 0;
    std::string gameName;
    bool allowSameAccount = false;  // user confirmed a second instance of the same account
};

// Holds the Roblox single-instance objects so additional clients can start.
// Must be acquired before the first Roblox client is running.
//
// It also keeps %LOCALAPPDATA%\Roblox\LocalStorage\RobloxCookies.dat open
// without write sharing: all clients share that file, and a client rewriting
// it can sign the other running instances out. Each instance started by the
// launcher receives its own one-time auth ticket, so clients don't need to
// write it.
class MultiInstanceGuard {
public:
    ~MultiInstanceGuard() { release(); }
    ErrorCode acquire();  // MultiInstanceUnavailable if Roblox already owns them
    void release();
    bool active() const { return mutex_ != nullptr; }
    bool cookieFileLocked() const { return cookieFile_ != INVALID_HANDLE_VALUE; }

private:
    void lockCookieFile();

    HANDLE mutex_ = nullptr;
    HANDLE event_ = nullptr;
    HANDLE cookieFile_ = INVALID_HANDLE_VALUE;
};

// Starts Roblox clients through the roblox-player: protocol, associates each
// with the new RobloxPlayerBeta.exe process, and tracks it until it exits.
// UI-thread API; blocking steps run on the pool.
class InstanceManager {
public:
    InstanceManager(accounts::AccountService& accounts, api::RobloxApi& api, ThreadPool& pool, MainThreadQueue& main,
                    Notify notify, AliveFlag alive);
    ~InstanceManager();

    void configure(bool multiInstance, int maxInstances, int launchDelayMs);

    // Synchronous pre-checks (no network). None when the launch may proceed.
    ErrorCode check(const LaunchRequest& req) const;
    // Runs check(), then the launch pipeline. `done` reports the final outcome.
    void launch(LaunchRequest req, std::function<void(ErrorCode)> done);
    void close(int instanceId);
    void clearFinished();

    const std::vector<Instance>& instances() const { return instances_; }
    int activeCount() const;
    bool accountActive(const std::string& accountId) const;
    int maxInstances() const { return multi_ ? maxInstances_ : 1; }

private:
    struct Waiter;
    Instance* find(int id);
    void onStarted(int id, DWORD pid);
    void onEnded(int id);
    void watch(int id, DWORD pid);

    accounts::AccountService& accounts_;
    api::RobloxApi& api_;
    ThreadPool& pool_;
    MainThreadQueue& main_;
    Notify notify_;
    AliveFlag alive_;

    std::vector<Instance> instances_;
    std::vector<std::unique_ptr<Waiter>> waiters_;
    int nextId_ = 1;
    bool multi_ = false;
    int maxInstances_ = 4;
    int launchDelayMs_ = 3000;
    MultiInstanceGuard guard_;

    // Launches are serialized so each new process can be attributed correctly.
    std::shared_ptr<std::mutex> launchMutex_ = std::make_shared<std::mutex>();
    std::shared_ptr<std::chrono::steady_clock::time_point> lastLaunch_ =
        std::make_shared<std::chrono::steady_clock::time_point>();
};

}  // namespace rl::instances
