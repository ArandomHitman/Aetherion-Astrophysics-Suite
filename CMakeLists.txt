cmake_minimum_required(VERSION 3.10)
project(blackhole-sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Platform-specific package search hints ────────────────────
if(APPLE)
    # Help CMake find Homebrew-provided packages when configured via IDE tools.
    if(EXISTS "/opt/homebrew")
        list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
    endif()
    if(EXISTS "/usr/local")
        list(APPEND CMAKE_PREFIX_PATH "/usr/local")
    endif()
elseif(UNIX)
    # Common Linux distro install prefixes for Qt6, SFML, GLEW, GLM.
    # Adjust or remove entries that don't exist on your distro.
    foreach(_prefix "/usr" "/usr/local" "/usr/lib/x86_64-linux-gnu"
                    "/usr/lib/aarch64-linux-gnu")
        if(EXISTS "${_prefix}")
            list(APPEND CMAKE_PREFIX_PATH "${_prefix}")
        endif()
    endforeach()
elseif(WIN32)
    # ── Windows: MSYS2 / mingw-w64 ─────────────────────────────
    # When building with MSYS2 + mingw-w64, CMake is typically invoked from
    # the MSYS2 shell and already has the right paths. These are fallbacks.
    foreach(_prefix
            "C:/msys64/mingw64"
            "C:/msys64/ucrt64"
            "C:/Qt/6.9.0/mingw_64"
            "C:/Qt/6.9.0/msvc2022_64")
        if(EXISTS "${_prefix}")
            list(APPEND CMAKE_PREFIX_PATH "${_prefix}")
        endif()
    endforeach()
    # ── Windows: vcpkg ──────────────────────────────────────────
    # If using vcpkg, pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
    # at configure time. No extra hints needed here in that case.
endif()


# Find Qt6
find_package(Qt6 COMPONENTS Core Gui Widgets Charts REQUIRED)

# Accept any installed SFML (2.x or 3.x). Homebrew provides SFML 3.x on macOS.
# Use SFML component names with correct capitalization (SFML 3 expects e.g. "Graphics").
find_package(SFML COMPONENTS Graphics Window System REQUIRED)

# Find GLM (package name differs between distros)
find_package(glm CONFIG QUIET)
if(NOT glm_FOUND)
    find_package(GLM REQUIRED)
endif()

# Find OpenGL
find_package(OpenGL REQUIRED)

# ── GLEW (Linux / Windows — not needed on macOS) ─────────────
# macOS uses the system OpenGL framework directly (Core Profile via
# OpenGL/gl3.h). All other platforms need a runtime loader.
if(NOT APPLE)
    find_package(GLEW REQUIRED)
endif()

# ── Unified Qt6-based Application with Embedded Simulations ──
# All windows and simulations are tied to one executable instance
# WIN32 tells MSVC/MinGW to use the Windows GUI subsystem (no console window).
add_executable(blackhole-sim
    $<$<PLATFORM_ID:Windows>:WIN32>
    # Qt Launcher & Main Window
    src/QT-LAUNCHER/main.cpp
    src/QT-LAUNCHER/mainwindow.h
    src/QT-LAUNCHER/mainwindow.cpp
    
    # SFML Canvas for embedding SFML in Qt
    src/QT-LAUNCHER/sfml_canvas.h
    src/QT-LAUNCHER/sfml_canvas.cpp
    
    # Simulation Widgets
    src/QT-LAUNCHER/simulation_2d_widget.h
    src/QT-LAUNCHER/simulation_2d_widget.cpp
    src/QT-LAUNCHER/simulation_3d_widget.h
    src/QT-LAUNCHER/simulation_3d_widget.cpp

    # Keybind remapping widget (requires AUTOMOC for Q_OBJECT)
    src/QT-LAUNCHER/keybindbuttonwidget.h
    src/QT-LAUNCHER/keybindbuttonwidget.cpp
    
    # 2D/3D integration files should be linked as libraries or refactored modules,
    # not standalone entrypoints with their own main().
    src/3D/shader_utils.cpp
    src/3D/texture_utils.cpp
)

target_include_directories(blackhole-sim PRIVATE 
    ${CMAKE_SOURCE_DIR}/src/QT-LAUNCHER
    ${CMAKE_SOURCE_DIR}/src/2D
    ${CMAKE_SOURCE_DIR}/src/3D
    ${CMAKE_SOURCE_DIR}/src          # exposes platform.hpp as "platform.hpp"
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(blackhole-sim PRIVATE 
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Charts
    SFML::Graphics SFML::Window SFML::System
    OpenGL::GL
)

# GLEW for non-Apple platforms
if(NOT APPLE)
    target_link_libraries(blackhole-sim PRIVATE GLEW::GLEW)
endif()

if(glm_FOUND)
    target_link_libraries(blackhole-sim PRIVATE glm::glm)
else()
    target_include_directories(blackhole-sim PRIVATE ${GLM_INCLUDE_DIRS})
endif()

# Link OpenGL on macOS (also silences Apple deprecation warnings via platform.hpp define)
if(APPLE)
    find_library(OpenGL_LIBRARY OpenGL)
    target_link_libraries(blackhole-sim PRIVATE ${OpenGL_LIBRARY})
endif()

# GCC < 9 ships <filesystem> in a separate library; warn-proof the build.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
    target_link_libraries(blackhole-sim PRIVATE stdc++fs)
endif()

# Copy shader files for 3D simulation to build output
add_custom_command(TARGET blackhole-sim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/src/3D/BlackHole3D.frag
            $<TARGET_FILE_DIR:blackhole-sim>/BlackHole3D.frag)

add_custom_command(TARGET blackhole-sim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/src/3D/BlackHole3D_PhotorealDisk.frag
            $<TARGET_FILE_DIR:blackhole-sim>/BlackHole3D_PhotorealDisk.frag)

# ── Standalone simulation executables (restored) ───────────────────────────
add_executable(blackhole-2D src/2D/BlackHole2D.cpp)
target_include_directories(blackhole-2D PRIVATE
    ${CMAKE_SOURCE_DIR}/src/2D
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(blackhole-2D PRIVATE SFML::Graphics SFML::Window SFML::System)

add_executable(blackhole-3D
    src/3D/BlackHole3D.cpp
    src/3D/shader_utils.cpp
    src/3D/texture_utils.cpp
)
target_include_directories(blackhole-3D PRIVATE
    ${CMAKE_SOURCE_DIR}/src/3D
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(blackhole-3D PRIVATE SFML::Graphics SFML::Window SFML::System OpenGL::GL)
if(NOT APPLE)
    target_link_libraries(blackhole-3D PRIVATE GLEW::GLEW)
endif()
if(glm_FOUND)
    target_link_libraries(blackhole-3D PRIVATE glm::glm)
else()
    target_include_directories(blackhole-3D PRIVATE ${GLM_INCLUDE_DIRS})
endif()

# ── Shader copy: file-driven so any .frag edit triggers a re-copy ────────────
# Using add_custom_command with OUTPUT means cmake tracks the .frag as a real
# dependency — editing the source file causes a rebuild of the copy target,
# regardless of whether any C++ source changed.
foreach(_frag BlackHole3D.frag BlackHole3D_PhotorealDisk.frag)
    add_custom_command(
        OUTPUT  ${CMAKE_BINARY_DIR}/${_frag}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/src/3D/${_frag}
                ${CMAKE_BINARY_DIR}/${_frag}
        DEPENDS ${CMAKE_SOURCE_DIR}/src/3D/${_frag}
        COMMENT "Syncing shader ${_frag} to build directory"
    )
endforeach()
add_custom_target(sync_shaders_3D ALL
    DEPENDS
        ${CMAKE_BINARY_DIR}/BlackHole3D.frag
        ${CMAKE_BINARY_DIR}/BlackHole3D_PhotorealDisk.frag
)
add_dependencies(blackhole-3D sync_shaders_3D)

# If a disk texture exists, copy it next to the executable for convenience.
if(EXISTS ${CMAKE_SOURCE_DIR}/src/disk_texture.png)
    add_custom_command(TARGET blackhole-sim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/src/disk_texture.png
                $<TARGET_FILE_DIR:blackhole-sim>/disk_texture.png)
    add_custom_command(TARGET blackhole-3D POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/src/disk_texture.png
                $<TARGET_FILE_DIR:blackhole-3D>/disk_texture.png)
endif()

# Copy app icon next to the executable (sidebar logo display at runtime).
if(EXISTS ${CMAKE_SOURCE_DIR}/icon.png)
    add_custom_command(TARGET blackhole-sim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/icon.png
                $<TARGET_FILE_DIR:blackhole-sim>/icon.png)
    if(APPLE)
        add_custom_command(TARGET blackhole-sim POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${CMAKE_SOURCE_DIR}/icon.png
                    ${CMAKE_BINARY_DIR}/Aetherion.app/Contents/Resources/icon.png)
    endif()
endif()
# Pre-baked transparent version of the icon (preferred by the sidebar logo loader).
if(EXISTS ${CMAKE_SOURCE_DIR}/icon_transparent.png)
    add_custom_command(TARGET blackhole-sim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/icon_transparent.png
                $<TARGET_FILE_DIR:blackhole-sim>/icon_transparent.png)
    if(APPLE)
        add_custom_command(TARGET blackhole-sim POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${CMAKE_SOURCE_DIR}/icon_transparent.png
                    ${CMAKE_BINARY_DIR}/Aetherion.app/Contents/Resources/icon_transparent.png)
    endif()
endif()

# Background texture (optional) is often kept next to the executable in build/.
if(EXISTS ${CMAKE_SOURCE_DIR}/build/background.png)
    add_custom_command(TARGET blackhole-sim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/build/background.png
                $<TARGET_FILE_DIR:blackhole-sim>/background.png)
    add_custom_command(TARGET blackhole-3D POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/build/background.png
                $<TARGET_FILE_DIR:blackhole-3D>/background.png)
elseif(EXISTS ${CMAKE_SOURCE_DIR}/background.png)
    add_custom_command(TARGET blackhole-sim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/background.png
                $<TARGET_FILE_DIR:blackhole-sim>/background.png)
    add_custom_command(TARGET blackhole-3D POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/background.png
                $<TARGET_FILE_DIR:blackhole-3D>/background.png)
endif()

# ── Install targets ───────────────────────────────────────────
if(APPLE)
    install(TARGETS blackhole-sim blackhole-2D blackhole-3D RUNTIME DESTINATION bin BUNDLE DESTINATION .)
else()
    install(TARGETS blackhole-sim blackhole-2D blackhole-3D RUNTIME DESTINATION bin)
endif()

# Runtime data files (shaders, textures) loaded via ResourceManager::find().
# Installed to share/aetherionsuite/ so XDG_DATA_DIRS search (Flatpak/system) finds them.
install(FILES
    src/3D/BlackHole3D.frag
    src/3D/BlackHole3D_PhotorealDisk.frag
    DESTINATION share/aetherionsuite)

if(EXISTS ${CMAKE_SOURCE_DIR}/src/disk_texture.png)
    install(FILES src/disk_texture.png DESTINATION share/aetherionsuite)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/build/background.png)
    install(FILES build/background.png DESTINATION share/aetherionsuite)
elseif(EXISTS ${CMAKE_SOURCE_DIR}/background.png)
    install(FILES background.png DESTINATION share/aetherionsuite)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/icon.png)
    install(FILES icon.png DESTINATION share/aetherionsuite)
    install(FILES icon.png
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME io.github.ArandomHitman.AetherionSuite.png)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/icon_transparent.png)
    install(FILES icon_transparent.png DESTINATION share/aetherionsuite)
endif()

# Desktop entry and AppStream metadata (used by Flatpak and system package managers).
if(EXISTS ${CMAKE_SOURCE_DIR}/flatpak/io.github.ArandomHitman.AetherionSuite.desktop)
    install(FILES flatpak/io.github.ArandomHitman.AetherionSuite.desktop
        DESTINATION share/applications)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/flatpak/io.github.ArandomHitman.AetherionSuite.metainfo.xml)
    install(FILES flatpak/io.github.ArandomHitman.AetherionSuite.metainfo.xml
        DESTINATION share/metainfo)
endif()

# ── macOS .app bundle target ──────────────────────────────────
if(APPLE)
    add_custom_target(bundle
        COMMAND bash ${CMAKE_SOURCE_DIR}/bundle_app.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS blackhole-sim blackhole-2D blackhole-3D
        COMMENT "Assembling Aetherion.app bundle"
    )
endif()

# ── Physics regression tests (beta validation) ──────────────────────────────
enable_testing()

add_executable(physics-regression-tests
    tests/physics_regression_tests.cpp
)
target_include_directories(physics-regression-tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/2D
)

add_test(
    NAME physics-regression
    COMMAND physics-regression-tests
)
