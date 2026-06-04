cmake_minimum_required(VERSION 3.20)

project(CPURasterizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(CPURasterizer WIN32
    src/main.cpp
    src/core/Application.cpp
    src/core/Camera.cpp
    src/core/Framebuffer.cpp
    src/core/PerformanceHud.cpp
    src/platform/Win32Window.cpp
    src/renderer/ObjLoader.cpp
    src/renderer/Renderer.cpp
    src/renderer/Texture.cpp
    src/scenes/TestScene.cpp
)

target_include_directories(CPURasterizer PRIVATE src)
target_compile_definitions(CPURasterizer PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(CPURasterizer PRIVATE ole32 windowscodecs)

if(MSVC)
    target_compile_options(CPURasterizer PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(CPURasterizer PRIVATE -Wall -Wextra -Wpedantic)
endif()
