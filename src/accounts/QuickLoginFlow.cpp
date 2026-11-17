#include "accounts/QuickLoginFlow.h"

#include <windows.h>

#include <chrono>
#include <thread>

#include "core/Logger.h"

namespace rl::accounts {
namespace {
constexpr int kPollMs = 2000;
constexpr int kMaxMinutes = 10;  // safety net if the server gives no expiry
}  // namespace

QuickLoginFlow::QuickLoginFlow(api::RobloxApi& api, ThreadPool& pool, MainThreadQueue& main, AccountService& accounts,
                               AliveFlag alive, std::function<void(const std::string&)> openBrowser)
    : api_(api),
      pool_(pool),
      main_(main),
      accounts_(accounts),
      alive_(std::move(alive)),
      openBrowser_(std::move(openBrowser)) {}

void QuickLoginFlow::start(const std::string& expectedAccountId, Done done) {
    if (busy()) return;
    const int gen = ++generation_;
    stage_ = Stage::Requesting;
    error_ = ErrorCode::None;
    accountName_.clear();
    expected_ = expectedAccountId;
    done_ = std::move(done);
    code_ = {};

    pool_.post([this, gen, alive = alive_] {
        auto res = api_.quickLoginCreate();
        main_.post([this, gen, alive, res = std::move(res)] {
            if (!*alive || gen != generation_) return;
            if (!res) return fail(res.error);
            code_ = *res.value;
            if (code_.expires == 0) code_.expires = std::time(nullptr) + kMaxMinutes * 60;
            stage_ = Stage::WaitingCode;
            log::info("login", "Quick login code issued, opening browser");
            reopenBrowser();
            schedulePoll(gen);
        });
    });
}

void QuickLoginFlow::schedulePoll(int gen) {
    const api::QuickLoginCode code = code_;
    pool_.post([this, gen, code, alive = alive_] {
        for (int waited = 0; waited < kPollMs && *alive; waited += 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!*alive) return;
        auto res = api_.quickLoginStatus(code);
        main_.post([this, gen, alive, res = std::move(res)] {
            if (!*alive || gen != generation_ || !busy()) return;
            if (std::time(nullptr) > code_.expires) return fail(ErrorCode::LoginExpired);
            if (!res) {
                // Transient network trouble: keep polling until the code expires.
                if (res.error == ErrorCode::Network || res.error == ErrorCode::Timeout ||
                    res.error == ErrorCode::RateLimited) {
                    return schedulePoll(gen);
                }
                return fail(res.error);
            }
            if (!res.value->accountName.empty()) accountName_ = res.value->accountName;
            switch (res.value->status) {
                case api::QuickLoginStatus::Created:
                    schedulePoll(gen);
                    break;
                case api::QuickLoginStatus::UserLinked:
                    stage_ = Stage::WaitingConfirm;
                    schedulePoll(gen);
                    break;
                case api::QuickLoginStatus::Validated:
                    finish(gen);
                    break;
                case api::QuickLoginStatus::Cancelled:
                    fail(ErrorCode::Cancelled);
                    break;
                case api::QuickLoginStatus::Invalid:
                    fail(ErrorCode::LoginExpired);
                    break;
            }
        });
    });
}

void QuickLoginFlow::finish(int gen) {
    stage_ = Stage::Finishing;
    const api::QuickLoginCode code = code_;
    pool_.post([this, gen, code, alive = alive_] {
        auto cookie = api_.quickLoginComplete(code);
        auto shared = cookie ? std::make_shared<SecureString>(std::move(*cookie.value)) : nullptr;
        const ErrorCode error = cookie.error;
        main_.post([this, gen, alive, shared, error] {
            if (!*alive || gen != generation_) return;
            if (!shared) return fail(error);
            log::info("login", "Quick login confirmed, validating session");
            accounts_.addFromCookie(std::move(*shared), [this, gen, alive](ErrorCode e, const std::string& id) {
                if (!*alive || gen != generation_) return;
                if (e != ErrorCode::None) return fail(e);
                stage_ = Stage::Done;
                SecureZeroMemory(code_.privateKey.data(), code_.privateKey.size());
                code_.privateKey.clear();
                if (done_) done_(ErrorCode::None, id);
            });
        });
    });
}

void QuickLoginFlow::fail(ErrorCode e) {
    log::warn("login", std::string("Quick login failed: ") + errorName(e));
    stage_ = Stage::Failed;
    error_ = e;
    SecureZeroMemory(code_.privateKey.data(), code_.privateKey.size());
    code_.privateKey.clear();
    if (done_) done_(e, {});
}

void QuickLoginFlow::cancel() {
    if (!visible()) return;
    ++generation_;
    SecureZeroMemory(code_.privateKey.data(), code_.privateKey.size());
    code_ = {};
    stage_ = Stage::Idle;
    error_ = ErrorCode::Cancelled;
    log::info("login", "Quick login cancelled by the user");
}

void QuickLoginFlow::dismiss() {
    if (!busy()) {
        stage_ = Stage::Idle;
        code_ = {};
    }
}

void QuickLoginFlow::reopenBrowser() const {
    if (openBrowser_) openBrowser_(kConfirmUrl);
}

}  // namespace rl::accounts
