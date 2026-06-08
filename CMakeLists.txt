cmake_minimum_required(VERSION 3.20)

if(NOT WIN32)
    message(FATAL_ERROR "CPURasterizer currently supports Windows only. Configure and build this project on Windows.")
endif()

project(CPURasterizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(CPURasterizerCore STATIC
    src/core/Application.cpp
    src/core/Camera.cpp
    src/core/Framebuffer.cpp
    src/core/PerformanceHud.cpp
    src/core/ThreadPool.cpp
    src/platform/Win32Window.cpp
    src/renderer/ObjLoader.cpp
    src/renderer/RenderPasses.cpp
    src/renderer/Renderer.cpp
    src/renderer/Texture.cpp
    src/scenes/AssetLocator.cpp
    src/scenes/LoadDiagnostics.cpp
    src/scenes/MeshFactory.cpp
    src/scenes/ScenePreset.cpp
    src/scenes/TestScene.cpp
)

target_include_directories(CPURasterizerCore PUBLIC src)

if(WIN32)
    target_compile_definitions(CPURasterizerCore PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(CPURasterizerCore PUBLIC ole32 windowscodecs)
endif()

add_executable(CPURasterizer WIN32
    src/main.cpp
)

target_link_libraries(CPURasterizer PRIVATE CPURasterizerCore)

enable_testing()

add_executable(CPURasterizerTests
    tests/TestMain.cpp
    tests/AssetLocatorTests.cpp
    tests/MeshFactoryTests.cpp
    tests/ObjLoaderTests.cpp
    tests/RendererTests.cpp
    tests/ThreadPoolTests.cpp
    tests/TestSceneTests.cpp
)

target_link_libraries(CPURasterizerTests PRIVATE CPURasterizerCore)

add_test(NAME CPURasterizerTests COMMAND CPURasterizerTests)

if(MSVC)
    target_compile_options(CPURasterizerCore PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(CPURasterizerTests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(CPURasterizerCore PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(CPURasterizerTests PRIVATE -Wall -Wextra -Wpedantic)
endif()
