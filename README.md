# Aetherion — Astrophysics Simulation Suite

[![Version](https://img.shields.io/badge/version-0.1.1--beta1-blue)]
[![Status](https://img.shields.io/badge/status-beta-orange)]
[![License](https://img.shields.io/badge/license-MIT-green)]

Aetherion is an open source astrophysics simulation and analysis suite designed for precision experimentation, visualization, and reproducible research around BLACK HOLES.

DISCLAIMER!
                                                       
As of 17-04-2026, Aetherion is in active BETA, with core features implemented and undergoing testing. 
DO NOT EXPECT PERFECT STABILITY OR COMPLETE FEATURE SET YET!

**Physics Model Limitations:**
- **Temperature/Sound Speed Model:** The gas temperature calculations assume ideal gas behavior, which is optimistic for hot accreting plasma near black holes where radiation pressure and magnetic effects dominate. This is fine for Bondi radius order-of-magnitude estimates but should not be relied upon for precision plasma dynamics.
---

## Table of Contents

- [About](#about)
- [Core Features](#core-features)
- [Architecture](#architecture)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Data Analysis](#data-analysis)
- [Configuration](#configuration)
- [Known Issues](#known-issues)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)

---

## About

The purpose of Aetherion is to bridge the gap between raw astrophysics simulation and structured analysis.

The program focuses on:
- Outcome-deterministic simulation pipelines
- Modular design
- Clean data export for external (or internal) analysis
- Built-in visualization tools 

Unlike traditional simulation tools, Aetherion is built around **reproducibility, extensibility, and clean results**.

---

## Core Features

- **Simulation Engine**
  - Orbital mechanics
  - Photon trajectory modeling
  - Relativistic effects (e.g., deflection, precession)

- **Data Analysis Suite**
  - CSV, FITS, or BINARY import/export
  - Interactive graphing (orbit paths, energy conservation, etc.)
  - Multi-dataset comparison

- **Visualization**
  - Real-time graphing visualization
  - 2D and 3D rendering of orbits and photon paths
  - Multi-tab analysis interface
  - Clean UI for rapid inspection

- **Object Library**
  - Reusable custom bodies (i.e, pulsar, NOT YET IMPLEMENTED)
  - Configurable parameters (mass, gas temp, etc, still WIP)

- **Architecture**
  - Modularity for easy extension
  - Plugin system for custom modularity (NOT YET IMPLEMENTED)
  - Configurable for keybinds, scenarios, etc

---

## Installation

### macOS (Homebrew)

```bash
brew install cmake sfml glm
git clone https://github.com/0xLiam0920/Aetherion-Astrophysics-Suite.git
cd Aetherion-Astrophysics-Suite
mkdir build && cd build
cmake ..
make -j$(nproc)
./blackhole-sim
```

### Linux (Ubuntu / Debian)

```bash
sudo apt install cmake g++ \
    libsfml-dev \
    libglm-dev \
    libglew-dev \
    qt6-base-dev qt6-charts-dev \
    qt6-qpa-plugins           # XCB platform plugin — required at runtime
git clone https://github.com/0xLiam0920/Aetherion-Astrophysics-Suite.git
cd Aetherion-Astrophysics-Suite
mkdir build && cd build
cmake ..
make -j$(nproc)
./blackhole-sim
```

> **Note:** SFML 3.x is required. Ubuntu 24.04+ provides it via `libsfml-dev`. On older, moreso legacy Ubuntu/Debian releases, build SFML 3.0 from source or use the Flatpak distr.

### Linux (Fedora / RHEL)

```bash
sudo dnf install cmake gcc-c++ \
    SFML-devel \
    glm-devel \
    glew-devel \
    qt6-qtbase-devel qt6-qtcharts-devel
```

### Linux (Flatpak — recommended for end users)

```bash
flatpak install flathub org.kde.Platform//6.8 org.kde.Sdk//6.8
flatpak-builder --user --install --force-clean build-flatpak \
    flatpak/io.github.0xLiam0920.AetherionSuite.json
flatpak run io.github.0xLiam0920.AetherionSuite
```

### Physics regression tests

```bash
cd build
ctest --output-on-failure -R physics-regression
```

---

## Known Issues

- SFML 2.x is not supported — 3.x API is required.
- On Wayland, the app forces X11/XWayland via `QT_QPA_PLATFORM=xcb` (required by SFML). Set `QT_QPA_PLATFORM=xcb` manually if needed.
- Pulsar and merger objects are still a VERY early WIP, and are not recommended for any serious data compilation (see TODO.md).
- Windows support is untested and officially unsupported, but structurally supported if you wish to make a Pull request to compile to exe for windows.

---

## License

See [LICENSE](LICENSE) for full terms.

