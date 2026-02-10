cmake_minimum_required(VERSION 3.7)
project(BeastEm VERSION 0.1.0)

set(BeastEm_PLATFORM Undefined)
if(WIN32)
    set(BeastEm_PLATFORM PLATFORM_WIN32)
elseif(APPLE)
    set(BeastEm_PLATFORM PLATFORM_MACOS)
elseif(UNIX AND NOT APPLE)
    set(BeastEm_PLATFORM PLATFORM_LINUX)
endif()

message("BeastEm Platform: ${BeastEm_PLATFORM}")

set(BeastEm_COMPILER Undefined)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    set(BeastEm_COMPILER COMPILER_CLANGCL)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(BeastEm_COMPILER COMPILER_MSVC)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    # Just treat clang as GCC...
    set(BeastEm_COMPILER COMPILER_GNU)
endif()

message("BeastEm Compiler: ${BeastEm_COMPILER}")

# Use latest C++ by default (should be the best one), but let user override it
if(NOT DEFINED CMAKE_CXX_STANDARD)
    if(CMAKE_VERSION VERSION_LESS "3.8")
        set (CMAKE_CXX_STANDARD 14)
    elseif(CMAKE_VERSION VERSION_LESS "3.12")
        set (CMAKE_CXX_STANDARD 17)
    elseif(CMAKE_VERSION VERSION_LESS "3.20")
        set (CMAKE_CXX_STANDARD 20)
    else()
        set (CMAKE_CXX_STANDARD 23)
    endif()
endif()

message("Using C++${CMAKE_CXX_STANDARD}")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(SDL2 REQUIRED)
find_package(SDL2_net REQUIRED)
find_package(SDL2_gfx REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_image REQUIRED)

add_executable(beastem
    beastem.cpp
    src/assets.cpp
    src/beast.cpp
    src/debugmanager.cpp
    src/digit.cpp
    src/i2c.cpp
    src/listing.cpp
    src/binaryFile.cpp
    src/gui.cpp
    src/pagemap.cpp
    src/videobeast.cpp
    src/debug.cpp
    src/display.cpp
    src/instructions.cpp
    src/rtc.cpp
)

# nfd cmake adds relevant platform GUI libraries to link_directories etc...
add_subdirectory(deps/nativefiledialog-extended-1.1.1)

include_directories(
    deps/nativefiledialog-extendevid-1.1.1/src/include
    ${SDL2_INCLUDE_DIRS} 
    ${SDL_NET_INCLUDE_DIRS}
    ${SDL_GFX_INCLUDE_DIRS}
    ${SDL_TTF_INCLUDE_DIRS}
    ${SDL_IMAGE_INCLUDE_DIRS}
)

if(TARGET SDL2::SDL2main)
    # MUST be added before SDL2::SDL2!
    target_link_libraries(beastem PRIVATE SDL2::SDL2main)
endif()

target_link_libraries(beastem PRIVATE
    nfd
    SDL2::SDL2
    SDL2_net::SDL2_net
    SDL2_image::SDL2_image
    SDL2_ttf::SDL2_ttf
    ${SDL2_GFX_LIBRARIES}
)

# Test executable for DebugManager
add_executable(test_debugmanager
    tests/test_debugmanager.cpp
    src/debugmanager.cpp
)

enable_testing()
add_test(NAME DebugManagerTests COMMAND test_debugmanager)

