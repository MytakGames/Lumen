#include "instances/InstanceManager.h"

#include <shellapi.h>

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <thread>

#include "core/Logger.h"
#include "core/StringUtil.h"
#include "instances/RobloxLocator.h"

namespace rl::instances {
namespace {

constexpr int kDetectTimeoutMs = 90000;  // first launch may include a client update
constexpr int kPollMs = 500;

// Sleeps in small steps so shutdown is not held up.
void sleepWhileAlive(const AliveFlag& alive, int ms) {
    for (int waited = 0; waited < ms && *alive; waited += 100) std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

std::string randomDigits(int n) {
    std::string hex = str::randomHex(static_cast<size_t>(n));
    std::string out;
    for (int i = 0; i < n; ++i) out.push_back(static_cast<char>('0' + (static_cast<unsigned char>(hex[static_cast<size_t>(i)]) % 10)));
    if (out[0] == '0') out[0] = '1';
    return out;
}

struct CloseCtx {
    DWORD pid;
    int posted = 0;
};

BOOL CALLBACK closeWindowsOf(HWND hwnd, LPARAM lp) {
    auto* ctx = reinterpret_cast<CloseCtx*>(lp);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == ctx->pid && IsWindowVisible(hwnd)) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        ++ctx->posted;
    }
    return TRUE;
}

}  // namespace

// --- MultiInstanceGuard ----------------------------------------------------------

ErrorCode MultiInstanceGuard::acquire() {
    if (mutex_) return ErrorCode::None;
    // Roblox clients check these named objects to enforce a single client.
    // If they already exist, a Roblox client is running and owns them.
    HANDLE m = CreateMutexW(nullptr, TRUE, L"ROBLOX_singletonMutex");
    const DWORD mErr = GetLastError();
    HANDLE e = CreateEventW(nullptr, FALSE, FALSE, L"ROBLOX_singletonEvent");
    const DWORD eErr = GetLastError();
    if (!m || mErr == ERROR_ALREADY_EXISTS || eErr == ERROR_ALREADY_EXISTS) {
        if (m) CloseHandle(m);
        if (e) CloseHandle(e);
        log::warn("instances", "Multi-instance unavailable: a Roblox client already owns the singleton objects");
        return ErrorCode::MultiInstanceUnavailable;
    }
    mutex_ = m;
    event_ = e;
    log::info("instances", "Multi-instance guard acquired");
    lockCookieFile();
    return ErrorCode::None;
}

void MultiInstanceGuard::lockCookieFile() {
    if (cookieFileLocked()) return;
    const auto file = robloxCookieFile();
    std::error_code ec;
    if (file.empty() || !std::filesystem::exists(file, ec)) {
        log::info("instances", "RobloxCookies.dat not found, nothing to lock");
        return;
    }
    // Read sharing only: clients may still read it, but none can rewrite it.
    cookieFile_ = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (cookieFileLocked()) {
        log::info("instances", "RobloxCookies.dat locked");
    } else {
        // Another tool may hold it already; multi-instance still works, just without this protection.
        log::warn("instances", "Could not lock RobloxCookies.dat (error " + std::to_string(GetLastError()) + ")");
    }
}

void MultiInstanceGuard::release() {
    if (cookieFileLocked()) {
        CloseHandle(cookieFile_);
        cookieFile_ = INVALID_HANDLE_VALUE;
        log::info("instances", "RobloxCookies.dat unlocked");
    }
    if (mutex_) {
        ReleaseMutex(mutex_);
        CloseHandle(mutex_);
        mutex_ = nullptr;
    }
    if (event_) {
        CloseHandle(event_);
        event_ = nullptr;
    }
}

// --- process exit watcher ----------------------------------------------------------

struct InstanceManager::Waiter {
    int id = 0;
    HANDLE process = nullptr;
    HANDLE wait = nullptr;
    MainThreadQueue* main = nullptr;
    InstanceManager* owner = nullptr;
    std::weak_ptr<std::atomic<bool>> alive;

    ~Waiter() {
        if (wait) UnregisterWaitEx(wait, INVALID_HANDLE_VALUE);  // blocks until a running callback returns
        if (process) CloseHandle(process);
    }

    static void CALLBACK onExit(PVOID ctx, BOOLEAN) {
        auto* w = static_cast<Waiter*>(ctx);
        const int id = w->id;
        InstanceManager* owner = w->owner;
        auto alive = w->alive;
        w->main->post([alive, owner, id] {
            if (const auto a = alive.lock(); a && *a) owner->onEnded(id);
        });
    }
};

// --- InstanceManager -----------------------------------------------------------------

InstanceManager::InstanceManager(accounts::AccountService& accounts, api::RobloxApi& api, ThreadPool& pool,
                                 MainThreadQueue& main, Notify notify, AliveFlag alive)
    : accounts_(accounts), api_(api), pool_(pool), main_(main), notify_(std::move(notify)), alive_(std::move(alive)) {}

InstanceManager::~InstanceManager() { waiters_.clear(); }

void InstanceManager::configure(bool multiInstance, int maxInstances, int launchDelayMs) {
    multi_ = multiInstance;
    maxInstances_ = maxInstances;
    launchDelayMs_ = launchDelayMs;
    if (!multi_) guard_.release();
}

int InstanceManager::activeCount() const {
    return static_cast<int>(std::count_if(instances_.begin(), instances_.end(), [](const Instance& i) {
        return i.status == InstanceStatus::Launching || i.status == InstanceStatus::Running;
    }));
}

bool InstanceManager::accountActive(const std::string& accountId) const {
    return std::any_of(instances_.begin(), instances_.end(), [&](const Instance& i) {
        return i.accountId == accountId &&
               (i.status == InstanceStatus::Launching || i.status == InstanceStatus::Running);
    });
}

Instance* InstanceManager::find(int id) {
    const auto it = std::find_if(instances_.begin(), instances_.end(), [&](const Instance& i) { return i.id == id; });
    return it == instances_.end() ? nullptr : &*it;
}

ErrorCode InstanceManager::check(const LaunchRequest& req) const {
    const accounts::Account* a = accounts_.find(req.accountId);
    if (!a) return ErrorCode::Unauthorized;
    if (!a->enabled) return ErrorCode::AccountDisabled;
    if (!a->hasSession()) return ErrorCode::Unauthorized;
    if (!locateRoblox().installed) return ErrorCode::RobloxNotInstalled;
    if (activeCount() >= maxInstances()) return ErrorCode::InstanceLimit;
    if (!multi_) {
        // Without the multi-instance guard a new client would replace the running one.
        const auto external = runningPlayerPids();
        if (!external.empty()) return ErrorCode::InstanceLimit;
    }
    if (!req.allowSameAccount && accountActive(req.accountId)) return ErrorCode::AccountBusy;
    return ErrorCode::None;
}

void InstanceManager::launch(LaunchRequest req, std::function<void(ErrorCode)> done) {
    if (const ErrorCode e = check(req); e != ErrorCode::None) {
        done(e);
        return;
    }
    if (multi_) {
        if (const ErrorCode g = guard_.acquire(); g != ErrorCode::None) {
            done(g);
            return;
        }
    }
    auto cookie = accounts_.cookieFor(req.accountId);
    if (!cookie) {
        done(cookie.error);
        return;
    }

    Instance inst;
    inst.id = nextId_++;
    inst.accountId = req.accountId;
    inst.accountLabel = accounts_.find(req.accountId)->label();
    inst.placeId = req.placeId;
    inst.universeId = req.universeId;
    inst.gameName = req.gameName;
    inst.startedAt = std::time(nullptr);
    instances_.push_back(inst);
    accounts_.markUsed(req.accountId);
    log::info("instances", "Launching instance #" + std::to_string(inst.id) + " place " + std::to_string(req.placeId));

    const int id = inst.id;
    const int delayMs = launchDelayMs_;
    auto secret = std::make_shared<SecureString>(std::move(*cookie.value));
    auto finish = [this, id, done, alive = alive_](ErrorCode e, DWORD pid) {
        main_.post([this, id, done, alive, e, pid] {
            if (!*alive) return;
            Instance* i = find(id);
            if (i) {
                if (e == ErrorCode::None) {
                    onStarted(id, pid);
                } else {
                    i->status = InstanceStatus::Failed;
                    i->error = e;
                    i->endedAt = std::time(nullptr);
                    if (e == ErrorCode::Unauthorized) accounts_.markExpired(i->accountId);
                }
            }
            done(e);
        });
    };

    pool_.post([this, req, secret, finish, delayMs, alive = alive_, mtx = launchMutex_, last = lastLaunch_] {
        std::lock_guard lock(*mtx);
        const auto since = std::chrono::steady_clock::now() - *last;
        const auto wait = std::chrono::milliseconds(delayMs) - std::chrono::duration_cast<std::chrono::milliseconds>(since);
        if (wait.count() > 0 && last->time_since_epoch().count() != 0) sleepWhileAlive(alive, static_cast<int>(wait.count()));
        if (!*alive) return;

        auto ticket = api_.authTicket(*secret, req.placeId);
        secret->wipe();
        if (!ticket) {
            log::warn("instances", std::string("Auth ticket failed: ") + errorName(ticket.error));
            finish(ticket.error, 0);
            return;
        }

        const auto before = runningPlayerPids();
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
        std::wstring uri = str::toWide(buildLaunchUri(ticket.value->reveal(), req.placeId, nowMs, randomDigits(12),
                                                      str::uuid4(), "en_us"));
        ticket.value->wipe();

        SHELLEXECUTEINFOW sei{};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
        sei.lpVerb = L"open";
        sei.lpFile = uri.c_str();
        sei.nShow = SW_SHOWNORMAL;
        const BOOL ok = ShellExecuteExW(&sei);
        SecureZeroMemory(uri.data(), uri.size() * sizeof(wchar_t));
        *last = std::chrono::steady_clock::now();
        if (!ok) {
            log::error("instances", "ShellExecuteEx(roblox-player:) failed: " + std::to_string(GetLastError()));
            finish(ErrorCode::LaunchFailed, 0);
            return;
        }

        for (int waited = 0; waited < kDetectTimeoutMs && *alive; waited += kPollMs) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
            for (const DWORD pid : runningPlayerPids()) {
                if (std::find(before.begin(), before.end(), pid) == before.end()) {
                    finish(ErrorCode::None, pid);
                    return;
                }
            }
        }
        if (*alive) {
            log::warn("instances", "No new RobloxPlayerBeta.exe process detected");
            finish(ErrorCode::LaunchFailed, 0);
        }
    });
}

void InstanceManager::onStarted(int id, DWORD pid) {
    Instance* i = find(id);
    if (!i) return;
    i->status = InstanceStatus::Running;
    i->pid = pid;
    log::info("instances", "Instance #" + std::to_string(id) + " running as pid " + std::to_string(pid));
    watch(id, pid);
}

void InstanceManager::watch(int id, DWORD pid) {
    auto w = std::make_unique<Waiter>();
    w->id = id;
    w->main = &main_;
    w->owner = this;
    w->alive = alive_;
    w->process = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!w->process) {
        onEnded(id);
        return;
    }
    if (!RegisterWaitForSingleObject(&w->wait, w->process, &Waiter::onExit, w.get(), INFINITE, WT_EXECUTEONLYONCE)) {
        w->wait = nullptr;
        log::warn("instances", "RegisterWaitForSingleObject failed");
    }
    waiters_.push_back(std::move(w));
}

void InstanceManager::onEnded(int id) {
    if (Instance* i = find(id); i && i->status != InstanceStatus::Ended) {
        i->status = InstanceStatus::Ended;
        i->endedAt = std::time(nullptr);
        log::info("instances", "Instance #" + std::to_string(id) + " ended");
    }
    waiters_.erase(std::remove_if(waiters_.begin(), waiters_.end(),
                                  [id](const std::unique_ptr<Waiter>& w) { return w->id == id; }),
                   waiters_.end());
    if (activeCount() == 0 && runningPlayerPids().empty()) guard_.release();
}

void InstanceManager::close(int instanceId) {
    Instance* i = find(instanceId);
    if (!i || i->status != InstanceStatus::Running || i->pid == 0) return;
    const DWORD pid = i->pid;
    pool_.post([pid] {
        CloseCtx ctx{pid};
        EnumWindows(&closeWindowsOf, reinterpret_cast<LPARAM>(&ctx));
        HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid);
        if (!h) return;
        if (ctx.posted == 0 || WaitForSingleObject(h, 4000) == WAIT_TIMEOUT) {
            log::info("instances", "Terminating pid " + std::to_string(pid));
            TerminateProcess(h, 0);
        }
        CloseHandle(h);
    });
}

void InstanceManager::clearFinished() {
    instances_.erase(std::remove_if(instances_.begin(), instances_.end(),
                                    [](const Instance& i) {
                                        return i.status == InstanceStatus::Ended || i.status == InstanceStatus::Failed;
                                    }),
                     instances_.end());
}

}  // namespace rl::instances
