# FAQ & Troubleshooting

Common questions and solutions for the Aetherion project.

---

## Build Issues

### CMake can't find SFML

**Error:**
```
Could not find a package configuration file provided by "SFML"
```

**Solution:**
- Make sure SFML **3.x** is installed (not 2.x).
- macOS: `brew install sfml`
- Linux: `sudo apt install libsfml-dev` — check that your distro provides SFML 3.x. If not, build SFML from source.
- Windows: Use vcpkg or build SFML 3.x from source. Install via vcpkg:
  ```bash
  vcpkg install sfml glm
  ```
- FreeBSD: yeah no I ain't adding compat for that bud, best you switch to something mainstream

- If SFML is installed in a non-standard location, pass its path to CMake:
  ```bash
  cmake -DCMAKE_PREFIX_PATH=/path/to/sfml ..
  ```

### CMake can't find GLM

**Error:**
```
Could not find a package configuration file provided by "GLM"
```

**Solution:**
- macOS: `brew install glm`
- Linux: `sudo apt install libglm-dev`
- The CMake config tries both `glm` (lowercase) and `GLM` (uppercase) package names. If neither works, set the include path manually:
  ```bash
  cmake -DGLM_INCLUDE_DIRS=/path/to/glm/include ..
  ```

### SFML 2.x vs 3.x API errors

**Error (examples):**
```
error: no member named 'antiAliasingLevel' in 'sf::ContextSettings'
error: no matching constructor for 'sf::VideoMode'
```

**Cause:** The project requires SFML 3.x. SFML 2.x has a different API.

**Solution:**
- Upgrade to SFML 3.x.
- macOS: `brew upgrade sfml` (Homebrew provides 3.x).
- Linux: If your package manager only has 2.x, build SFML 3.x from source (https://github.com/SFML/SFML).
- Windows: Use vcpkg or build SFML 3.x from source. Install via vcpkg:
  ```bash
  vcpkg install sfml glm
  ```

### OpenGL deprecation warnings on macOS

**Warning:**
```
'glGenFramebuffers' is deprecated: first deprecated in macOS 10.14
```

**Cause:** Apple deprecated OpenGL in favour of Metal. The project defines `GL_SILENCE_DEPRECATION` to suppress these, but some may still appear depending on compiler flags.

**Solution:** These warnings are cosmetic and can be safely ignored. The simulator works fine with macOS's OpenGL implementation.

### `build-essential` / no compiler found (Linux)

**Error:**
```
No CMAKE_CXX_COMPILER could be found.
```

**Solution:**
```bash
sudo apt install build-essential
```

---

## Runtime Issues

### Black screen on launch (`blackhole-3D`)

**Possible causes:**
1. **OpenGL 3.3 not supported** — The simulator requires OpenGL 3.3 Core. Check your GPU/driver support:
   ```bash
   glxinfo | grep "OpenGL version"   # Linux
   ```
   On macOS, OpenGL 3.3 is supported on all Macs from 2012 onwards.
   On windows, check your GPU specs online, or use a tool like GPU-Z. Corrupted drivers can also cause this, so use DDU or something along that line.

2. **Shader files missing** — The build copies `.frag` files next to the executable. If you moved the binary, make sure `BlackHole3D.frag` and `BlackHole3D_PhotorealDisk.frag` are in the same directory as the executable, or in the project's `src/3D/` directory.

3. **Missing textures** — Background or disk textures are optional but the shader may render a black disk without them. See the [Textures](#textures-not-loading) section.

### Shaders fail to compile at runtime

**Error (in terminal output):**
```
Shader compilation failed: ...
```

**Solution:**
- Ensure your GPU supports GLSL `#version 330 core`.
- If you edited a `.frag` file, check for syntax errors. The simulator loads shaders from disk at startup, so edits take effect on next launch without recompiling.
- On Linux with Intel integrated graphics, install the latest Mesa drivers:
  ```bash
  sudo apt install mesa-utils libgl1-mesa-dri
  ```

### Textures not loading

**Symptoms:** Black or missing accretion disk, no background stars.

**Solution:**
- **Disk texture:** Run the generator script (requires Python 3 + Pillow):
  ```bash
  pip install Pillow
  python makedisktexture.py
  ```
  This creates `src/disk_texture.png`. Rebuild to copy it to the build directory.
- **Background texture:** Place a `background.png` in the project root or `build/` directory. The build system copies it automatically.

### Low FPS / poor performance

**Tips:**
- The 3D simulator is GPU-bound (full-screen ray marching every frame). A discrete GPU is recommended for good performance.
- Reduce the window size, don't use fullscreen to avoid fps stuttering.
- Toggle off expensive features:
  - **J** — disable relativistic jets
  - **G** — disable Broad Line Region
  - **V** — disable Doppler beaming
- Check that your system isn't falling back to software rendering (integrated GPU), for windows sometimes task manager priority going to very high/realtime can fix this.

### Mouse cursor stuck / input not working

- Press **Esc** to release the mouse cursor from the window.
- Right-click and hold to enter freelook mode; release to stop.
- Press **F** to toggle between freelook and orbit camera modes.

---

## macOS-Specific Issues

### `.app` bundle won't open ("damaged" or "unidentified developer")

**Cause:** The bundle isn't code-signed.

**Solution:**
```bash
# Remove quarantine attribute
xattr -cr build/Aetherion.app
```

Or right-click the app → Open → confirm in the dialog.

### `bundle_app.sh` warnings about missing dylibs

**Warning:**
```
WARNING: /opt/homebrew/opt/glm/lib/libglm.dylib not found, skipping.
```

**Cause:** GLM is header-only on some Homebrew versions and doesn't produce a `.dylib`.

**Solution:** This warning is harmless for GLM since it's header-only at runtime. For other missing dylibs (SFML, FreeType, libpng), reinstall the relevant package:
```bash
brew reinstall sfml freetype libpng
```

### Homebrew on Apple Silicon vs Intel

The build system checks both `/opt/homebrew` (Apple Silicon) and `/usr/local` (Intel) prefix paths. If CMake still can't find packages:
```bash
cmake -DCMAKE_PREFIX_PATH="$(brew --prefix)" ..
```

---

## Linux-Specific Issues

### App crashes immediately ("Could not find the Qt platform plugin xcb")

**Error:**
```
qt.qpa.plugin: Could not find the Qt platform plugin "xcb" in ""
This application failed to start because no Qt platform plugin could be initialized.
```

**Cause:** The Qt6 XCB platform plugin is missing. The app forces `QT_QPA_PLATFORM=xcb` on Linux so SFML can embed into a Qt widget via X11/XWayland. If the `xcb` plugin isn't installed, Qt can't open any window.

**Solution:**
```bash
# Ubuntu / Debian
sudo apt install qt6-qpa-plugins

# Fedora
sudo dnf install qt6-qtbase-gui

# Arch Linux
sudo pacman -S qt6-base
```
If you're using the bundled tarball (`bundle_app.sh --platform linux`), the plugin is already included under `plugins/platforms/libqxcb.so`.

### No OpenGL context / `GLXBadFBConfig`

**Cause:** X11/Wayland driver issue or missing OpenGL support.

**Solution:**
```bash
sudo apt install libgl1-mesa-dri libgl1-mesa-glx mesa-utils
```
Verify with `glxinfo | grep "OpenGL version"` — you need 3.3+.

### SFML linking errors with system packages

**Error:**
```
undefined reference to `sf::RenderWindow::...'
```

**Cause:** Mismatched SFML version (system has 2.x, project needs 3.x).

**Solution:** Build SFML 3.x from source:
```bash
git clone https://github.com/SFML/SFML.git
cd SFML && mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
cmake --build . && sudo cmake --install .
```
Then rebuild the project with:
```bash
cmake -DCMAKE_PREFIX_PATH=/usr/local ..
```


---------- RANT STARTS HERE ----------

Note: No, I am not adding support for other versions of SFML or any graphical library. 
MacOS only supports OpenGL 3.3, so anything 2x for SFML is not going to happen for cross-compatibility sake.
If you want to use an older or different version/alternative to any library here, fork this project and change the code to support it yourself. I will not be doing it for you.
If you want to add support directly to this project, make a Pull Request. Any help here is appreciated, but my priority is in maintaining the codebase and adding research 
grade features. Quality over quantity.


---------- RANT OVER ----------

---

## General FAQ

### What are the three executables?

| Executable | Description |
|---|---|
| `blackhole-sim` | Main launcher menu (ImGui interface to choose which simulator to run) |
| `blackhole-2D` | 2D Schwarzschild null-geodesic simulator | if behaves as expected should be able to run either standalone (terminal) or in the aetherion app
| `blackhole-3D` | 3D Kerr-metric ray-marched black hole with accretion disk, jets, bloom |, same case as above.

### Can I edit shaders without recompiling?

The `.frag` shader files are loaded from disk at runtime. Edit `BlackHole3D.frag` or `BlackHole3D_PhotorealDisk.frag` in the build directory and relaunch the simulator. Do so at your own risk

### What units does the simulation use?

Simulation units are in standarized Schwarzschild radii (Rs = 1). For reference, TON 618's Schwarzschild radius is approximately 1,300 AU (1.95×10¹⁴ m).

### How do I switch between black hole presets?

The 3D simulator includes presets for TON 618, Sgr A*, M87*, and others. Check the HUD overlay (**H** to toggle) and key bindings for preset switching. Presets are defined in [src/3D/presets.hpp](src/3D/presets.hpp).
  Note: You can change the keybinds to whatever you want at any time. Just edit the keybinds in the aetherion app, though try not to create conflicts.

### Does this work on Windows?

The codebase is cross-platform C++17. While I (Liam N, main and only lead developer) made this on MACOS and Asahi Linux, my testing for windows is very limited for now.
If you are open to beta testing here, you can try building with MSVC or MinGW. You will need:
- MSVC or MinGW with C++17 support
- SFML 3.x (via [vcpkg](https://vcpkg.io/): `vcpkg install sfml glm`)
- CMake ≥ 3.10
- A GPU with OpenGL 3.3 support

### How do I regenerate the accretion disk texture?

do this:
```bash
pip install Pillow
python makedisktexture.py
```

This writes the `src/disk_texture.png`. Rebuild to have it copied to `build/`.

### Can you add `insert blackhole here` 

**Answer:** Maybe. If it's a significant enough discovery and is highly requested, I can add a preset for it.
Otherwise, try making a custom preset based on properties yourself. In future, I may make a plugin system
for adding more properties, and maybe a hub of sorts to share custom black hole files if this somehow gets enough traction.


### FreeBSD?

**Answer:** I have zero idea why I got asked by several people for this, but absolutely not.
I have zero interest in supporting FreeBSD. If you want to run this on FreeBSD, 
you can try building it yourself and fixing any issues that come up, but I will not be supporting you on this.
This is an open source research tool, not a game.