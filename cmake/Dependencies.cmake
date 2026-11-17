# Third-party dependencies, fetched at configure time with FetchContent.
# All are header-only or compiled from source, so the same recipe works on
# MSVC and MinGW without vcpkg.
include(FetchContent)

set(FETCHCONTENT_QUIET ON)

# --- nlohmann/json (MIT) ----------------------------------------------------
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")

# --- Dear ImGui (MIT) -------------------------------------------------------
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
    GIT_SHALLOW    TRUE)

# imgui has no CMakeLists.txt, so MakeAvailable only downloads it and the
# library target is defined below.
FetchContent_MakeAvailable(nlohmann_json imgui)

add_library(imgui_dx11 STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
target_include_directories(imgui_dx11 SYSTEM PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(imgui_dx11 PUBLIC
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_USE_WCHAR32   # glyphs above U+FFFF (emoji in game names)
    UNICODE _UNICODE)
target_link_libraries(imgui_dx11 PUBLIC d3d11 dxgi d3dcompiler dwmapi imm32)

# --- stb_image (public domain / MIT): PNG/JPEG decoding for thumbnails -----
FetchContent_Declare(stb_image
    URL https://raw.githubusercontent.com/nothings/stb/5c205738c191bcb0abc65c4febfa9bd25ff35234/stb_image.h
    DOWNLOAD_NO_EXTRACT TRUE)
FetchContent_MakeAvailable(stb_image)
add_library(stb_image INTERFACE)
target_include_directories(stb_image SYSTEM INTERFACE ${stb_image_SOURCE_DIR})

# --- doctest (MIT), tests only ---------------------------------------------
if(RL_BUILD_TESTS)
    FetchContent_Declare(doctest
        URL https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h
        DOWNLOAD_NO_EXTRACT TRUE)
    FetchContent_MakeAvailable(doctest)
    add_library(doctest INTERFACE)
    target_include_directories(doctest SYSTEM INTERFACE ${doctest_SOURCE_DIR})
endif()
