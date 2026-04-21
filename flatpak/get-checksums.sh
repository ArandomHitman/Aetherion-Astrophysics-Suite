#!/usr/bin/env bash
# flatpak/get-checksums.sh
# ============================================================
# Downloads the three source archives used in the Flatpak manifest
# and prints their sha256 checksums.  Also patches them directly
# into io.github.ArandomHitman.AetherionSuite.json.
#
# Run this whenever you update a dependency URL or version:
#
#   cd flatpak/
#   bash get-checksums.sh
# ============================================================

set -euo pipefail

TMPDIR_WORK="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_WORK"' EXIT

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MANIFEST="$SCRIPT_DIR/io.github.ArandomHitman.AetherionSuite.json"

declare -A URLS=(
    ["glew"]="https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.tgz"
    ["glm"]="https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz"
    ["sfml"]="https://github.com/SFML/SFML/archive/refs/tags/3.0.0.tar.gz"
)

declare -A SUMS

echo "Downloading and computing sha256 sums..."
echo ""

for NAME in glew glm sfml; do
    URL="${URLS[$NAME]}"
    DEST="$TMPDIR_WORK/$NAME.archive"
    echo "  Downloading $NAME ..."
    curl -fsSL "$URL" -o "$DEST"
    if command -v sha256sum &>/dev/null; then
        SUM="$(sha256sum "$DEST" | awk '{print $1}')"
    else
        SUM="$(shasum -a 256 "$DEST" | awk '{print $1}')"
    fi
    SUMS[$NAME]="$SUM"
    echo "  sha256 ($NAME): $SUM"
    echo ""
done

# Patch the manifest in-place using Python for reliable JSON editing
python3 - "$MANIFEST" "${SUMS[glew]}" "${SUMS[glm]}" "${SUMS[sfml]}" <<'PYEOF'
import sys, json

manifest_path, glew_sum, glm_sum, sfml_sum = sys.argv[1:]
sums = {"glew": glew_sum, "glm": glm_sum, "sfml": sfml_sum}

with open(manifest_path) as f:
    data = json.load(f)

for module in data.get("modules", []):
    name = module.get("name", "")
    if name in sums:
        for src in module.get("sources", []):
            if src.get("type") == "archive":
                src["sha256"] = sums[name]

with open(manifest_path, "w") as f:
    json.dump(data, f, indent=4)
    f.write("\n")

print(f"Manifest patched: {manifest_path}")
PYEOF

echo "-------------------------------------------------------------"
echo "Checksums written to $MANIFEST"
echo "-------------------------------------------------------------"
