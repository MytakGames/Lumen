cmake_minimum_required(VERSION 3.20)

project(RobloxLauncher
    VERSION 0.1.0
    DESCRIPTION "Lumen - alternative multi-account launcher for Roblox"
    LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "RobloxLauncher targets Windows 10/11 x64 only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RL_BUILD_TESTS "Build unit tests" ON)

include(cmake/Dependencies.cmake)
include(cmake/CompilerWarnings.cmake)

# ---------------------------------------------------------------------------
# Core library: everything that does not depend on the UI toolkit.
# Kept separate so unit tests can link it without a window or a GPU.
# ---------------------------------------------------------------------------
add_library(rl_core STATIC
    src/core/Logger.cpp
    src/core/Paths.cpp
    src/core/AtomicFile.cpp
    src/core/StringUtil.cpp
    src/core/ThreadPool.cpp
    src/core/Autostart.cpp
    src/core/Error.cpp
    src/settings/Settings.cpp
    src/i18n/Localizer.cpp
    src/robloxapi/HttpClient.cpp
    src/robloxapi/Parsers.cpp
    src/robloxapi/RobloxApi.cpp
    src/accounts/CookieVault.cpp
    src/accounts/AccountStore.cpp
    src/accounts/AccountService.cpp
    src/accounts/QuickLoginFlow.cpp
    src/instances/RobloxLocator.cpp
    src/instances/InstanceManager.cpp
    src/games/GameList.cpp
    src/games/RecommendationEngine.cpp
    src/games/GameService.cpp
)
target_include_directories(rl_core PUBLIC src)
target_link_libraries(rl_core PUBLIC nlohmann_json::nlohmann_json shell32 ole32 advapi32 winhttp crypt32)
target_compile_definitions(rl_core PUBLIC
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00
    RL_VERSION="${PROJECT_VERSION}"
    RL_VERSION_W=L"${PROJECT_VERSION}")
rl_set_warnings(rl_core)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
add_executable(RobloxLauncher WIN32
    src/main.cpp
    src/app/Application.cpp
    src/app/Window.cpp
    src/app/TrayIcon.cpp
    src/app/FileDialogs.cpp
    src/ui/Theme.cpp
    src/ui/Fonts.cpp
    src/ui/Widgets.cpp
    src/ui/Components.cpp
    src/ui/LoginDialog.cpp
    src/ui/TextureCache.cpp
    src/ui/ImageDecode.cpp
    src/ui/MainLayout.cpp
    src/ui/screens/GameUi.cpp
    src/ui/screens/HomeScreen.cpp
    src/ui/screens/GameDetailScreen.cpp
    src/ui/screens/AccountsScreen.cpp
    src/ui/screens/InstancesScreen.cpp
    src/ui/screens/SettingsScreen.cpp
)
# Third-party implementation compiled in our target: silence its warnings only.
if(MSVC)
    set_source_files_properties(src/ui/ImageDecode.cpp PROPERTIES COMPILE_OPTIONS "/W0")
else()
    set_source_files_properties(src/ui/ImageDecode.cpp PROPERTIES COMPILE_OPTIONS "-w")
endif()
# Resources compiled separately: windres cannot handle include paths with
# spaces, so this object library deliberately has no include directories.
add_library(rl_resources OBJECT assets/app.rc)
target_sources(RobloxLauncher PRIVATE $<TARGET_OBJECTS:rl_resources>)
target_link_libraries(RobloxLauncher PRIVATE rl_core imgui_dx11 stb_image dwmapi comdlg32)
rl_set_warnings(RobloxLauncher)
set_target_properties(RobloxLauncher PROPERTIES OUTPUT_NAME Lumen)

if(MSVC)
    # The manifest is embedded by assets/app.rc; avoid a duplicate from the linker.
    target_link_options(RobloxLauncher PRIVATE /MANIFEST:NO)
endif()
if(MINGW)
    # Static runtime: the exe runs without MinGW DLLs on PATH.
    target_link_options(RobloxLauncher PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# Locale files live next to the executable.
add_custom_command(TARGET RobloxLauncher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/locales $<TARGET_FILE_DIR:RobloxLauncher>/locales
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/README.md ${CMAKE_SOURCE_DIR}/README.it.md $<TARGET_FILE_DIR:RobloxLauncher>
    COMMENT "Copying locales and README")

install(TARGETS RobloxLauncher RUNTIME DESTINATION .)
install(DIRECTORY locales DESTINATION .)
install(FILES README.md README.it.md LICENSE DESTINATION .)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(RL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
