cmake_minimum_required(VERSION 3.15)
project(game_dod_sdl VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer find_package(SDL2) if system provides, else use pkg-config
find_package(SDL2 QUIET)

if (SDL2_FOUND)
    message(STATUS "Found SDL2 via find_package")
    include_directories(${SDL2_INCLUDE_DIRS})
    add_executable(game src/main.cpp)
    target_link_libraries(game PRIVATE ${SDL2_LIBRARIES})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_PKG REQUIRED sdl2)
    message(STATUS "Found SDL2 via pkg-config: ${SDL2_PKG_INCLUDE_DIRS}")
    include_directories(${SDL2_PKG_INCLUDE_DIRS})
    add_executable(game src/main.cpp)
    target_link_libraries(game PRIVATE ${SDL2_PKG_LIBRARIES})
    target_compile_options(game PRIVATE ${SDL2_PKG_CFLAGS_OTHER})
endif()
