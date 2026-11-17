#include "accounts/CookieVault.h"

#include <windows.h>
#include <wincrypt.h>

#include <vector>

#include "core/Logger.h"

namespace rl::accounts::vault {
namespace {

constexpr char kEntropy[] = "RobloxLauncher/cookie-vault/v1";

DATA_BLOB entropyBlob() {
    DATA_BLOB b;
    b.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(kEntropy));
    b.cbData = sizeof(kEntropy) - 1;
    return b;
}

std::string toBase64(const BYTE* data, DWORD size) {
    DWORD chars = 0;
    const DWORD flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
    if (!CryptBinaryToStringA(data, size, flags, nullptr, &chars)) return {};
    std::string out(chars, '\0');
    if (!CryptBinaryToStringA(data, size, flags, out.data(), &chars)) return {};
    out.resize(chars);
    while (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

bool fromBase64(const std::string& s, std::vector<BYTE>& out) {
    DWORD size = 0;
    if (!CryptStringToBinaryA(s.c_str(), static_cast<DWORD>(s.size()), CRYPT_STRING_BASE64, nullptr, &size, nullptr,
                              nullptr))
        return false;
    out.resize(size);
    return CryptStringToBinaryA(s.c_str(), static_cast<DWORD>(s.size()), CRYPT_STRING_BASE64, out.data(), &size,
                                nullptr, nullptr) != FALSE;
}

}  // namespace

Result<std::string> protect(const SecureString& plaintext) {
    DATA_BLOB in;
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.reveal().data()));
    in.cbData = static_cast<DWORD>(plaintext.reveal().size());
    DATA_BLOB entropy = entropyBlob();
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"RobloxLauncher session", &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                          &out)) {
        log::error("vault", "CryptProtectData failed: " + std::to_string(GetLastError()));
        return Result<std::string>::fail(ErrorCode::CryptoFailed);
    }
    std::string encoded = toBase64(out.pbData, out.cbData);
    LocalFree(out.pbData);
    if (encoded.empty()) return Result<std::string>::fail(ErrorCode::CryptoFailed);
    return Result<std::string>::ok(std::move(encoded));
}

Result<SecureString> unprotect(const std::string& base64Blob) {
    std::vector<BYTE> blob;
    if (base64Blob.empty() || !fromBase64(base64Blob, blob)) {
        return Result<SecureString>::fail(ErrorCode::CryptoFailed);
    }
    DATA_BLOB in;
    in.pbData = blob.data();
    in.cbData = static_cast<DWORD>(blob.size());
    DATA_BLOB entropy = entropyBlob();
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        log::error("vault", "CryptUnprotectData failed: " + std::to_string(GetLastError()));
        return Result<SecureString>::fail(ErrorCode::CryptoFailed);
    }
    SecureString value(std::string(reinterpret_cast<const char*>(out.pbData), out.cbData));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return Result<SecureString>::ok(std::move(value));
}

}  // namespace rl::accounts::vault
