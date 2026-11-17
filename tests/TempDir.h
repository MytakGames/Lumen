#pragma once

#include <windows.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <system_error>

// Unique temporary directory removed at scope exit.
class TempDir {
public:
    TempDir() {
        static std::atomic<int> counter{0};
        path_ = std::filesystem::temp_directory_path() /
                ("rl_tests_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(counter++));
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};
