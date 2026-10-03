cmake_minimum_required(VERSION 3.24)
project(SAPIemu VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SAPIEMU_BUILD_GUI   "Build the SDL3/ImGui frontend" ON)
option(SAPIEMU_BUILD_TESTS "Build unit and integration tests" ON)

set(TP ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

if(MSVC)
    add_compile_options(/utf-8 /W3 /permissive- /Zc:__cplusplus)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# --- third party -------------------------------------------------------------

# floooh/chips Z80 core (zlib license), implementation compiled as C
add_library(chips_z80 STATIC src/cpu/z80_impl.c)
target_include_directories(chips_z80 PUBLIC ${TP}/chips)

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${TP}/json)

add_library(httplib INTERFACE)
target_include_directories(httplib INTERFACE ${TP}/httplib)
if(WIN32)
    target_link_libraries(httplib INTERFACE ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(httplib INTERFACE Threads::Threads)
endif()

# --- emulator core (no GUI dependency) ----------------------------------------

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    src/core/*.cpp src/cpu/*.cpp src/chips/*.cpp src/cards/*.cpp
    src/input/*.cpp src/serial/*.cpp src/mcp/*.cpp)
add_library(sapicore STATIC ${CORE_SOURCES})
target_include_directories(sapicore PUBLIC src)
target_link_libraries(sapicore PUBLIC chips_z80 nlohmann_json httplib)
if(NOT WIN32)
    target_link_libraries(sapicore PUBLIC Threads::Threads)
endif()
target_compile_definitions(sapicore PUBLIC
    SAPIEMU_VERSION="${PROJECT_VERSION}"
    SAPIEMU_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# --- frontend -------------------------------------------------------------------

if(SAPIEMU_BUILD_GUI)
    # Prefer an installed SDL3 (Linux distro packages), otherwise fetch the release.
    find_package(SDL3 CONFIG QUIET)
    if(NOT SDL3_FOUND)
        include(FetchContent)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(SDL3
            URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(SDL3)
    endif()

    add_library(imgui STATIC
        ${TP}/imgui/imgui.cpp ${TP}/imgui/imgui_draw.cpp ${TP}/imgui/imgui_tables.cpp
        ${TP}/imgui/imgui_widgets.cpp ${TP}/imgui/imgui_demo.cpp
        ${TP}/imgui/backends/imgui_impl_sdl3.cpp ${TP}/imgui/backends/imgui_impl_sdlrenderer3.cpp)
    target_include_directories(imgui PUBLIC ${TP}/imgui ${TP}/imgui/backends)
    target_link_libraries(imgui PUBLIC SDL3::SDL3)

    file(GLOB_RECURSE FRONTEND_SOURCES CONFIGURE_DEPENDS src/frontend/*.cpp)
    # GUI application (no console window on Windows)
    add_executable(sapiemu WIN32 src/main.cpp ${FRONTEND_SOURCES})
    target_link_libraries(sapiemu PRIVATE sapicore imgui SDL3::SDL3)
    target_compile_definitions(sapiemu PRIVATE SAPIEMU_GUI=1)
endif()

# Console tool: headless runs, scripting, tests
add_executable(sapiemu-cli src/main.cpp)
target_link_libraries(sapiemu-cli PRIVATE sapicore)

if(SAPIEMU_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
