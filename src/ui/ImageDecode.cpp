#include "ui/ImageDecode.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

namespace rl::ui {

std::string decodeRgba(const std::string& encoded, int& width, int& height) {
    int channels = 0;
    stbi_uc* data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(encoded.data()),
                                          static_cast<int>(encoded.size()), &width, &height, &channels, 4);
    if (!data) return {};
    std::string out(reinterpret_cast<const char*>(data), static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    stbi_image_free(data);
    return out;
}

}  // namespace rl::ui
