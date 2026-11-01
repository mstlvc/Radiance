cmake_minimum_required(VERSION 3.21)
project(Radiance VERSION 1.0.0 LANGUAGES C CXX RC)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Radiance builds with MSVC for x64 (the bundled FreeType library is x64 / static CRT).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Static CRT: one Radiance.exe with nothing to install, and it matches freetype.lib.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---------------------------------------------------------------- Dear ImGui (from the UI source)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp
    ${IMGUI_DIR}/misc/imgui_stdlib.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/freetype/include
)
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS NOMINMAX UNICODE _UNICODE)
target_link_libraries(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/freetype/win64/freetype.lib
    d3d11 dxgi d3dcompiler)
target_compile_options(imgui PRIVATE /W0)

# ---------------------------------------------------------------- Radiance

add_executable(Radiance WIN32
    src/main.cpp
    src/app/App.cpp
    src/core/Catalog.cpp
    src/core/ColourMath.cpp
    src/core/Json.cpp
    src/core/Mccs.cpp
    src/core/Model.cpp
    src/core/Settings.cpp
    src/core/ShareCode.cpp
    src/platform/Ddc.cpp
    src/platform/DisplayColour.cpp
    src/platform/Displays.cpp
    src/platform/Dx11Renderer.cpp
    src/platform/Nvapi.cpp
    src/platform/NvidiaDisplay.cpp
    src/platform/Processes.cpp
    src/platform/Screenshot.cpp
    src/platform/Strings.cpp
    src/platform/System.cpp
    src/platform/Textures.cpp
    src/platform/Tray.cpp
    src/services/ColourEngine.cpp
    src/services/DisplayService.cpp
    src/services/MonitorControl.cpp
    src/services/GameWatcher.cpp
    src/services/Services.cpp
    src/ui/Fonts.cpp
    src/ui/Icons.cpp
    src/ui/QuickPanel.cpp
    src/ui/Backdrop.cpp
    src/ui/Shell.cpp
    src/ui/Theme.cpp
    src/ui/Widgets.cpp
    src/ui/pages/DisplayPage.cpp
    src/ui/pages/MonitorPage.cpp
    src/ui/pages/ProfilesPage.cpp
    src/ui/pages/ResolutionPage.cpp
    src/ui/pages/SettingsPage.cpp
    src/ui/pages/Shared.cpp
    src/ui/pages/UtilitiesPage.cpp
    res/radiance.rc
)
target_include_directories(Radiance PRIVATE src)
target_compile_definitions(Radiance PRIVATE RADIANCE_VERSION="${PROJECT_VERSION}" _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_compile_options(Radiance PRIVATE /W4 /permissive- /utf-8 /MP /wd4100)
target_link_libraries(Radiance PRIVATE imgui dwmapi shcore windowscodecs bcrypt shell32 ole32 comctl32 advapi32 dxva2)
# radiance.rc embeds radiance.manifest itself.
target_link_options(Radiance PRIVATE /MANIFEST:NO $<$<CONFIG:Debug>:/NODEFAULTLIB:LIBCMT>)
set_target_properties(Radiance PROPERTIES OUTPUT_NAME "Radiance")
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Radiance)
