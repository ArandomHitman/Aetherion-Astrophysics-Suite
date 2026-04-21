# Aetherion Unified Window Management System

## Overview
```
┌──────────────────────────────────────┐
│  Aetherion (single instance)         │
├──────────────────────────────────────┤
│  ┌────────────────────────────────┐  │
│  │  Qt Main Application           │  │
│  │  ├─ Qt Launcher/Menu (native)  │  │
│  │  ├─ 2D Simulation (embedded)   │  │
│  │  ├─ 3D Visualization (embedded)│  │
│  │  └─ Tools/Analysis (embedded)  │  │
│  └────────────────────────────────┘  │
│                                      │
│  All windows share:                  │
│  • Memory space                      │
│  • Application state                 │
│  • Resource management               │
│  • Parent application context        │
└──────────────────────────────────────┘
```

**Benefit**: Single process, unified state management, guaranteed window hierarchy, coordinated lifecycle.

## Key Components

### 1. **QSFMLCanvas** (`sfml_canvas.h/cpp`)
A custom Qt widget that embeds SFML rendering directly in Qt.

```cpp
class QSFMLCanvas : public QWidget, public sf::RenderWindow
```

- Allows SFML windows to render within Qt widgets
- Inherits from both `QWidget` and `sf::RenderWindow` for seamless integration
- Handles Qt event system (mouse, keyboard, resize) and translates to SFML
- Continuous rendering via `update()` scheduling
- Virtual methods for override: `onInit()`, `onUpdate()`, input handlers

**Key features**:
- Mouse events: move, press, release, wheel scroll
- Keyboard event mapping (Qt keys → SFML keys)
- Automatic SFML context management via Qt's window handle
- No sub-processes or threading needed

### 2. **Simulation2DWidget** (`simulation_2d_widget.h/cpp`)
Embedded 2D black hole simulation tab.

```cpp
class Simulation2DWidget : public QSFMLCanvas
```

- Wraps 2D simulation logic as a Qt widget
- Stub implementation ready for full 2D core integration
- Receives input from parent window's event system
- Renders directly to embedded canvas

**Integration points**:
- `BlackHole2D.cpp` core logic (units, physics, rendering)
- Simulation state management
- HUD and UI rendering

### 3. **Simulation3DWidget** (`simulation_3d_widget.h/cpp`)
Embedded 3D visualization tab.

```cpp
class Simulation3DWidget : public QSFMLCanvas
```

- Wraps 3D simulation logic as a Qt widget
- OpenGL context setup via SFML
- Camera controller integration
- Bloom pipeline and shader management

**Integration points**:
- `BlackHole3D.cpp` core logic (OpenGL rendering, shaders)
- ResourceManager, CameraController, SimulationState
- Bloom postprocessing pipeline
- Shader loading and texture utilities

### 4. **Updated MainWindow** (`mainwindow.h/cpp`)
Central hub for unified application management.

**Changes**:
- Removed `QProcess` and process spawning logic
- Replaced separate launcher execution with tabbed interface
- Stores embedded simulation widgets as member objects:
  ```cpp
  std::unique_ptr<Simulation2DWidget> sim2DWidget;
  std::unique_ptr<Simulation3DWidget> sim3DWidget;
  ```
- `createSimulationPage()` now returns a `QTabWidget` with embedded simulations
- Click handlers switch to simulation workspace instead of launching processes

**Slot functions**:
```cpp
void on2DSimulationClicked();      // Switch to 2D tab
void on3DSimulationClicked();      // Switch to 3D tab
void onWorkspaceSelectionChanged(); // Navigate pages
void onToolsSelectionChanged();     // Access tools
void onRecentFileSelected();        // Load progress
```

## Build System Update (CMakeLists.txt)

### Single Executable Build
```cmake
add_executable(blackhole-sim
    # Qt Launcher & Main Window
    src/QT-LAUNCHER/main.cpp
    src/QT-LAUNCHER/mainwindow.cpp
    src/QT-LAUNCHER/sfml_canvas.cpp
    
    # Simulation Widgets
    src/QT-LAUNCHER/simulation_2d_widget.cpp
    src/QT-LAUNCHER/simulation_3d_widget.cpp
    
    # Core Simulations (embedded)
    src/2D/BlackHole2D.cpp
    src/3D/BlackHole3D.cpp
    src/3D/shader_utils.cpp
    src/3D/texture_utils.cpp
)
```

- All code compiled into **one executable**: `blackhole-sim`
- Linked libraries:
  - Qt6 (Core, Gui, Widgets)
  - SFML (Graphics, Window, System)
  - OpenGL
  - GLM (for math)
- Post-build: Shader files copied to binary output directory
- No separate `blackhole-2D` or `blackhole-3D` executables

### Legacy Support (Optional)
Commented-out sections in CMakeLists.txt allow re-enabling separate executables if needed for standalone testing:

```cmake
# Uncomment to build standalone 2D/3D for testing
# add_executable(blackhole-2D src/2D/BlackHole2D.cpp)
# add_executable(blackhole-3D src/3D/BlackHole3D.cpp ...)
```

## Event Flow & Window Management

### Initialization
1. **main.cpp**: Creates `QApplication` and `MainWindow`
2. **MainWindow**: Calls `setupUI()` → creates tabbed interface
3. **createSimulationPage()**: Instantiates `Simulation2DWidget` and `Simulation3DWidget`
4. User switches to "Simulation" workspace

### User Interaction
```
User clicks "2D Research Mode"
        ↓
MainWindow::on2DSimulationClicked()
        ↓
workspaceList→setCurrentRow(1)
contentStack→setCurrentIndex(1)
        ↓
Simulation workspace becomes visible
QTabWidget shows 2D and 3D tabs
        ↓
User sees embedded 2D canvas
```

### Event Handling
```
Qt Event (keyboard/mouse/resize)
        ↓
QSFMLCanvas receives event
        ↓
Translates to SFML equivalent
        ↓
Calls virtual method (onKeyPressed, onMouseMoved, etc.)
        ↓
Simulation2DWidget/3DWidget implements logic
        ↓
Renders to SFML RenderWindow context
        ↓
Display updates in embedded widget
```

### Shutdown
1. User closes main window
2. MainWindow destructor called
3. All embedded widgets (`sim2DWidget`, `sim3DWidget`) destroyed
4. SFML contexts cleaned up automatically
5. Single clean process exit

## Integration Workflow

### For 2D Simulation
1. Replace `BlackHole2D.cpp`'s `main()` with:
   - Simulation state initialization in `Simulation2DWidget::onInit()`
   - Frame rendering loop in `onUpdate()`
   - Input handling methods (`onKeyPressed`, `onMouseMoved`, etc.)
2. Keep existing physics, rendering, and UI logic
3. Adapt HUD/text rendering for SFML context

### For 3D Simulation
1. OpenGL context is provided by SFML via `QSFMLCanvas`
2. Call `setActive()` before GL operations
3. Shader loading, texture management in `onInit()`
4. Rendering in `onUpdate()`
5. Camera control via input handlers

### For Additional Simulations/Tools
Follow the same pattern:
```cpp
class NewSimulationWidget : public QSFMLCanvas {
    void onInit() override { /* Initialize resources */ }
    void onUpdate() override { /* Render frame */ }
    void onKeyPressed(sf::Keyboard::Key code) override { /* Handle input */ }
};
```

Then add to `createSimulationPage()`:
```cpp
auto newWidget = std::make_unique<NewSimulationWidget>(page);
simTabs->addTab(newWidget.get(), "New Simulation");
```

## Benefits of Unified Architecture

✅ **Single Instance**: One process, shared resources  
✅ **Unified State**: All windows see same application state  
✅ **Parent-Child Relationship**: Simulations are children of main window  
✅ **Task Umbrella**: All tasks loop under one Qt event loop  
✅ **Graceful Shutdown**: Clean lifecycle management  
✅ **Memory Efficient**: No process overhead for simulations  
✅ **Data Synchronization**: Easy cross-window communication  
✅ **Debugging**: Single debugger session for entire app  
✅ **Resource Sharing**: Textures, fonts, shaders shared  
✅ **Native Menu Integration**: One menu bar for all simulations  

## Testing & Validation

### Build
```bash
mkdir build && cd build
cmake ..
make
```

### Run
```bash
./blackhole-sim
```

### Tab Switch
- Click "Simulation" in sidebar → 2D and 3D tabs appear
- Click tab headers to switch between simulations
- Both simulations share the same main window context

### Verify Single Instance
- Check process list: only one `blackhole-sim` process
- Use Activity Monitor → Confirm single executable running
- Task manager shows one "Aetherion" entry

## Migration Checklist

- [x] Create `QSFMLCanvas` base class for SFML-in-Qt integration
- [x] Create `Simulation2DWidget` widget wrapper
- [x] Create `Simulation3DWidget` widget wrapper
- [x] Update `MainWindow` to embed simulations in tabs
- [x] Remove `QProcess` and process spawning
- [x] Update `CMakeLists.txt` for single executable
- [ ] **Next**: Integrate full 2D simulation core into `Simulation2DWidget::onUpdate()`
- [ ] **Next**: Integrate full 3D simulation core into `Simulation3DWidget::onUpdate()`
- [ ] **Next**: Adapt main() loops from original simulations
- [ ] **Next**: Test input handling and event propagation
- [ ] **Next**: Optimize rendering pipeline for tabbed multi-simulation context
- [ ] **Next**: Add window state persistence (zoom, camera position, etc.)
- [ ] **Next**: Implement tab-specific settings/presets

## Notes

- **Memory**: Shared between tabs (more efficient than separate processes)
- **Rendering**: Both tabs render via SFML; OpenGL context managed by SFML
- **Input Focus**: Qt's focus system ensures only active tab receives input
- **Performance**: Single Qt event loop handles all windows
- **Deployment**: One executable distributes easier than three

## References

- Qt Documentation: `QWidget`, `QTabWidget`, `QPainter`
- SFML Documentation: `RenderWindow`, `Event`, `Window`
- CMake: Modern Qt6 configuration and linking
