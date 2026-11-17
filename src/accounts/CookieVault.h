#pragma once

#include <string>

#include "core/Error.h"
#include "core/SecureString.h"

// Session cookies at rest are protected with Windows DPAPI (CryptProtectData):
// the blob can only be decrypted by the same Windows user on the same machine.
// An app-specific entropy value is mixed in so other programs running as the
// user cannot decrypt it by calling CryptUnprotectData blindly.
namespace rl::accounts::vault {

// Returns the protected blob, Base64-encoded for JSON storage.
Result<std::string> protect(const SecureString& plaintext);
Result<SecureString> unprotect(const std::string& base64Blob);

}  // namespace rl::accounts::vault
