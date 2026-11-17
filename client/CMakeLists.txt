cmake_minimum_required(VERSION 3.16)
project(MiniMilitiaNative C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# -------------------------------------------------------------
# 1. Dependency: Raylib (v5.0)
# -------------------------------------------------------------
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
    GIT_SHALLOW TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# -------------------------------------------------------------
# 2. Dependency: nlohmann/json (v3.11.3)
# -------------------------------------------------------------
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# -------------------------------------------------------------
# 3. Dependency: IXWebSocket (v11.4.4)
# -------------------------------------------------------------
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG v11.4.4
    GIT_SHALLOW TRUE
)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(USE_TLS ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ixwebsocket)

# -------------------------------------------------------------
# 4. Dependency: raygui (v4.0)
# -------------------------------------------------------------
FetchContent_Declare(
    raygui
    GIT_REPOSITORY https://github.com/raysan5/raygui.git
    GIT_TAG 4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raygui)

# -------------------------------------------------------------
# 5. Project Sources & Executable
# -------------------------------------------------------------
set(SOURCE_FILES
    src/main.cpp
    src/TouchControls.cpp
    src/NetworkManager.cpp
    src/GameWorld.cpp
    src/Renderer.cpp
)

add_executable(MiniMilitiaNative ${SOURCE_FILES})

target_include_directories(MiniMilitiaNative PRIVATE
    include
    src
    ${raygui_SOURCE_DIR}/src
)

target_link_libraries(MiniMilitiaNative PRIVATE
    raylib
    nlohmann_json::nlohmann_json
    ixwebsocket
)

# Platform specific link flags
if(UNIX AND NOT APPLE AND NOT ANDROID)
    target_link_libraries(MiniMilitiaNative PRIVATE pthread dl m)
endif()

if(WIN32)
    target_link_libraries(MiniMilitiaNative PRIVATE ws2_32 crypt32)
endif()

if(ANDROID)
    target_link_libraries(MiniMilitiaNative PRIVATE android log EGL GLESv2 OpenSLES)
endif()

# Copy assets to build directory
add_custom_command(TARGET MiniMilitiaNative POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:MiniMilitiaNative>/assets
)
