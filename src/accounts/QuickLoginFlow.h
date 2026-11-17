#pragma once

#include <ctime>
#include <functional>
#include <string>

#include "accounts/AccountService.h"
#include "core/Error.h"
#include "core/ThreadPool.h"
#include "robloxapi/RobloxApi.h"

namespace rl::accounts {

// Sign-in through Roblox's "log in with another device" flow:
//   1. the launcher asks Roblox for a short code;
//   2. the user's default browser opens roblox.com, where they (already
//      signed in there, or signing in normally) enter the code and confirm
//      "Do you want to log in to …?";
//   3. the launcher sees the confirmation and receives the session, which is
//      encrypted and stored by AccountService.
// The launcher never sees a password and never fills any login form.
// UI-thread API; network calls run on the pool.
class QuickLoginFlow {
public:
    static constexpr char kConfirmUrl[] = "https://www.roblox.com/crossdevicelogin/ConfirmCode";

    enum class Stage {
        Idle,
        Requesting,   // asking Roblox for a code
        WaitingCode,  // code shown; user has not entered it yet
        WaitingConfirm,  // code entered on roblox.com; waiting for "Confirm"
        Finishing,    // exchanging the code for the session
        Done,
        Failed,
    };

    using Done = std::function<void(ErrorCode, const std::string& accountId)>;

    QuickLoginFlow(api::RobloxApi& api, ThreadPool& pool, MainThreadQueue& main, AccountService& accounts,
                   AliveFlag alive, std::function<void(const std::string& url)> openBrowser);

    // `expectedAccountId` is set when refreshing an existing account's session.
    void start(const std::string& expectedAccountId, Done done);
    void cancel();
    void reopenBrowser() const;
    void dismiss();  // hide a finished (Done/Failed) dialog

    Stage stage() const { return stage_; }
    bool visible() const { return stage_ != Stage::Idle; }
    bool busy() const { return stage_ != Stage::Idle && stage_ != Stage::Done && stage_ != Stage::Failed; }
    const std::string& code() const { return code_.code; }
    const std::string& accountName() const { return accountName_; }
    std::time_t expires() const { return code_.expires; }
    ErrorCode error() const { return error_; }
    const std::string& expectedAccountId() const { return expected_; }

private:
    void schedulePoll(int generation);
    void fail(ErrorCode e);
    void finish(int generation);

    api::RobloxApi& api_;
    ThreadPool& pool_;
    MainThreadQueue& main_;
    AccountService& accounts_;
    AliveFlag alive_;
    std::function<void(const std::string&)> openBrowser_;

    Stage stage_ = Stage::Idle;
    api::QuickLoginCode code_;
    std::string accountName_;
    std::string expected_;
    ErrorCode error_ = ErrorCode::None;
    Done done_;
    int generation_ = 0;  // bumps on start/cancel to drop stale callbacks
};

}  // namespace rl::accounts
