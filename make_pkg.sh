#!/bin/bash
# =============================================================
# make_pkg.sh
#
# Usage:
#   ./make_pkg.sh                      # builds .app then packages it (default)
#   ./make_pkg.sh --no-bundle          # skip bundle_app.sh 
#   ./make_pkg.sh --version 1.2.3      # override version string
#
# Prerequisites:
#   • Xcode Command Line Tools (pkgbuild, productbuild)
#       install via xcode-select --install, most macOS devs should have this though...
#
# Output: build/Aetherion-Installer-v<VERSION>.pkg
# =============================================================
set -euo pipefail

APP_NAME="Aetherion"
BUNDLE_ID="com.liamnayyar.aetherion"
VERSION="1.0.0"
BUILD_DIR="build"

# ── Code-signing configuration ────────────────────────────────
# Set via environment variables or the flags below.
# Find the exact names in Keychain Access → "My Certificates".
#
#   APPLE_SIGNING_IDENTITY   → "Developer ID Application: Your Name (TEAMID)"
#   APPLE_INSTALLER_IDENTITY → "Developer ID Installer: Your Name (TEAMID)"
#
# Leave blank (or unset) to use ad-hoc signing for the .app (no paid account needed).
# Note: .pkg files cannot be ad-hoc signed; the installer will be unsigned unless
# APPLE_INSTALLER_IDENTITY is set to a valid Developer ID Installer certificate.
APP_SIGN_IDENTITY="${APPLE_SIGNING_IDENTITY:--}"
PKG_SIGN_IDENTITY="${APPLE_INSTALLER_IDENTITY:-}"

BUNDLE_FIRST=true
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUNDLE="${SCRIPT_DIR}/${BUILD_DIR}/${APP_NAME}.app"

# ── Argument parsing ──────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-bundle)      BUNDLE_FIRST=false; shift ;;
        --version)        VERSION="${2:?'--version requires a value'}"; shift 2 ;;
        --version=*)      VERSION="${1#--version=}"; shift ;;
        --app-sign-identity)    APP_SIGN_IDENTITY="${2:?'--app-sign-identity requires a value'}"; shift 2 ;;
        --app-sign-identity=*)  APP_SIGN_IDENTITY="${1#--app-sign-identity=}"; shift ;;
        --pkg-sign-identity)    PKG_SIGN_IDENTITY="${2:?'--pkg-sign-identity requires a value'}"; shift 2 ;;
        --pkg-sign-identity=*)  PKG_SIGN_IDENTITY="${1#--pkg-sign-identity=}"; shift ;;
        -h|--help)        sed -n '2,12p' "$0" | sed 's/^# \?//'; exit 0 ;;
        *)
            printf 'ERROR: Unknown argument: %s\nRun with --help for usage.\n' "$1" >&2
            exit 1 ;;
    esac
done

# ── macOS only ────────────────────────────────────────────────────────────────
if [[ "$(uname -s)" != "Darwin" ]]; then
    printf 'ERROR: make_pkg.sh only supports macOS.\n' >&2
    printf '       pkgbuild and productbuild are macOS-only tools.\n' >&2
    exit 1
fi

# ── Check for required tools ──────────────────────────────────────────────────
for tool in pkgbuild productbuild hdiutil; do
    if ! command -v "$tool" &>/dev/null; then
        printf 'ERROR: %s not found.\n' "$tool" >&2
        if [[ "$tool" == "hdiutil" ]]; then
            printf '       hdiutil is part of macOS; ensure you are running macOS.\n' >&2
        else
            printf '       Install Xcode Command Line Tools:  xcode-select --install\n' >&2
        fi
        exit 1
    fi
done

# ── Optionally build the .app bundle first ────────────────────────────────────
if $BUNDLE_FIRST; then
    echo "=== Running bundle_app.sh --platform mac ==="
    bash "${SCRIPT_DIR}/bundle_app.sh" --platform mac --sign-identity "${APP_SIGN_IDENTITY}"
fi

# ── Verify .app exists ────────────────────────────────────────────────────────
if [[ ! -d "${BUNDLE}" ]]; then
    printf 'ERROR: %s not found.\n' "${BUNDLE}" >&2
    printf '       Run ./bundle_app.sh first, or use --bundle-first.\n' >&2
    exit 1
fi

echo "=== Building ${APP_NAME} v${VERSION} installer package ==="

# ── Temp working directory (always cleaned up) ────────────────────────────────
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT

COMPONENT_PKG="${WORK_DIR}/Aetherion-component.pkg"
RESOURCES_DIR="${WORK_DIR}/resources"
mkdir -p "${RESOURCES_DIR}"

# ── Step 1: component package (.app → /Applications) ─────────────────────────
echo "  [1/3] Building component package …"
pkgbuild_args=(
    --component   "${BUNDLE}"
    --install-location /Applications
    --identifier  "${BUNDLE_ID}"
    --version     "${VERSION}"
)
[[ -n "${PKG_SIGN_IDENTITY}" ]] && pkgbuild_args+=(--sign "${PKG_SIGN_IDENTITY}")
pkgbuild_args+=("${COMPONENT_PKG}")
pkgbuild "${pkgbuild_args[@]}"

# ── Step 2: installer resources ───────────────────────────────────────────────
echo "  [2/3] Generating installer resources …"

# Welcome screen (shown before the license page)
cat > "${RESOURCES_DIR}/welcome.html" <<'WELCOME'
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8"/>
<style>
  body { font-family: -apple-system, Helvetica Neue, sans-serif;
         font-size: 13px; color: #111; margin: 20px 24px; line-height: 1.55; }
  h1   { font-size: 17px; font-weight: 600; margin-bottom: 6px; }
  p    { margin: 0 0 10px; }
</style>
</head>
<body>
  <h1>Welcome to Aetherion Astrophysics Suite</h1>
  <p>This installer will place <strong>Aetherion.app</strong> in your
     <strong>/Applications</strong> folder.</p>
  <p>Aetherion is an open-source black hole simulation and visualisation tool
     with a 2D general-relativity research mode and a real-time 3D renderer
     featuring physically-based accretion disk rendering.</p>
  <p>Click <strong>Continue</strong> to review the license and begin installation.</p>
</body>
</html>
WELCOME

# Conclusion screen (shown after successful install)
if [[ "${APP_SIGN_IDENTITY}" == "-" ]]; then
    _LAUNCH_NOTE='<p><em>First-launch note:</em> this build is ad-hoc signed and not notarized. macOS may show a security prompt — open <strong>System Settings \u2192 Privacy &amp; Security</strong> and click <strong>Open Anyway</strong> to proceed.</p>'
else
    _LAUNCH_NOTE='<p><em>First-launch note:</em> if this build has not been notarized with Apple, macOS Gatekeeper may still show a security prompt. Open <strong>System Settings \u2192 Privacy &amp; Security</strong> and click <strong>Open Anyway</strong> to proceed.</p>'
fi

cat > "${RESOURCES_DIR}/conclusion.html" <<CONCLUSION
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8"/>
<style>
  body { font-family: -apple-system, Helvetica Neue, sans-serif;
         font-size: 13px; color: #111; margin: 20px 24px; line-height: 1.55; }
  h1   { font-size: 17px; font-weight: 600; margin-bottom: 6px; }
  p    { margin: 0 0 10px; }
  code { background: #f0f0f0; padding: 1px 4px; border-radius: 3px; font-size: 12px; }
</style>
</head>
<body>
  <h1>Installation complete</h1>
  <p><strong>Aetherion</strong> has been installed to <strong>/Applications</strong>.</p>
  <p>Open it from <strong>Launchpad</strong>, <strong>Spotlight</strong>
     (&#8984; Space), or the Applications folder.</p>
  ${_LAUNCH_NOTE}
</body>
</html>
CONCLUSION

# License — use project LICENSE file if present, otherwise fall back to inline MIT
if [[ -f "${SCRIPT_DIR}/LICENSE" ]]; then
    cp "${SCRIPT_DIR}/LICENSE" "${RESOURCES_DIR}/LICENSE.txt"
else
    cat > "${RESOURCES_DIR}/LICENSE.txt" <<'LICENSE'
MIT License

Copyright (c) 2026 ArandomHitman

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
LICENSE
fi

# Distribution XML — defines UI pages, OS requirement, and package reference
cat > "${WORK_DIR}/distribution.xml" <<DIST_XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">

    <title>Aetherion Astrophysics Suite</title>
    <organization>com.liamnayyar</organization>

    <!-- Install to /Applications for all users; no user-home installs -->
    <domains enable_localSystem="true" enable_userHome="false"/>

    <!-- Require macOS 13.0+ (matches LSMinimumSystemVersion in Info.plist) -->
    <allowed-os-versions>
        <os-version min="13.0"/>
    </allowed-os-versions>

    <!-- Installer UI pages (order matters) -->
    <welcome    file="welcome.html"    mime-type="text/html"/>
    <license    file="LICENSE.txt"     mime-type="text/plain"/>
    <conclusion file="conclusion.html" mime-type="text/html"/>

    <!-- Single default choice (no component picker needed) -->
    <choice id="default" visible="false" start_selected="true" start_enabled="true">
        <pkg-ref id="${BUNDLE_ID}"/>
    </choice>

    <choices-outline>
        <line choice="default"/>
    </choices-outline>

    <pkg-ref id="${BUNDLE_ID}" version="${VERSION}" onConclusion="none">Aetherion-component.pkg</pkg-ref>

</installer-gui-script>
DIST_XML

# ── Step 3: final distribution installer ─────────────────────────────────────
OUTPUT_PKG="${SCRIPT_DIR}/${BUILD_DIR}/${APP_NAME}-Installer-v${VERSION}.pkg"
echo "  [3/3] Building distribution installer …"
productbuild_args=(
    --distribution "${WORK_DIR}/distribution.xml"
    --package-path "${WORK_DIR}"
    --resources    "${RESOURCES_DIR}"
)
[[ -n "${PKG_SIGN_IDENTITY}" ]] && productbuild_args+=(--sign "${PKG_SIGN_IDENTITY}")
productbuild_args+=("${OUTPUT_PKG}")
productbuild "${productbuild_args[@]}"

SIZE="$(du -sh "${OUTPUT_PKG}" | cut -f1)"
printf '\n=== Done ===\n'
printf '  Output:  %s  (%s)\n' "${OUTPUT_PKG}" "${SIZE}"
if [[ -n "${PKG_SIGN_IDENTITY}" ]]; then
    printf '  Signed:  %s\n' "${PKG_SIGN_IDENTITY}"
else
    printf '  Signed:  unsigned (no --pkg-sign-identity provided)\n'
fi
printf '\n  To install via GUI:     double-click the .pkg\n'
printf '  To install via terminal: sudo installer -pkg "%s" -target /\n' "${OUTPUT_PKG}"
if [[ -z "${APP_SIGN_IDENTITY}" || -z "${PKG_SIGN_IDENTITY}" ]]; then
    printf '\n  NOTE: App/installer is not signed with a Developer ID. Users may need to\n'
    printf '        allow it in System Settings \xe2\x86\x92 Privacy & Security on first launch.\n'
    printf '        To sign: set APPLE_SIGNING_IDENTITY and APPLE_INSTALLER_IDENTITY env vars,\n'
    printf '        or pass --app-sign-identity and --pkg-sign-identity flags.\n'
else
    printf '\n  NOTE: Signed. To distribute publicly without Gatekeeper warnings, notarize:\n'
    printf '        xcrun notarytool submit "%s" --wait --keychain-profile <PROFILE>\n' "${OUTPUT_PKG}"
    printf '        xcrun stapler staple "%s"\n' "${OUTPUT_PKG}"
fi
