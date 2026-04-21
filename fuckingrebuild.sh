#!/usr/bin/env bash
set -euo pipefail

cd ~/Aetherion-Astrophysics-Suite-git
git pull
flatpak-builder --user --install --force-clean build-flatpak \
    flatpak/io.github.ArandomHitman.AetherionSuite.json
flatpak run io.github.ArandomHitman.AetherionSuite
