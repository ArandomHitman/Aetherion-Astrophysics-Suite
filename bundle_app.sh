#!/bin/bash
# ============================================================
# bundle_app.sh — Package and optionally install Aetherion
#
# Usage:
#   ./bundle_app.sh                           # auto-detect platform
#   ./bundle_app.sh --platform mac            # macOS  → build/Aetherion.app
#   ./bundle_app.sh --platform linux          # Linux  → build/Aetherion-linux/
#   ./bundle_app.sh --platform windows        # Windows → build/Aetherion-windows/ + .zip
#   ./bundle_app.sh [--platform X] --install  # bundle then install to system location
#
# Prerequisites:
#   macOS:   Xcode CLI tools (install_name_tool, codesign), Homebrew deps installed
#   Linux:   ldd (coreutils), patchelf (apt install patchelf)
#   Windows: zip; run from Git Bash / MSYS2 with a Windows build present in build/
#
# Always run 'cmake --build build' before this script.
# ============================================================
set -euo pipefail

APP_NAME="Aetherion"
BUILD_DIR="build"
VERSION="1.0.0"

# ── Code-signing configuration ────────────────────────────────
# Set APPLE_SIGNING_IDENTITY in your environment, or pass --sign-identity.
# Use "-" for ad-hoc signing (no Apple Developer account required).
# For distribution, set to the exact certificate name shown in Keychain Access,
# e.g. "Developer ID Application: Your Name (TEAMID)"
SIGN_IDENTITY="${APPLE_SIGNING_IDENTITY:--}"

# ── Argument parsing ──────────────────────────────────────────
PLATFORM=""
DO_INSTALL=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --platform)      PLATFORM="${2:?'--platform requires an argument (mac|linux|windows)'}"; shift 2 ;;
        --platform=*)    PLATFORM="${1#--platform=}"; shift ;;
        --install)       DO_INSTALL=true; shift ;;
        --sign-identity)   SIGN_IDENTITY="${2:?'--sign-identity requires a value'}"; shift 2 ;;
        --sign-identity=*) SIGN_IDENTITY="${1#--sign-identity=}"; shift ;;
        -h|--help)       sed -n '2,11p' "$0" | sed 's/^# \?//'; exit 0 ;;
        *) printf 'ERROR: Unknown argument: %s\nRun with --help for usage.\n' "$1" >&2; exit 1 ;;
    esac
done

# ── Platform auto-detection ───────────────────────────────────
if [[ -z "$PLATFORM" ]]; then
    case "$(uname -s)" in
        Darwin)              PLATFORM="mac"     ;;
        Linux)               PLATFORM="linux"   ;;
        MINGW*|MSYS*|CYGWIN*) PLATFORM="windows" ;;
        *)
            printf 'ERROR: Cannot auto-detect platform. Use --platform mac|linux|windows\n' >&2
            exit 1 ;;
    esac
    echo "  Auto-detected platform: ${PLATFORM}"
fi

case "$PLATFORM" in
    mac|linux|windows) ;;
    *) printf 'ERROR: Unknown platform "%s". Use mac, linux, or windows.\n' "$PLATFORM" >&2; exit 1 ;;
esac

echo "=== Packaging ${APP_NAME} v${VERSION} for ${PLATFORM} ==="

# ── Sanity check: build output must exist ────────────────────
if [[ ! -f "${BUILD_DIR}/blackhole-sim" && ! -f "${BUILD_DIR}/blackhole-sim.exe" ]]; then
    printf 'ERROR: No executable found in %s/. Run "cmake --build build" first.\n' "${BUILD_DIR}" >&2
    exit 1
fi

# ── Shared: copy shaders, textures, and icons ────────────────
copy_resources() {
    local dest="$1"
    mkdir -p "${dest}"

    # 3D shaders (prefer build-dir copies over source, as they're more up-to-date)
    for f in BlackHole3D.frag BlackHole3D_PhotorealDisk.frag; do
        if [ -f "${BUILD_DIR}/${f}" ]; then
            cp "${BUILD_DIR}/${f}" "${dest}/${f}"
        elif [ -f "src/3D/${f}" ]; then
            cp "src/3D/${f}" "${dest}/${f}"
        fi
    done

    # Bloom / post-process shaders
    for f in shaders/*.frag shaders/*.vert; do
        [ -f "$f" ] && cp "$f" "${dest}/$(basename "$f")"
    done

    # Textures (prioritize: build-dir > source > project-root)
    if [ -f "${BUILD_DIR}/background.png" ]; then
        cp "${BUILD_DIR}/background.png" "${dest}/background.png"
    elif [ -f "background.png" ]; then
        cp "background.png" "${dest}/background.png"
    fi

    if [ -f "src/3D/disk_texture.png" ]; then
        cp "src/3D/disk_texture.png" "${dest}/disk_texture.png"
    fi

    # Icons (use first found, in priority order)
    if [ -f "AppIcon.icns" ]; then
        cp "AppIcon.icns" "${dest}/AppIcon.icns"
    fi
    if [ -f "AppIcon.png" ]; then
        cp "AppIcon.png" "${dest}/AppIcon.png"
    fi
    if [ -f "icon.png" ]; then
        cp "icon.png" "${dest}/icon.png"
    fi
    if [ -f "icon_transparent.png" ]; then
        cp "icon_transparent.png" "${dest}/icon_transparent.png"
    fi
    return 0
}

# ══════════════════════════════════════════════════════════════
# macOS — .app bundle
# ══════════════════════════════════════════════════════════════
bundle_mac() {
    local BUNDLE="${BUILD_DIR}/${APP_NAME}.app"
    local CONTENTS="${BUNDLE}/Contents"
    local MACOS_DIR="${CONTENTS}/MacOS"
    local RESOURCES="${CONTENTS}/Resources"
    local FRAMEWORKS="${CONTENTS}/Frameworks"

    echo "  Building ${BUNDLE} …"
    rm -rf "${BUNDLE}"
    mkdir -p "${MACOS_DIR}" "${RESOURCES}" "${FRAMEWORKS}"

    # Executables — unified build produces blackhole-sim; legacy targets are optional
    local copied_exe=false
    for exe in blackhole-sim blackhole-2D blackhole-3D; do
        if [ -f "${BUILD_DIR}/${exe}" ]; then
            cp "${BUILD_DIR}/${exe}" "${MACOS_DIR}/${exe}"
            copied_exe=true
        fi
    done
    $copied_exe || { echo "ERROR: No executables found in ${BUILD_DIR}." >&2; exit 1; }
    echo "  Executables copied."

    copy_resources "${RESOURCES}"
    echo "  Resources copied."

    # ── Info.plist (written before macdeployqt so it can read bundle metadata) ─
    cat > "${CONTENTS}/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>                      <string>${APP_NAME}</string>
    <key>CFBundleDisplayName</key>               <string>Aetherion</string>
    <key>CFBundleIdentifier</key>                <string>com.liamnayyar.aetherion</string>
    <key>CFBundleVersion</key>                   <string>${VERSION}</string>
    <key>CFBundleShortVersionString</key>         <string>${VERSION}</string>
    <key>CFBundleExecutable</key>                <string>blackhole-sim</string>
    <key>CFBundlePackageType</key>               <string>APPL</string>
    <key>CFBundleInfoDictionaryVersion</key>      <string>6.0</string>
    <key>NSHighResolutionCapable</key>            <true/>
    <key>CFBundleIconFile</key>                  <string>AppIcon</string>
    <key>LSMinimumSystemVersion</key>             <string>13.0</string>
    <key>NSSupportsAutomaticGraphicsSwitching</key> <true/>
</dict>
</plist>
PLIST
    echo "  Info.plist written."

    # ── Step 1: Deploy Qt with macdeployqt ───────────────────────────────────
    # macdeployqt copies Qt frameworks + the macOS platform plugin (libqcocoa),
    # writes qt.conf, and rewrites all Qt load paths in the executables to
    # @executable_path/../Frameworks — no hardcoded Homebrew paths left behind.
    local MACDEPLOYQT=""
    for _candidate in \
        "$(command -v macdeployqt 2>/dev/null || true)" \
        "$(brew --prefix qt 2>/dev/null)/bin/macdeployqt" \
        "$(brew --prefix qt@6 2>/dev/null)/bin/macdeployqt" \
        /opt/homebrew/opt/qt/bin/macdeployqt \
        /usr/local/opt/qt/bin/macdeployqt; do
        [[ -x "${_candidate}" ]] && { MACDEPLOYQT="${_candidate}"; break; }
    done

    if [[ -n "${MACDEPLOYQT}" ]]; then
        echo "  Running macdeployqt (${MACDEPLOYQT}) …"
        # -no-plugins: prevents macdeployqt from pulling in every Qt plugin it
        # can find (pdf, virtualkeyboard, svg …) which may reference Qt
        # frameworks not present on this machine and produce rpath errors.
        # We copy only the plugin categories the app actually needs below.
        "${MACDEPLOYQT}" "${BUNDLE}" -always-overwrite -no-plugins 2>&1 | sed 's/^/    /'
        echo "  Qt frameworks deployed."

        # Derive the Qt prefix from the macdeployqt path (works for Homebrew
        # formulas that install into per-formula cellar dirs, e.g. qtbase/qtcharts).
        local _QT_PREFIX
        _QT_PREFIX="$(cd "$(dirname "${MACDEPLOYQT}")/.." && pwd)"
        # Some Homebrew Qt installs use libexec/plugins, others use lib/Qt*.framework/../plugins
        local _QT_PLUGINS_DIR=""
        for _candidate in \
            "${_QT_PREFIX}/libexec/plugins" \
            "${_QT_PREFIX}/share/qt/plugins" \
            "${_QT_PREFIX}/plugins"; do
            [[ -d "${_candidate}" ]] && { _QT_PLUGINS_DIR="${_candidate}"; break; }
        done

        if [[ -n "${_QT_PLUGINS_DIR}" ]]; then
            # Only copy plugin categories actually needed by the app.
            # platforms  – required (Qt cannot start without the cocoa plugin)
            # styles     – optional but recommended for correct native look
            # imageformats – needed for PNG/JPEG icon loading
            # iconengines  – needed for SVG-based icons in Qt widgets
            for _cat in platforms styles imageformats iconengines; do
                if [[ -d "${_QT_PLUGINS_DIR}/${_cat}" ]]; then
                    mkdir -p "${CONTENTS}/PlugIns/${_cat}"
                    cp -RL "${_QT_PLUGINS_DIR}/${_cat}/." "${CONTENTS}/PlugIns/${_cat}/" 2>/dev/null || true
                fi
            done
            echo "  Qt plugins copied (platforms, styles, imageformats, iconengines)."

            # Fix rpaths in every plugin dylib so they resolve Qt frameworks
            # from the bundle's Frameworks/ dir at runtime.
            # Plugins live two levels below Contents/ (Contents/PlugIns/<cat>/),
            # so the correct loader-relative rpath is @loader_path/../../Frameworks.
            find "${CONTENTS}/PlugIns" -name '*.dylib' | while read -r _plug; do
                chmod u+w "${_plug}"
                while IFS= read -r _rp; do
                    install_name_tool -delete_rpath "${_rp}" "${_plug}" 2>/dev/null || true
                done < <(otool -l "${_plug}" 2>/dev/null \
                    | awk '/LC_RPATH/{f=1} f && /[[:space:]]path /{print $2; f=0}')
                install_name_tool -add_rpath "@loader_path/../../Frameworks" "${_plug}" 2>/dev/null || true
            done
        else
            echo "  WARNING: Qt plugins directory not found under ${_QT_PREFIX} — app may not start."
        fi
    else
        echo "  WARNING: macdeployqt not found — falling back to manual Qt copy."
        echo "           Install Qt via Homebrew:  brew install qt"
        local _HB_FB
        _HB_FB="$(brew --prefix 2>/dev/null || echo /opt/homebrew)"
        local _QT_LIB_FB="${_HB_FB}/opt/qt/lib"
        local _QT_PLG_FB="${_HB_FB}/opt/qt/libexec/plugins"
        for fw in QtCore QtGui QtWidgets; do
            [[ -d "${_QT_LIB_FB}/${fw}.framework" ]] \
                && cp -R "${_QT_LIB_FB}/${fw}.framework" "${FRAMEWORKS}/"
        done
        if [[ -d "${_QT_PLG_FB}/platforms" ]]; then
            mkdir -p "${CONTENTS}/PlugIns/platforms"
            cp -RL "${_QT_PLG_FB}/platforms/." "${CONTENTS}/PlugIns/platforms/"
        fi
        # qt.conf tells Qt where to find the PlugIns dir at runtime
        printf '[Paths]\nPlugins = ../PlugIns\n' > "${RESOURCES}/qt.conf"
        echo "  Qt frameworks copied (manual fallback)."
    fi

    # ── Step 2: Bundle non-Qt dylibs (SFML, freetype, libpng, etc.) ─────────
    # We discover every non-system, non-Qt dylib the executables need at runtime
    # via `otool -L`, then recursively collect transitive dependencies — no
    # hardcoded library names or Homebrew paths anywhere.

    # Match system paths and already-relative (@-prefixed) references to skip.
    # Qt paths (containing "/Qt") are handled by macdeployqt above.
    local _SKIP_RE='^(/usr/lib|/System/Library|@)'

    # Print absolute external dep paths of a binary (non-system, non-Qt, non-@)
    _mac_ext_deps() {
        otool -L "$1" 2>/dev/null \
            | tail -n +2 \
            | awk '{print $1}' \
            | grep -vE "${_SKIP_RE}" \
            | grep -v '/Qt' \
            | grep -v '^$' \
            || true
    }

    # Recursively copy a dylib and its transitive deps into Frameworks/,
    # setting their install names to @rpath-relative and fixing internal refs.
    _mac_bundle_dylib() {
        local _src="$1"
        local _name
        _name="$(basename "${_src}")"
        local _dst="${FRAMEWORKS}/${_name}"
        [[ -f "${_dst}" ]] && return 0   # already bundled
        if [[ ! -f "${_src}" ]]; then
            echo "    WARNING: ${_src} not found — skipping."
            return 0
        fi
        cp -Lf "${_src}" "${_dst}"       # -L dereferences symlinks
        chmod u+w "${_dst}"
        # Set this dylib's own install name to @rpath-relative
        install_name_tool -id "@rpath/${_name}" "${_dst}" 2>/dev/null || true
        # Rewrite this dylib's deps and recurse into them
        while IFS= read -r _dep; do
            local _depname
            _depname="$(basename "${_dep}")"
            install_name_tool -change "${_dep}" "@rpath/${_depname}" "${_dst}" 2>/dev/null || true
            _mac_bundle_dylib "${_dep}"
        done < <(_mac_ext_deps "${_dst}")
    }

    echo "  Collecting non-Qt dylib dependencies …"
    for exe in blackhole-sim blackhole-2D blackhole-3D; do
        [[ -f "${MACOS_DIR}/${exe}" ]] || continue
        while IFS= read -r dep; do
            _mac_bundle_dylib "${dep}"
        done < <(_mac_ext_deps "${MACOS_DIR}/${exe}")
    done
    local _dylib_count
    _dylib_count="$(find "${FRAMEWORKS}" -maxdepth 1 -name '*.dylib' 2>/dev/null | wc -l | tr -d ' ')"
    echo "  Bundled ${_dylib_count} dylib(s) into Frameworks/."

    # ── Step 3: Fix executables — rewrite load paths and set RPATH ──────────
    # Each executable gets:
    #   • Absolute Homebrew dep paths → @rpath/name.dylib
    #   • All existing LC_RPATH entries removed (Homebrew dirs, build dirs, etc.)
    #   • A single new LC_RPATH: @executable_path/../Frameworks
    #     This makes the dynamic linker resolve every @rpath/... reference —
    #     both our flat dylibs and any Qt frameworks — from inside the bundle.

    _mac_fix_exe() {
        local _bin="$1"
        # Rewrite remaining absolute dep paths to @rpath-relative
        while IFS= read -r _dep; do
            local _depname
            _depname="$(basename "${_dep}")"
            install_name_tool -change "${_dep}" "@rpath/${_depname}" "${_bin}" 2>/dev/null || true
        done < <(_mac_ext_deps "${_bin}")
        # Strip every existing rpath (Homebrew dirs, CMake build dirs, etc.)
        while IFS= read -r _rp; do
            install_name_tool -delete_rpath "${_rp}" "${_bin}" 2>/dev/null || true
        done < <(otool -l "${_bin}" 2>/dev/null \
            | awk '/LC_RPATH/{f=1} f && /[[:space:]]path /{print $2; f=0}')
        # Add the single canonical bundle-relative rpath
        install_name_tool -add_rpath "@executable_path/../Frameworks" "${_bin}" 2>/dev/null || true
    }

    echo "  Fixing executable load paths and rpaths …"
    for exe in blackhole-sim blackhole-2D blackhole-3D; do
        [[ -f "${MACOS_DIR}/${exe}" ]] || continue
        _mac_fix_exe "${MACOS_DIR}/${exe}"
        echo "    Fixed rpath: ${exe}"
    done
    echo "  Load paths and rpaths fixed."

    # ── Code-sign ──────────────────────────────────────────────
    if [[ "${SIGN_IDENTITY}" == "-" ]]; then
        echo "  Code signing (ad-hoc, no Apple Developer account required) …"
        # Sign innermost components first, then the bundle.
        for dylib in "${FRAMEWORKS}"/*.dylib; do
            [[ -f "${dylib}" ]] || continue
            codesign --force -s - "${dylib}" 2>/dev/null || true
        done
        for fw in "${FRAMEWORKS}"/*.framework; do
            [[ -d "${fw}" ]] || continue
            codesign --force --deep -s - "${fw}" 2>/dev/null || true
        done
        # Sign plugin dylibs before the executables that load them
        find "${CONTENTS}/PlugIns" -name '*.dylib' 2>/dev/null | while read -r _plug; do
            codesign --force -s - "${_plug}" 2>/dev/null || true
        done
        for exe in blackhole-sim blackhole-2D blackhole-3D; do
            [[ -f "${MACOS_DIR}/${exe}" ]] || continue
            codesign --force -s - "${MACOS_DIR}/${exe}" 2>/dev/null || true
        done
        codesign --force -s - "${BUNDLE}" 2>/dev/null \
            && echo "  Code signed (ad-hoc)." \
            || echo "  WARNING: codesign failed — the app may still run normally."
    else
        echo "  Code signing with: ${SIGN_IDENTITY}"
        # Sign bundled dylibs first, then frameworks, then each executable,
        # then the bundle itself — innermost components must be signed first.
        for dylib in "${FRAMEWORKS}"/*.dylib; do
            [[ -f "${dylib}" ]] || continue
            codesign --force --options runtime --timestamp \
                -s "${SIGN_IDENTITY}" "${dylib}" 2>/dev/null || true
        done
        for fw in "${FRAMEWORKS}"/*.framework; do
            [[ -d "${fw}" ]] || continue
            codesign --force --deep --options runtime --timestamp \
                -s "${SIGN_IDENTITY}" "${fw}" 2>/dev/null || true
        done
        # Sign plugin dylibs before the executables that load them
        find "${CONTENTS}/PlugIns" -name '*.dylib' 2>/dev/null | while read -r _plug; do
            codesign --force --options runtime --timestamp \
                -s "${SIGN_IDENTITY}" "${_plug}" 2>/dev/null || true
        done
        for exe in blackhole-sim blackhole-2D blackhole-3D; do
            [[ -f "${MACOS_DIR}/${exe}" ]] || continue
            codesign --force --options runtime --timestamp \
                -s "${SIGN_IDENTITY}" "${MACOS_DIR}/${exe}"
        done
        codesign --force --options runtime --timestamp \
            -s "${SIGN_IDENTITY}" "${BUNDLE}" \
            && echo "  Code signed with Developer ID (hardened runtime)." \
            || { echo "ERROR: codesign failed. Verify your certificate is in the keychain." >&2; exit 1; }
    fi

    printf '\n  Output: %s  (%s)\n' "${BUNDLE}" "$(du -sh "${BUNDLE}" | cut -f1)"
    printf '  Run:    open %s\n'    "${BUNDLE}"
}

install_mac() {
    local BUNDLE="${BUILD_DIR}/${APP_NAME}.app"
    echo "  Installing to /Applications/${APP_NAME}.app …"
    rm -rf "/Applications/${APP_NAME}.app"
    cp -R "${BUNDLE}" "/Applications/${APP_NAME}.app"
    echo "  Installed. Launch from /Applications or Spotlight."
}

# ══════════════════════════════════════════════════════════════
# Linux — self-contained directory with launcher + .desktop entry
# ══════════════════════════════════════════════════════════════
bundle_linux() {
    local OUT="${BUILD_DIR}/Aetherion-linux"
    local BIN_DIR="${OUT}/bin"
    local LIB_DIR="${OUT}/lib"
    local RES_DIR="${OUT}/resources"

    echo "  Building ${OUT} …"
    rm -rf "${OUT}"
    mkdir -p "${BIN_DIR}" "${LIB_DIR}" "${RES_DIR}"

    # Binary
    cp "${BUILD_DIR}/blackhole-sim" "${BIN_DIR}/blackhole-sim"
    chmod +x "${BIN_DIR}/blackhole-sim"
    echo "  Executable copied."

    # Collect runtime shared libraries via ldd.
    # We bundle SFML, Qt6, GLEW, freetype, libpng, and their transitive non-system deps.
    # Core system libs (libc, libm, libpthread, X11, GL drivers) are intentionally
    # left to the host — bundling them causes more problems than it solves.
    local SKIP_PATTERN
    SKIP_PATTERN='linux-vdso|ld-linux|libdl\.so|libpthread|libm\.so|libc\.so|librt\.so'
    SKIP_PATTERN+="|libgcc_s|libstdc\+\+|libX11|libxcb|libGL\.so|libEGL|libnvidia|libdrm|libwayland|libdbus"

    if command -v ldd &>/dev/null; then
        while IFS= read -r line; do
            # ldd format: "  libfoo.so.N => /path/to/libfoo.so.N (0xADDR)"
            local sopath
            sopath="$(echo "${line}" | awk '{print $3}')"
            [[ -z "${sopath}" || "${sopath}" == "not" ]] && continue
            [[ ! -f "${sopath}" ]] && continue
            echo "${sopath}" | grep -qE "${SKIP_PATTERN}" && continue
            cp --preserve=links "${sopath}" "${LIB_DIR}/" 2>/dev/null || true
        done < <(ldd "${BIN_DIR}/blackhole-sim" 2>/dev/null)
        echo "  Runtime libs collected ($(ls "${LIB_DIR}" | wc -l) file(s))."
    else
        echo "  WARNING: ldd not found — skipping automatic library collection."
        echo "           Manually place required .so files in ${LIB_DIR}/."
    fi

    # Qt xcb platform plugin — required for SFML embedding inside Qt widgets
    local QT_XCB_DIRS=(
        "/usr/lib/x86_64-linux-gnu/qt6/plugins/platforms"
        "/usr/lib/qt6/plugins/platforms"
        "/usr/lib64/qt6/plugins/platforms"
        "/usr/lib/aarch64-linux-gnu/qt6/plugins/platforms"
    )
    for pd in "${QT_XCB_DIRS[@]}"; do
        if [ -f "${pd}/libqxcb.so" ]; then
            mkdir -p "${OUT}/plugins/platforms"
            cp "${pd}/libqxcb.so" "${OUT}/plugins/platforms/"
            echo "  Qt xcb platform plugin copied from ${pd}."
            break
        fi
    done

    # Use patchelf to embed RPATH so bundled libs are found without LD_LIBRARY_PATH.
    # The launcher script below provides a fallback if patchelf is unavailable.
    if command -v patchelf &>/dev/null; then
        patchelf --set-rpath '$ORIGIN/../lib' "${BIN_DIR}/blackhole-sim"
        echo "  RPATH set to \$ORIGIN/../lib."
    else
        echo "  NOTE: patchelf not found — launcher will use LD_LIBRARY_PATH instead."
        echo "        Install with: sudo apt install patchelf"
    fi

    copy_resources "${RES_DIR}"
    echo "  Resources copied."

    # Launcher script: sets up environment and execs the real binary
    cat > "${OUT}/aetherion" <<'LAUNCHER'
#!/bin/bash
# Aetherion launcher script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Prepend bundled libs (fallback if RPATH was not set by patchelf)
export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

# Point Qt to the bundled platform plugin
export QT_PLUGIN_PATH="${SCRIPT_DIR}/plugins"

# SFML embeds via X11/XCB native window handle — force XCB backend
# unless the caller has already set a preference
if [[ -z "${QT_QPA_PLATFORM:-}" ]]; then
    export QT_QPA_PLATFORM=xcb
fi

exec "${SCRIPT_DIR}/bin/blackhole-sim" "$@"
LAUNCHER
    chmod +x "${OUT}/aetherion"
    echo "  Launcher script written."

    # .desktop entry (install path is /opt/Aetherion; used by install_linux)
    cat > "${OUT}/aetherion.desktop" <<DESKTOP
[Desktop Entry]
Version=1.0
Type=Application
Name=Aetherion
GenericName=Black Hole Simulator
Comment=Real-time GPU ray-marched black hole visualisation
Exec=/opt/Aetherion/aetherion %u
Icon=/opt/Aetherion/resources/AppIcon.png
Terminal=false
Categories=Science;Education;
Keywords=black hole;physics;astrophysics;simulation;
DESKTOP
    echo "  .desktop entry written."

    printf '\n  Output: %s  (%s)\n' "${OUT}" "$(du -sh "${OUT}" | cut -f1)"
    printf '  Run:    %s/aetherion\n' "${OUT}"

    # Package into a distributable tarball
    local TARBALL="${BUILD_DIR}/${APP_NAME}-linux-x86_64.tar.gz"
    echo "  Creating tarball …"
    tar -czf "${TARBALL}" -C "${BUILD_DIR}" "$(basename "${OUT}")"
    printf '  Tarball: %s  (%s)\n' "${TARBALL}" "$(du -sh "${TARBALL}" | cut -f1)"
}

install_linux() {
    local OUT="${BUILD_DIR}/Aetherion-linux"
    local INSTALL_DIR="/opt/Aetherion"
    local BIN_LINK="/usr/local/bin/aetherion"
    local DESKTOP_DIR="${HOME}/.local/share/applications"

    echo "  Installing to ${INSTALL_DIR} (requires sudo) …"
    sudo rm -rf "${INSTALL_DIR}"
    sudo cp -R "${OUT}" "${INSTALL_DIR}"
    sudo chmod +x "${INSTALL_DIR}/aetherion" "${INSTALL_DIR}/bin/blackhole-sim"

    # Symlink into PATH so 'aetherion' works from any terminal
    sudo ln -sf "${INSTALL_DIR}/aetherion" "${BIN_LINK}"
    echo "  Symlink created: ${BIN_LINK} -> ${INSTALL_DIR}/aetherion"

    # Install .desktop entry for the user's application menu
    mkdir -p "${DESKTOP_DIR}"
    cp "${OUT}/aetherion.desktop" "${DESKTOP_DIR}/aetherion.desktop"
    chmod +x "${DESKTOP_DIR}/aetherion.desktop"
    command -v update-desktop-database &>/dev/null \
        && update-desktop-database "${DESKTOP_DIR}" 2>/dev/null || true
    echo "  Desktop entry installed to ${DESKTOP_DIR}/aetherion.desktop"

    printf '\n  Done. Launch with: aetherion\n'
    printf '  Or find "Aetherion" in your application menu.\n'
}

# ══════════════════════════════════════════════════════════════
# Windows — portable folder + zip archive
# ══════════════════════════════════════════════════════════════
bundle_windows() {
    local OUT="${BUILD_DIR}/Aetherion-windows"
    local RES_DIR="${OUT}/resources"

    echo "  Building ${OUT} …"
    rm -rf "${OUT}"
    mkdir -p "${OUT}" "${RES_DIR}"

    # Find the Windows executable (.exe from cross-compile, or bare name from MSYS2 build)
    local EXE_SRC=""
    for candidate in "${BUILD_DIR}/blackhole-sim.exe" "${BUILD_DIR}/blackhole-sim"; do
        [ -f "${candidate}" ] && { EXE_SRC="${candidate}"; break; }
    done
    if [[ -z "${EXE_SRC}" ]]; then
        printf 'ERROR: No Windows executable found in %s/.\n' "${BUILD_DIR}" >&2
        echo '       Cross-compile with a MinGW toolchain, or run on Windows with MSYS2.' >&2
        exit 1
    fi
    cp "${EXE_SRC}" "${OUT}/Aetherion.exe"
    echo "  Executable: ${EXE_SRC} -> ${OUT}/Aetherion.exe"

    # Collect any DLLs already placed in build/ by CMake or the toolchain
    local dll_count=0
    for dll in "${BUILD_DIR}"/*.dll; do
        [ -f "${dll}" ] && { cp "${dll}" "${OUT}/"; ((dll_count++)) || true; }
    done

    # When running natively under Git Bash / MSYS2, also collect DLLs via ldd.
    # Windows system DLLs (system32 / syswow64) are intentionally skipped.
    if command -v ldd &>/dev/null; then
        while IFS= read -r line; do
            local dllpath
            dllpath="$(echo "${line}" | awk '{print $3}')"
            [[ -z "${dllpath}" || "${dllpath}" == "not" || ! -f "${dllpath}" ]] && continue
            echo "${dllpath}" | grep -qiE 'system32|syswow64|\\Windows\\' && continue
            cp "${dllpath}" "${OUT}/" 2>/dev/null && ((dll_count++)) || true
        done < <(ldd "${EXE_SRC}" 2>/dev/null)
    fi
    echo "  DLLs collected: ${dll_count} file(s)."

    copy_resources "${RES_DIR}"
    echo "  Resources copied."

    # Basic README for end-users
    cat > "${OUT}/README.txt" <<README
Aetherion v${VERSION}
===================
Run Aetherion.exe to launch the simulator.

Requirements:
  - GPU with OpenGL 3.3+ support and up-to-date drivers
  - Visual C++ Redistributable (if built with MSVC)

If the app fails to start, run it from a terminal (cmd.exe) to see error output.
README

    # Create a distributable zip archive
    local ZIP="${BUILD_DIR}/Aetherion-windows-${VERSION}.zip"
    if command -v zip &>/dev/null; then
        (cd "${BUILD_DIR}" && zip -r "Aetherion-windows-${VERSION}.zip" "Aetherion-windows/" -x '*.zip' -q)
        printf '  Archive: %s  (%s)\n' "${ZIP}" "$(du -sh "${ZIP}" | cut -f1)"
    else
        echo "  NOTE: zip not found — skipping archive. Install with: brew install zip / apt install zip"
    fi

    printf '\n  Output: %s  (%s)\n' "${OUT}" "$(du -sh "${OUT}" | cut -f1)"
}

install_windows() {
    local OUT="${BUILD_DIR}/Aetherion-windows"

    # Default to the user-local programs folder (no administrator rights required).
    # $LOCALAPPDATA is available in Git Bash and MSYS2 on Windows.
    local INSTALL_DIR="${LOCALAPPDATA:-${HOME}/AppData/Local}/Programs/Aetherion"

    echo "  Installing to ${INSTALL_DIR} …"
    rm -rf "${INSTALL_DIR}"
    mkdir -p "${INSTALL_DIR}"
    cp -R "${OUT}/." "${INSTALL_DIR}/"

    printf '\n  Installed to: %s\n' "${INSTALL_DIR}"
    printf '  Run: "%s/Aetherion.exe"\n' "${INSTALL_DIR}"
    echo "  Tip: right-click Aetherion.exe -> 'Send to > Desktop (create shortcut)' for quick access."
}

# ══════════════════════════════════════════════════════════════
# Dispatch to the selected platform
# ══════════════════════════════════════════════════════════════
case "$PLATFORM" in
    mac)
        bundle_mac
        $DO_INSTALL && install_mac
        ;;
    linux)
        bundle_linux
        $DO_INSTALL && install_linux
        ;;
    windows)
        bundle_windows
        $DO_INSTALL && install_windows
        ;;
esac

echo ""
echo "=== ${APP_NAME} packaging complete (${PLATFORM}) ==="

