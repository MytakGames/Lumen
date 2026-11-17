#pragma once

#include <d3d11.h>

#include <cstdint>
#include <filesystem>
#include <list>
#include <string>
#include <unordered_map>

#include "core/ComPtr.h"
#include "core/ThreadPool.h"
#include "imgui.h"
#include "robloxapi/RobloxApi.h"

namespace rl::ui {

// Immutable RGBA8 texture; empty on failure.
ComPtr<ID3D11ShaderResourceView> createRgbaTexture(ID3D11Device* device, const std::string& rgba, int w, int h);
// Decodes a PNG/JPEG embedded as RCDATA resource `resourceId`; empty on failure.
ComPtr<ID3D11ShaderResourceView> loadResourceImage(ID3D11Device* device, int resourceId);
inline constexpr int kLogoResourceId = 2;  // assets/app.rc: 2 RCDATA "logo.png"

// Asynchronous image loader: disk cache (raw downloaded bytes, LRU-trimmed to
// a size limit) + GPU textures in memory (LRU, bounded count).
// get() never blocks: it returns nullptr while loading.
class TextureCache {
public:
    TextureCache(ID3D11Device* device, api::RobloxApi& api, ThreadPool& pool, MainThreadQueue& main,
                 std::filesystem::path dir, AliveFlag alive);

    // Returns the texture for `url` or nullptr (placeholder) while it loads.
    ImTextureID get(const std::string& url);
    float aspect(const std::string& url) const;  // width / height, 1 when unknown

    void setDiskLimitMb(int mb) { limitBytes_ = static_cast<std::uintmax_t>(mb) * 1024 * 1024; }
    void trimDiskAsync();
    void clear();  // drops memory and disk caches
    std::uintmax_t diskUsage() const;

private:
    struct Entry {
        ComPtr<ID3D11ShaderResourceView> srv;
        int width = 0, height = 0;
        bool loading = false;
        bool failed = false;
        std::list<std::string>::iterator lru;
    };
    void upload(const std::string& url, std::string pixels, int w, int h);
    void evict();

    ID3D11Device* device_;
    api::RobloxApi& api_;
    ThreadPool& pool_;
    MainThreadQueue& main_;
    std::filesystem::path dir_;
    AliveFlag alive_;
    std::uintmax_t limitBytes_ = 200ull * 1024 * 1024;
    std::unordered_map<std::string, Entry> entries_;
    std::list<std::string> lru_;  // front = most recently used
    int inFlight_ = 0;
};

}  // namespace rl::ui
