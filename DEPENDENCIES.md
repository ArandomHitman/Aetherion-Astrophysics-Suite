# Dependencies

The dependency reference page for the Aetherion project.

---

## Build System

| Dependency | Version | Purpose |
|---|---|---|
| **CMake** | ≥ 3.10 | Build system generator |
| **C++17 compiler** | Clang / GCC / MSVC | Core language standard |
| **Make** (or Ninja) | Any | Build backend (default: Unix Makefiles) |

---

## Required Libraries

### SFML (Simple and Fast Multimedia Library)

| | Details |
|---|---|
| **Version** | 3.x |
| **Components** | Graphics, Window, System |
| **Used by** | All three executables (`blackhole-sim`, `blackhole-2D`, `blackhole-3D`) |
| **Purpose** | Window management, rendering, input handling, event loop, clock/timing |
| **Install (macOS)** | `brew install sfml` |
| **Install (Linux)** | `sudo apt install libsfml-dev` |
| **Website** | https://www.sfml-dev.org/ |

> **Note:** The project targets SFML 3.x. SFML 2.x is not supported due to API changes (e.g. `sf::Mouse::Button::Left`, `sf::ContextSettings` fields, `sf::VideoMode` constructor).

### GLM (OpenGL Mathematics)

| | Details |
|---|---|
| **Version** | Any recent release |
| **Used by** | `blackhole-3D` |
| **Purpose** | Vector/matrix math for 3D camera, transformations, and orbital mechanics |
| **Install (macOS)** | `brew install glm` |
| **Install (Linux)** | `sudo apt install libglm-dev` |
| **Website** | https://github.com/g-truc/glm |

> CMake first tries `find_package(glm CONFIG)`, then falls back to `find_package(GLM)`.

### OpenGL

| | Details |
|---|---|
| **Version** | 3.3 Core Profile |
| **GLSL** | `#version 330 core` |
| **Used by** | `blackhole-3D` |
| **Purpose** | GPU ray-marching, shader-based rendering, bloom pipeline, HUD overlay |
| **macOS** | Provided by the system (linked via `find_library(OpenGL)`) |
| **Linux** | Typically provided by GPU drivers; may need `libgl-dev` or Mesa |

> On macOS, `GL_SILENCE_DEPRECATION` is defined to suppress Apple's OpenGL deprecation warnings.

---

## Vendored Dependencies (included in `external/`)

These are bundled with the repository and do not need to be installed separately.

### Dear ImGui

| | Details |
|---|---|
| **Location** | `external/imgui/` |
| **Used by** | `blackhole-sim` (main menu / launcher) |
| **Purpose** | Immediate-mode GUI for the launcher interface |
| **License** | MIT |

### ImGui-SFML

| | Details |
|---|---|
| **Location** | `external/imgui-sfml/` |
| **Used by** | `blackhole-sim` |
| **Purpose** | Integration layer between Dear ImGui and SFML |
| **License** | MIT |

Both are compiled into a static library (`imgui`) as part of the CMake build.

---

## Linux Runtime Requirements

The Qt6 XCB platform plugin is required on Linux. Without it the app crashes immediately on launch with `qt.qpa.plugin: Could not find the Qt platform plugin "xcb"`.

| Distro | Package to install |
|---|---|
| Ubuntu / Debian | `sudo apt install qt6-qpa-plugins` |
| Fedora / RHEL | `sudo dnf install qt6-qtbase-gui` |
| Arch Linux | `sudo pacman -S qt6-base` (usually already pulled in) |
| openSUSE | `sudo zypper install libQt6Gui6` |

> The app forces `QT_QPA_PLATFORM=xcb` at startup (see `src/QT-LAUNCHER/main.cpp`) so it uses X11/XWayland rather than native Wayland, which SFML requires. Wayland users will still get a working window via XWayland.

---

## Transitive / Runtime Dependencies

These are pulled in automatically by SFML and do not require manual installation in most cases:

| Library | Pulled by | Notes |
|---|---|---|
| **FreeType** | SFML Graphics | Font rendering. On macOS: `libfreetype.6.dylib` |
| **libpng** | SFML Graphics / FreeType | PNG image loading. On macOS: `libpng16.16.dylib` |

The `bundle_app.sh` script handles copying and relinking these for macOS `.app` bundles. On Linux, `bundle_app.sh --platform linux` collects runtime `.so` files automatically via `ldd` and bundles the `libqxcb.so` platform plugin.

---

## Optional / Development Dependencies

### Python (for disk texture generation)

| | Details |
|---|---|
| **Script** | `makedisktexture.py` |
| **Requires** | Python 3 with [Pillow](https://pypi.org/project/Pillow/) (`pip install Pillow`) |
| **Purpose** | Generates `src/disk_texture.png` (accretion disk texture) |
| **Required?** | No — only needed if you want to regenerate the texture |

### Homebrew (macOS only)

Recommended package manager for installing SFML, GLM, and CMake on macOS:

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
brew install sfml glm cmake
```

---

## Platform Support Matrix

| Platform | Compiler | Status | Notes |
|---|---|---|---|
| **macOS (ARM/Intel)** | Apple Clang | Supported | Primary development platform; `.app` bundling via `bundle_app.sh` |
| **Linux (x86_64)** | GCC / Clang | Supported | Tested with apt-based distros (Ubuntu/Debian) |
| **Windows** | MSVC | Untested | Should work with vcpkg-provided SFML 3.x and GLM |

---

## Version Summary

```
C++ Standard:    C++17
CMake:           ≥ 3.10
SFML:            3.x
GLM:             Any
OpenGL:          3.3 Core
GLSL:            #version 330 core
Dear ImGui:      Vendored (see external/imgui/)
ImGui-SFML:      Vendored (see external/imgui-sfml/)
```

``
This document will be updated as dependencies evolve. Always refer to the latest version in the repository for the most accurate information.
``