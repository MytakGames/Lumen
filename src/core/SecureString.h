#pragma once

#include <windows.h>

#include <string>
#include <utility>

namespace rl {

// Holds a secret (session cookie, auth ticket) and wipes it on destruction.
// Move-only, never implicitly converted, never logged.
class SecureString {
public:
    SecureString() = default;
    explicit SecureString(std::string value) : value_(std::move(value)) {}
    ~SecureString() { wipe(); }
    SecureString(const SecureString&) = delete;
    SecureString& operator=(const SecureString&) = delete;
    SecureString(SecureString&& o) noexcept : value_(std::move(o.value_)) { o.wipe(); }
    SecureString& operator=(SecureString&& o) noexcept {
        if (this != &o) {
            wipe();
            value_ = std::move(o.value_);
            o.wipe();
        }
        return *this;
    }

    const std::string& reveal() const { return value_; }
    bool empty() const { return value_.empty(); }
    SecureString clone() const { return SecureString(value_); }

    void wipe() {
        if (!value_.empty()) SecureZeroMemory(value_.data(), value_.size());
        value_.clear();
    }

private:
    std::string value_;
};

}  // namespace rl
