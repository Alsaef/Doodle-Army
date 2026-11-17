cmake_minimum_required(VERSION 3.16)
project(MiniMilitiaNativeAndroid C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# 1. Raylib for Android
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
    GIT_SHALLOW TRUE
)
set(PLATFORM "Android" CACHE STRING "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# 2. nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# 3. MbedTLS & IXWebSocket (for Android WSS TLS support)
FetchContent_Declare(
    mbedtls
    GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
    GIT_TAG v2.28.8
    GIT_SHALLOW TRUE
)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(mbedtls)

set(USE_TLS ON CACHE BOOL "" FORCE)
set(USE_MBED_TLS ON CACHE BOOL "" FORCE)
set(USE_OPEN_SSL OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FOUND TRUE CACHE BOOL "" FORCE)
set(MBEDTLS_INCLUDE_DIRS ${mbedtls_SOURCE_DIR}/include CACHE PATH "" FORCE)
set(MBEDTLS_LIBRARIES mbedtls mbedcrypto mbedx509 CACHE STRING "" FORCE)

FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG v11.4.4
    GIT_SHALLOW TRUE
)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ixwebsocket)

# 4. raygui
FetchContent_Declare(
    raygui
    GIT_REPOSITORY https://github.com/raysan5/raygui.git
    GIT_TAG 4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raygui)

# 5. Native shared library for Android NativeActivity
add_library(main SHARED
    main.cpp
    TouchControls.cpp
    NetworkManager.cpp
    GameWorld.cpp
    Renderer.cpp
)

target_include_directories(main PRIVATE
    .
    ${raygui_SOURCE_DIR}/src
    ${mbedtls_SOURCE_DIR}/include
)

target_link_libraries(main PRIVATE
    raylib
    nlohmann_json::nlohmann_json
    ixwebsocket
    mbedtls
    mbedcrypto
    mbedx509
    android
    log
    EGL
    GLESv2
    OpenSLES
)
