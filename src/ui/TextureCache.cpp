#include "ui/TextureCache.h"

#include <algorithm>
#include <cstdio>
#include <system_error>
#include <vector>

#include "core/AtomicFile.h"
#include "core/Logger.h"
#include "core/StringUtil.h"
#include "ui/ImageDecode.h"
#include "ui/Widgets.h"

namespace rl::ui {
namespace {

constexpr size_t kMaxTextures = 400;
constexpr int kMaxInFlight = 6;

std::filesystem::path fileFor(const std::filesystem::path& dir, const std::string& url) {
    char name[32];
    std::snprintf(name, sizeof(name), "%016llx.img", static_cast<unsigned long long>(str::fnv1a(url)));
    return dir / name;
}

void trimDirectory(const std::filesystem::path& dir, std::uintmax_t limit) {
    struct F {
        std::filesystem::path path;
        std::filesystem::file_time_type time;
        std::uintmax_t size;
    };
    std::vector<F> files;
    std::uintmax_t total = 0;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
        if (!e.is_regular_file(ec)) continue;
        F f{e.path(), e.last_write_time(ec), e.file_size(ec)};
        total += f.size;
        files.push_back(std::move(f));
    }
    if (total <= limit) return;
    std::sort(files.begin(), files.end(), [](const F& a, const F& b) { return a.time < b.time; });
    for (const auto& f : files) {
        if (total <= limit) break;
        if (std::filesystem::remove(f.path, ec)) total -= f.size;
    }
}

}  // namespace

TextureCache::TextureCache(ID3D11Device* device, api::RobloxApi& api, ThreadPool& pool, MainThreadQueue& main,
                           std::filesystem::path dir, AliveFlag alive)
    : device_(device), api_(api), pool_(pool), main_(main), dir_(std::move(dir)), alive_(std::move(alive)) {
    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
}

ImTextureID TextureCache::get(const std::string& url) {
    if (url.empty()) return ImTextureID{};
    auto it = entries_.find(url);
    if (it != entries_.end()) {
        Entry& e = it->second;
        lru_.splice(lru_.begin(), lru_, e.lru);
        if (e.srv) return reinterpret_cast<ImTextureID>(e.srv.get());
        if (e.loading || e.failed) return ImTextureID{};
    }
    if (inFlight_ >= kMaxInFlight) {
        requestRedraw();  // try again next frame
        return ImTextureID{};
    }

    lru_.push_front(url);
    Entry& e = entries_[url];
    e.lru = lru_.begin();
    e.loading = true;
    ++inFlight_;
    const auto file = fileFor(dir_, url);
    const auto limit = limitBytes_;
    pool_.post([this, url, file, limit, alive = alive_] {
        std::string bytes;
        bool fromDisk = false;
        if (auto cached = fs::readAll(file)) {
            bytes = std::move(*cached);
            fromDisk = true;
            std::error_code ec;
            std::filesystem::last_write_time(file, std::filesystem::file_time_type::clock::now(), ec);  // LRU touch
        } else if (auto dl = api_.download(url)) {
            bytes = std::move(*dl.value);
        }
        int w = 0, h = 0;
        std::string pixels = bytes.empty() ? std::string{} : decodeRgba(bytes, w, h);
        if (!pixels.empty() && !fromDisk) {
            fs::writeAtomic(file, bytes);
            trimDirectory(dir_, limit);
        }
        main_.post([this, url, pixels = std::move(pixels), w, h, alive]() mutable {
            if (!*alive) return;
            --inFlight_;
            upload(url, std::move(pixels), w, h);
        });
    });
    return ImTextureID{};
}

ComPtr<ID3D11ShaderResourceView> createRgbaTexture(ID3D11Device* device, const std::string& rgba, int w, int h) {
    ComPtr<ID3D11ShaderResourceView> out;
    if (!device || w <= 0 || h <= 0 || rgba.size() < static_cast<size_t>(w) * static_cast<size_t>(h) * 4) return out;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(w);
    desc.Height = static_cast<UINT>(h);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = rgba.data();
    data.SysMemPitch = desc.Width * 4;
    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(device->CreateTexture2D(&desc, &data, tex.put()))) return out;
    D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = desc.Format;
    srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    if (FAILED(device->CreateShaderResourceView(tex.get(), &srv, out.put()))) out.reset();
    return out;
}

ComPtr<ID3D11ShaderResourceView> loadResourceImage(ID3D11Device* device, int resourceId) {
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC res = FindResourceW(module, MAKEINTRESOURCEW(resourceId), MAKEINTRESOURCEW(10) /* RT_RCDATA */);
    HGLOBAL handle = res ? LoadResource(module, res) : nullptr;
    const void* bytes = handle ? LockResource(handle) : nullptr;
    if (!bytes) return {};
    const std::string encoded(static_cast<const char*>(bytes), SizeofResource(module, res));
    int w = 0, h = 0;
    const std::string rgba = decodeRgba(encoded, w, h);
    return createRgbaTexture(device, rgba, w, h);
}

void TextureCache::upload(const std::string& url, std::string pixels, int w, int h) {
    auto it = entries_.find(url);
    if (it == entries_.end()) return;
    Entry& e = it->second;
    e.loading = false;
    if (pixels.empty() || w <= 0 || h <= 0) {
        e.failed = true;
        return;
    }
    e.srv = createRgbaTexture(device_, pixels, w, h);
    if (!e.srv) {
        e.failed = true;
        return;
    }
    e.width = w;
    e.height = h;
    requestRedraw();
    evict();
}

float TextureCache::aspect(const std::string& url) const {
    const auto it = entries_.find(url);
    if (it == entries_.end() || it->second.height == 0) return 1.0f;
    return static_cast<float>(it->second.width) / static_cast<float>(it->second.height);
}

void TextureCache::evict() {
    while (entries_.size() > kMaxTextures && !lru_.empty()) {
        const std::string& victim = lru_.back();
        auto it = entries_.find(victim);
        if (it != entries_.end() && it->second.loading) break;  // keep in-flight entries
        if (it != entries_.end()) entries_.erase(it);
        lru_.pop_back();
    }
}

void TextureCache::trimDiskAsync() {
    pool_.post([dir = dir_, limit = limitBytes_] { trimDirectory(dir, limit); });
}

void TextureCache::clear() {
    // Keep in-flight entries so their callbacks still find a slot.
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (it->second.loading) {
            ++it;
        } else {
            lru_.erase(it->second.lru);
            it = entries_.erase(it);
        }
    }
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir_, ec)) std::filesystem::remove(e.path(), ec);
    log::info("cache", "Thumbnail cache cleared");
}

std::uintmax_t TextureCache::diskUsage() const {
    std::uintmax_t total = 0;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir_, ec)) {
        if (e.is_regular_file(ec)) total += e.file_size(ec);
    }
    return total;
}

}  // namespace rl::ui
