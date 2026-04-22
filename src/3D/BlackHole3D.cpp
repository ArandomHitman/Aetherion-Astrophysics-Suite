// ============================================================
// BlackHole3D.cpp — Main application (uses RAII wrappers, config,
//                   CameraController, PhysicsSnapshot, BloomPipeline)
// ============================================================

#include "platform.hpp"
#include <SFML/Window.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include "gl_types.hpp"
#include "config.hpp"
#include "resource_manager.hpp"
#include "shader_sources.hpp"
#include "shader_utils.hpp"
#include "texture_utils.hpp"
#include "bloom_pipeline.hpp"
#include "camera_controller.hpp"
#include "input.hpp"
#include "simulation_state.hpp"
#include "presets.hpp"
#include "orbital_body.hpp"
#include "gl_font.hpp"
#include <cstring>

// oh boy i sure do love 1200 lines of code in one document!

// ────────────────────────────────────────────────────────────
// Helper: set all scene-shader uniforms
// ────────────────────────────────────────────────────────────
// Pre-cached uniform locations for the scene shader (looked up once at init).
struct SceneUniforms {
    GLint resolution, cameraPos, cameraDir, cameraUp, fov;
    GLint blackHoleRadius, blackHolePos, backgroundTex, diskTex;
    GLint diskInnerRadius, diskOuterRadius, diskHalfThickness;
    GLint spinParameter;
    GLint jetRadius, jetLength, jetColor;
    GLint showJets, showBLR, showDoppler, showBlueshift;
    GLint blrInnerRadius, blrOuterRadius, blrThickness;
    GLint uTime;
    GLint showOrbBody;
    GLint orbBodyPos[10];
    GLint orbBodyRadius[10];
    GLint orbBodyColor[10];
    GLint numOrbBodies;
    GLint showHostGalaxy, hostGalaxyRadius, hostGalaxyColor;
    GLint showLAB, labPos, labRadius, labColor;
    GLint showCGM, cgmRadius, cgmColor;
    GLint maxStepsOverride;
    GLint diskPeakTemp, diskDisplayTempInner, diskDisplayTempOuter;
    GLint diskSatBoostInner, diskSatBoostOuter;

    static SceneUniforms lookup(GLProgram& prog) {
        SceneUniforms u;
        u.resolution      = prog.uniform("resolution");
        u.cameraPos        = prog.uniform("cameraPos");
        u.cameraDir        = prog.uniform("cameraDir");
        u.cameraUp         = prog.uniform("cameraUp");
        u.fov              = prog.uniform("fov");
        u.blackHoleRadius  = prog.uniform("blackHoleRadius");
        u.blackHolePos     = prog.uniform("blackHolePos");
        u.backgroundTex    = prog.uniform("backgroundTex");
        u.diskTex          = prog.uniform("diskTex");
        u.diskInnerRadius  = prog.uniform("diskInnerRadius");
        u.diskOuterRadius  = prog.uniform("diskOuterRadius");
        u.diskHalfThickness= prog.uniform("diskHalfThickness");
        u.spinParameter    = prog.uniform("spinParameter");
        u.jetRadius        = prog.uniform("jetRadius");
        u.jetLength        = prog.uniform("jetLength");
        u.jetColor         = prog.uniform("jetColor");
        u.showJets         = prog.uniform("showJets");
        u.showBLR          = prog.uniform("showBLR");
        u.showDoppler      = prog.uniform("showDoppler");
        u.showBlueshift    = prog.uniform("showBlueshift");
        u.blrInnerRadius   = prog.uniform("blrInnerRadius");
        u.blrOuterRadius   = prog.uniform("blrOuterRadius");
        u.blrThickness     = prog.uniform("blrThickness");
        u.uTime            = prog.uniform("uTime");
        u.showOrbBody      = prog.uniform("showOrbBody");
        for (int i = 0; i < 10; ++i) {
            char buf[64];
            snprintf(buf, sizeof(buf), "orbBodyPos[%d]", i);
            u.orbBodyPos[i] = prog.uniform(buf);
            snprintf(buf, sizeof(buf), "orbBodyRadius[%d]", i);
            u.orbBodyRadius[i] = prog.uniform(buf);
            snprintf(buf, sizeof(buf), "orbBodyColor[%d]", i);
            u.orbBodyColor[i] = prog.uniform(buf);
        }
        u.numOrbBodies     = prog.uniform("numOrbBodies");
        u.showHostGalaxy   = prog.uniform("showHostGalaxy");
        u.hostGalaxyRadius = prog.uniform("hostGalaxyRadius");
        u.hostGalaxyColor  = prog.uniform("hostGalaxyColor");
        u.showLAB          = prog.uniform("showLAB");
        u.labPos           = prog.uniform("labPos");
        u.labRadius        = prog.uniform("labRadius");
        u.labColor         = prog.uniform("labColor");
        u.showCGM          = prog.uniform("showCGM");
        u.cgmRadius        = prog.uniform("cgmRadius");
        u.cgmColor         = prog.uniform("cgmColor");
        u.maxStepsOverride = prog.uniform("maxStepsOverride");
        u.diskPeakTemp          = prog.uniform("diskPeakTemp");
        u.diskDisplayTempInner  = prog.uniform("diskDisplayTempInner");
        u.diskDisplayTempOuter  = prog.uniform("diskDisplayTempOuter");
        u.diskSatBoostInner     = prog.uniform("diskSatBoostInner");
        u.diskSatBoostOuter     = prog.uniform("diskSatBoostOuter");
        return u;
    }
};

// Takes a pure-data PhysicsSnapshot, all physics objects will be shot on site if they cross this boundary >:)
static void setSceneUniforms(GLProgram& prog,
                             const SceneUniforms& u,
                             const PhysicsSnapshot& snap,
                             GLuint bgTexId,
                             GLuint diskTexId)
{
    prog.use();

    // Camera & viewport
    glUniform2f(u.resolution, float(snap.windowW), float(snap.windowH));
    glUniform3f(u.cameraPos, snap.cameraPos.x, snap.cameraPos.y, snap.cameraPos.z);
    glUniform3f(u.cameraDir, snap.cameraDir.x, snap.cameraDir.y, snap.cameraDir.z);
    glUniform3f(u.cameraUp,  snap.cameraUp.x,  snap.cameraUp.y,  snap.cameraUp.z);
    glUniform1f(u.fov, snap.fov);

    // Black hole
    glUniform1f(u.blackHoleRadius, snap.bhRadius);
    glUniform3f(u.blackHolePos, snap.bhPosition.x, snap.bhPosition.y, snap.bhPosition.z);

    // Textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, bgTexId);
    glUniform1i(u.backgroundTex, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, diskTexId);
    glUniform1i(u.diskTex, 1);

    // Disk
    glUniform1f(u.diskInnerRadius,  snap.diskInnerRadius);
    glUniform1f(u.diskOuterRadius,  snap.diskOuterRadius);
    glUniform1f(u.diskHalfThickness, snap.diskHalfThickness);

    // Disk color temperatures
    glUniform1f(u.diskPeakTemp,         snap.diskPeakTemp);
    glUniform1f(u.diskDisplayTempInner, snap.diskDisplayTempInner);
    glUniform1f(u.diskDisplayTempOuter, snap.diskDisplayTempOuter);
    glUniform1f(u.diskSatBoostInner,    snap.diskSatBoostInner);
    glUniform1f(u.diskSatBoostOuter,    snap.diskSatBoostOuter);

    // Spin
    glUniform1f(u.spinParameter, snap.bhSpin);

    // Jets
    glUniform1f(u.jetRadius, snap.jetRadius);
    glUniform1f(u.jetLength, snap.jetLength);
    glUniform3f(u.jetColor, snap.jetColor.x, snap.jetColor.y, snap.jetColor.z);

    // Toggles
    glUniform1i(u.showJets,    snap.jetsEnabled    ? 1 : 0);
    glUniform1i(u.showBLR,     snap.blrEnabled     ? 1 : 0);
    glUniform1i(u.showDoppler, snap.dopplerEnabled  ? 1 : 0);
    glUniform1i(u.showBlueshift, snap.blueshiftEnabled ? 1 : 0);

    // BLR
    glUniform1f(u.blrInnerRadius, snap.blrInnerRadius);
    glUniform1f(u.blrOuterRadius, snap.blrOuterRadius);
    glUniform1f(u.blrThickness,   snap.blrThickness);

    // Time
    glUniform1f(u.uTime, snap.animTime);

    // Orbiting bodies
    glUniform1i(u.showOrbBody, snap.orbBodyEnabled ? 1 : 0);
    glUniform1i(u.numOrbBodies, snap.orbBodyPositions.size());
    for (size_t i = 0; i < snap.orbBodyPositions.size() && i < 10; ++i) {
        glUniform3f(u.orbBodyPos[i], snap.orbBodyPositions[i].x, snap.orbBodyPositions[i].y, snap.orbBodyPositions[i].z);
        glUniform1f(u.orbBodyRadius[i], snap.orbBodyRadii[i]);
        glUniform3f(u.orbBodyColor[i], snap.orbBodyColors[i].x, snap.orbBodyColors[i].y, snap.orbBodyColors[i].z);
    }

    // Large-scale structures
    glUniform1i(u.showHostGalaxy, snap.hostGalaxyEnabled ? 1 : 0);
    glUniform1f(u.hostGalaxyRadius, 10.0f);  // 10 kpc radius
    glUniform3f(u.hostGalaxyColor, 0.8f, 0.6f, 0.4f);  // Warm galaxy color
    glUniform1i(u.showLAB, snap.labEnabled ? 1 : 0);
    glUniform3f(u.labPos, 50.0f, 0.0f, 0.0f);  // Offset position
    glUniform1f(u.labRadius, 100.0f);  // 100 kpc radius
    glUniform3f(u.labColor, 0.4f, 0.7f, 1.0f);  // Blue LAB
    glUniform1i(u.showCGM, snap.cgmEnabled ? 1 : 0);
    glUniform1f(u.cgmRadius, 200.0f);  // 200 kpc halo
    glUniform3f(u.cgmColor, 0.3f, 0.5f, 0.8f);  // Diffuse blue halo

    // Step count override
    glUniform1i(u.maxStepsOverride, snap.maxSteps);
}

/*------------------- Keybinds & HUD drawing -------------------*/
struct ActionKeybinds {
    sf::Keyboard::Key toggleFreelook   = sf::Keyboard::Key::F;
    sf::Keyboard::Key toggleJets       = sf::Keyboard::Key::J;
    sf::Keyboard::Key toggleBLR        = sf::Keyboard::Key::G;
    sf::Keyboard::Key toggleOrbBody    = sf::Keyboard::Key::O;
    sf::Keyboard::Key toggleDoppler    = sf::Keyboard::Key::V;
    sf::Keyboard::Key toggleBlueshift  = sf::Keyboard::Key::U;
    sf::Keyboard::Key toggleHostGalaxy = sf::Keyboard::Key::Num1;
    sf::Keyboard::Key toggleLAB        = sf::Keyboard::Key::Num2;
    sf::Keyboard::Key toggleCGM        = sf::Keyboard::Key::Num3;
    sf::Keyboard::Key cycleAnimSpeed   = sf::Keyboard::Key::Y;
    sf::Keyboard::Key toggleHUD        = sf::Keyboard::Key::H;
    sf::Keyboard::Key toggleDebugHUD   = sf::Keyboard::Key::B;
    sf::Keyboard::Key resetTilt        = sf::Keyboard::Key::R;
    sf::Keyboard::Key nextProfile      = sf::Keyboard::Key::N;
    sf::Keyboard::Key toggleRender     = sf::Keyboard::Key::P;
    sf::Keyboard::Key releaseMouse     = sf::Keyboard::Key::Escape;
};

static const char* keyToString(sf::Keyboard::Key k);

static void drawHUD(const GLBitmapFont& font,
                    const ActionKeybinds& kb,
                    bool freelook,
                    bool jets, bool blr, bool orbBody, bool doppler, bool blueshift, bool fastOrbit,
                    bool hostGalaxy, bool lab, bool cgm,
                    float animSpd,
                    float fps,
                    bool cinematic,
                    float rollDeg,
                    const std::string& profileName,
                    int w, int h)
{
    std::string controls =
        "[WASD] Move  [Q/E] Tilt  [" + std::string(keyToString(kb.resetTilt)) + "] Reset Tilt  [Space/LCTRL] Up/Down  [LSHIFT] Fast  [RMB] Look\n"
        "[" + std::string(keyToString(kb.toggleFreelook)) + "] Freelook/Orbit  [" + std::string(keyToString(kb.toggleJets)) + "] Jets  [" + std::string(keyToString(kb.toggleBLR)) + "] BLR"
        "  [" + std::string(keyToString(kb.toggleOrbBody)) + "] Orb Body  [" + std::string(keyToString(kb.toggleDoppler)) + "] Doppler  [" + std::string(keyToString(kb.toggleBlueshift)) + "] Blueshift"
        "  [" + std::string(keyToString(kb.toggleRender)) + "] Cinematic\n"
        "[" + std::string(keyToString(kb.toggleHostGalaxy)) + "] Host Galaxy  [" + std::string(keyToString(kb.toggleLAB)) + "] LAB  [" + std::string(keyToString(kb.toggleCGM)) + "] CGM"
        "  [" + std::string(keyToString(kb.cycleAnimSpeed)) + "] Anim Speed  [" + std::string(keyToString(kb.nextProfile)) + "] Next Profile"
        "  [" + std::string(keyToString(kb.toggleHUD)) + "] Hide HUD  [" + std::string(keyToString(kb.toggleDebugHUD)) + "] Debug"
        "  [" + std::string(keyToString(kb.releaseMouse)) + "] Release\n\n";
    controls += "Profile: " + profileName;
    controls += "  |  Render: " + std::string(cinematic ? "CINEMATIC" : "FAST");
    controls += "  |  Mode: " + std::string(freelook ? "Freelook" : "Orbit");
    controls += "  |  Jets: " + std::string(jets ? "ON" : "OFF");
    controls += "  |  BLR: " + std::string(blr ? "ON" : "OFF");
    controls += "  |  Orb Body: " + std::string(orbBody ? "ON" : "OFF");
    controls += "  |  Doppler: " + std::string(doppler ? "ON" : "OFF");
    controls += "  |  Blueshift: " + std::string(blueshift ? "ON" : "OFF");
    controls += "  |  Host Galaxy: " + std::string(hostGalaxy ? "ON" : "OFF");
    controls += "  |  LAB: " + std::string(lab ? "ON" : "OFF");
    controls += "  |  CGM: " + std::string(cgm ? "ON" : "OFF");
    controls += "  |  Anim: " + std::to_string(static_cast<int>(animSpd + 0.5f)) + "x";
    controls += "  |  Tilt: " + std::to_string(static_cast<int>(rollDeg + 0.5f)) + " deg";
    controls += "  |  FPS: " + std::to_string(static_cast<int>(fps + 0.5f));

    font.drawText(controls, 10.f, 10.f, 0.86f, 0.86f, 0.86f, 0.78f, w, h);
}

/*-------------------- Keybinds & HUD loading --------------------*/
struct DebugInfo {
    bool cinematic, freelook, jets, blr, doppler;
    bool havePhotoreal, haveSimple;
    float animSpeed, fps, totalTime, animTime, rollDeg;
    int maxSteps, windowW, windowH;
    glm::vec3 camPos, camDir;
};

static std::string trimCopy(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

static std::string upperCopy(std::string s) {
    for (char& c : s) c = char(std::toupper((unsigned char)c));
    return s;
}

static const char* keyToString(sf::Keyboard::Key k) {
    switch (k) {
        case sf::Keyboard::Key::A: return "A";
        case sf::Keyboard::Key::B: return "B";
        case sf::Keyboard::Key::C: return "C";
        case sf::Keyboard::Key::D: return "D";
        case sf::Keyboard::Key::E: return "E";
        case sf::Keyboard::Key::F: return "F";
        case sf::Keyboard::Key::G: return "G";
        case sf::Keyboard::Key::H: return "H";
        case sf::Keyboard::Key::I: return "I";
        case sf::Keyboard::Key::J: return "J";
        case sf::Keyboard::Key::K: return "K";
        case sf::Keyboard::Key::L: return "L";
        case sf::Keyboard::Key::M: return "M";
        case sf::Keyboard::Key::N: return "N";
        case sf::Keyboard::Key::O: return "O";
        case sf::Keyboard::Key::P: return "P";
        case sf::Keyboard::Key::Q: return "Q";
        case sf::Keyboard::Key::R: return "R";
        case sf::Keyboard::Key::S: return "S";
        case sf::Keyboard::Key::T: return "T";
        case sf::Keyboard::Key::U: return "U";
        case sf::Keyboard::Key::V: return "V";
        case sf::Keyboard::Key::W: return "W";
        case sf::Keyboard::Key::X: return "X";
        case sf::Keyboard::Key::Y: return "Y";
        case sf::Keyboard::Key::Z: return "Z";
        case sf::Keyboard::Key::Num0: return "NUM0";
        case sf::Keyboard::Key::Num1: return "NUM1";
        case sf::Keyboard::Key::Num2: return "NUM2";
        case sf::Keyboard::Key::Num3: return "NUM3";
        case sf::Keyboard::Key::Num4: return "NUM4";
        case sf::Keyboard::Key::Num5: return "NUM5";
        case sf::Keyboard::Key::Num6: return "NUM6";
        case sf::Keyboard::Key::Num7: return "NUM7";
        case sf::Keyboard::Key::Num8: return "NUM8";
        case sf::Keyboard::Key::Num9: return "NUM9";
        case sf::Keyboard::Key::Space: return "SPACE";
        case sf::Keyboard::Key::LControl: return "LCTRL";
        case sf::Keyboard::Key::LShift: return "LSHIFT";
        case sf::Keyboard::Key::Escape: return "ESCAPE";
        default: return "UNKNOWN";
    }
}

static bool keyFromString(const std::string& raw, sf::Keyboard::Key& out) {
    const std::string s = upperCopy(trimCopy(raw));
    if (s.empty()) return false;
    if (s == "A") { out = sf::Keyboard::Key::A; return true; }
    if (s == "B") { out = sf::Keyboard::Key::B; return true; }
    if (s == "C") { out = sf::Keyboard::Key::C; return true; }
    if (s == "D") { out = sf::Keyboard::Key::D; return true; }
    if (s == "E") { out = sf::Keyboard::Key::E; return true; }
    if (s == "F") { out = sf::Keyboard::Key::F; return true; }
    if (s == "G") { out = sf::Keyboard::Key::G; return true; }
    if (s == "H") { out = sf::Keyboard::Key::H; return true; }
    if (s == "I") { out = sf::Keyboard::Key::I; return true; }
    if (s == "J") { out = sf::Keyboard::Key::J; return true; }
    if (s == "K") { out = sf::Keyboard::Key::K; return true; }
    if (s == "L") { out = sf::Keyboard::Key::L; return true; }
    if (s == "M") { out = sf::Keyboard::Key::M; return true; }
    if (s == "N") { out = sf::Keyboard::Key::N; return true; }
    if (s == "O") { out = sf::Keyboard::Key::O; return true; }
    if (s == "P") { out = sf::Keyboard::Key::P; return true; }
    if (s == "Q") { out = sf::Keyboard::Key::Q; return true; }
    if (s == "R") { out = sf::Keyboard::Key::R; return true; }
    if (s == "S") { out = sf::Keyboard::Key::S; return true; }
    if (s == "T") { out = sf::Keyboard::Key::T; return true; }
    if (s == "U") { out = sf::Keyboard::Key::U; return true; }
    if (s == "V") { out = sf::Keyboard::Key::V; return true; }
    if (s == "W") { out = sf::Keyboard::Key::W; return true; }
    if (s == "X") { out = sf::Keyboard::Key::X; return true; }
    if (s == "Y") { out = sf::Keyboard::Key::Y; return true; }
    if (s == "Z") { out = sf::Keyboard::Key::Z; return true; }
    if (s == "NUM0" || s == "0") { out = sf::Keyboard::Key::Num0; return true; }
    if (s == "NUM1" || s == "1") { out = sf::Keyboard::Key::Num1; return true; }
    if (s == "NUM2" || s == "2") { out = sf::Keyboard::Key::Num2; return true; }
    if (s == "NUM3" || s == "3") { out = sf::Keyboard::Key::Num3; return true; }
    if (s == "NUM4" || s == "4") { out = sf::Keyboard::Key::Num4; return true; }
    if (s == "NUM5" || s == "5") { out = sf::Keyboard::Key::Num5; return true; }
    if (s == "NUM6" || s == "6") { out = sf::Keyboard::Key::Num6; return true; }
    if (s == "NUM7" || s == "7") { out = sf::Keyboard::Key::Num7; return true; }
    if (s == "NUM8" || s == "8") { out = sf::Keyboard::Key::Num8; return true; }
    if (s == "NUM9" || s == "9") { out = sf::Keyboard::Key::Num9; return true; }
    if (s == "SPACE") { out = sf::Keyboard::Key::Space; return true; }
    if (s == "LCTRL" || s == "LCONTROL" || s == "CTRL") { out = sf::Keyboard::Key::LControl; return true; }
    if (s == "LSHIFT" || s == "SHIFT") { out = sf::Keyboard::Key::LShift; return true; }
    if (s == "ESC" || s == "ESCAPE") { out = sf::Keyboard::Key::Escape; return true; }
    return false;
}

static std::filesystem::path keybindConfigPath() {
    const char* home = std::getenv("HOME");
    std::filesystem::path base = home ? home : ".";
    return base / "Library" / "Application Support" / "Aetherion" / "blackhole3d_keybinds.cfg";
}

static void enforceKeybindConflicts(ActionKeybinds& keys) {
    const ActionKeybinds defaults{};
    auto isMovementKey = [](sf::Keyboard::Key k) {
        return k == sf::Keyboard::Key::W ||
               k == sf::Keyboard::Key::A ||
               k == sf::Keyboard::Key::S ||
               k == sf::Keyboard::Key::D ||
               k == sf::Keyboard::Key::Q ||
               k == sf::Keyboard::Key::E ||
               k == sf::Keyboard::Key::Space ||
               k == sf::Keyboard::Key::LControl ||
               k == sf::Keyboard::Key::LShift;
    };

    struct Entry { const char* name; sf::Keyboard::Key* key; sf::Keyboard::Key def; };
    std::array<Entry, 16> entries = {{
        {"toggle_freelook", &keys.toggleFreelook, defaults.toggleFreelook},
        {"toggle_jets", &keys.toggleJets, defaults.toggleJets},
        {"toggle_blr", &keys.toggleBLR, defaults.toggleBLR},
        {"toggle_orb_body", &keys.toggleOrbBody, defaults.toggleOrbBody},
        {"toggle_doppler", &keys.toggleDoppler, defaults.toggleDoppler},
        {"toggle_blueshift", &keys.toggleBlueshift, defaults.toggleBlueshift},
        {"toggle_host_galaxy", &keys.toggleHostGalaxy, defaults.toggleHostGalaxy},
        {"toggle_lab", &keys.toggleLAB, defaults.toggleLAB},
        {"toggle_cgm", &keys.toggleCGM, defaults.toggleCGM},
        {"cycle_anim_speed", &keys.cycleAnimSpeed, defaults.cycleAnimSpeed},
        {"toggle_hud", &keys.toggleHUD, defaults.toggleHUD},
        {"toggle_debug_hud", &keys.toggleDebugHUD, defaults.toggleDebugHUD},
        {"reset_tilt", &keys.resetTilt, defaults.resetTilt},
        {"next_profile", &keys.nextProfile, defaults.nextProfile},
        {"toggle_render", &keys.toggleRender, defaults.toggleRender},
        {"release_mouse", &keys.releaseMouse, defaults.releaseMouse}
    }};

    for (auto& e : entries) {
        if (isMovementKey(*e.key)) {
            std::cerr << "[keybinds] " << e.name << " conflicts with movement; reset to "
                      << keyToString(e.def) << "\n";
            *e.key = e.def;
        }
    }

    std::unordered_map<int, const char*> seen;
    for (auto& e : entries) {
        const int code = static_cast<int>(*e.key);
        auto it = seen.find(code);
        if (it != seen.end()) {
            std::cerr << "[keybinds] duplicate action key " << keyToString(*e.key)
                      << " for " << e.name << " and " << it->second
                      << "; reset " << e.name << " to " << keyToString(e.def) << "\n";
            *e.key = e.def;
        }
        seen[static_cast<int>(*e.key)] = e.name;
    }
}

static void writeDefaultKeybindFile(const std::filesystem::path& path, const ActionKeybinds& k) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    if (!out) return;
    out << "# Aetherion 3D keybinds\n";
    out << "# Format: action=KEY\n";
    out << "# Movement keys are fixed in this build: W A S D Q E SPACE LCTRL LSHIFT\n\n";
    out << "toggle_freelook=" << keyToString(k.toggleFreelook) << "\n";
    out << "toggle_jets=" << keyToString(k.toggleJets) << "\n";
    out << "toggle_blr=" << keyToString(k.toggleBLR) << "\n";
    out << "toggle_orb_body=" << keyToString(k.toggleOrbBody) << "\n";
    out << "toggle_doppler=" << keyToString(k.toggleDoppler) << "\n";
    out << "toggle_blueshift=" << keyToString(k.toggleBlueshift) << "\n";
    out << "toggle_host_galaxy=" << keyToString(k.toggleHostGalaxy) << "\n";
    out << "toggle_lab=" << keyToString(k.toggleLAB) << "\n";
    out << "toggle_cgm=" << keyToString(k.toggleCGM) << "\n";
    out << "cycle_anim_speed=" << keyToString(k.cycleAnimSpeed) << "\n";
    out << "toggle_hud=" << keyToString(k.toggleHUD) << "\n";
    out << "toggle_debug_hud=" << keyToString(k.toggleDebugHUD) << "\n";
    out << "reset_tilt=" << keyToString(k.resetTilt) << "\n";
    out << "next_profile=" << keyToString(k.nextProfile) << "\n";
    out << "toggle_render=" << keyToString(k.toggleRender) << "\n";
    out << "release_mouse=" << keyToString(k.releaseMouse) << "\n";
}

static ActionKeybinds loadActionKeybinds() {
    ActionKeybinds keys;
    const auto cfgPath = keybindConfigPath();

    if (!std::filesystem::exists(cfgPath)) {
        writeDefaultKeybindFile(cfgPath, keys);
        std::cerr << "[keybinds] created default config: " << cfgPath << "\n";
        return keys;
    }

    std::ifstream in(cfgPath);
    if (!in) {
        std::cerr << "[keybinds] failed to read config: " << cfgPath << "\n";
        return keys;
    }

    std::unordered_map<std::string, sf::Keyboard::Key*> targets = {
        {"toggle_freelook", &keys.toggleFreelook},
        {"toggle_jets", &keys.toggleJets},
        {"toggle_blr", &keys.toggleBLR},
        {"toggle_orb_body", &keys.toggleOrbBody},
        {"toggle_doppler", &keys.toggleDoppler},
        {"toggle_blueshift", &keys.toggleBlueshift},
        {"toggle_host_galaxy", &keys.toggleHostGalaxy},
        {"toggle_lab", &keys.toggleLAB},
        {"toggle_cgm", &keys.toggleCGM},
        {"cycle_anim_speed", &keys.cycleAnimSpeed},
        {"toggle_hud", &keys.toggleHUD},
        {"toggle_debug_hud", &keys.toggleDebugHUD},
        {"reset_tilt", &keys.resetTilt},
        {"next_profile", &keys.nextProfile},
        {"toggle_render", &keys.toggleRender},
        {"release_mouse", &keys.releaseMouse}
    };

    std::string line;
    while (std::getline(in, line)) {
        const auto trimmed = trimCopy(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;
        const auto eqPos = trimmed.find('=');
        if (eqPos == std::string::npos) continue;

        const std::string key = trimCopy(trimmed.substr(0, eqPos));
        const std::string value = trimCopy(trimmed.substr(eqPos + 1));
        auto it = targets.find(key);
        if (it == targets.end()) continue;

        sf::Keyboard::Key parsed{};
        if (keyFromString(value, parsed)) {
            *it->second = parsed;
        } else {
            std::cerr << "[keybinds] invalid key for " << key << ": " << value << "\n";
        }
    }

    enforceKeybindConflicts(keys);
    std::cerr << "[keybinds] loaded config: " << cfgPath << "\n";
    return keys;
}

static void drawDebugHUD(const GLBitmapFont& font, const DebugInfo& d)
{
    auto onOff = [](bool v) -> const char* { return v ? "ON" : "OFF"; };

    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        "=== DEBUG ===\n"
        "Shader:      %s\n"
        "  photoreal: %s  |  simple: %s\n"
        "Camera:      %s\n"
        "  pos: (%.1f, %.1f, %.1f)\n"
        "  dir: (%.2f, %.2f, %.2f)\n"
        "  tilt: %.1f deg\n"
        "Render:\n"
        "  maxSteps:  %d\n"
        "  cinematic: %s\n"
        "Features:\n"
        "  jets:      %s\n"
        "  BLR:       %s\n"
        "  doppler:   %s\n"
        "Animation:\n"
        "  speed:     %.0fx\n"
        "  animTime:  %.1fs\n"
        "  totalTime: %.1fs\n"
        "Window:      %dx%d\n"
        "FPS:         %.0f",
        d.cinematic ? "PHOTOREAL" : "SIMPLE",
        d.havePhotoreal ? "compiled" : "N/A",
        d.haveSimple    ? "compiled" : "N/A",
        d.freelook ? "Freelook" : "Orbit",
        d.camPos.x, d.camPos.y, d.camPos.z,
        d.camDir.x, d.camDir.y, d.camDir.z,
        d.rollDeg,
        d.maxSteps,
        onOff(d.cinematic),
        onOff(d.jets),
        onOff(d.blr),
        onOff(d.doppler),
        d.animSpeed,
        d.animTime,
        d.totalTime,
        d.windowW, d.windowH,
        d.fps
    );

    // Draw background panel
    font.drawRect(float(d.windowW) - 350.f, float(d.windowH) - 370.f,
                  340.f, 360.f,
                  0.f, 0.f, 0.f, 0.67f,
                  d.windowW, d.windowH);

    font.drawText(buf,
                  float(d.windowW) - 340.f, float(d.windowH) - 360.f,
                  0.63f, 1.0f, 0.63f, 0.86f,
                  d.windowW, d.windowH);
}

// ════════════════════════════════════════════════════════════
//                          MAIN
// ════════════════════════════════════════════════════════════
int main(int argc, char* argv[]) {
    /*--------- Black hole profiles ---------*/
    auto bhProfiles = profiles::allProfiles();
    int  profileIdx = 0;  // Start with TON 618

    // CLI: --preset <id>  (ton618, sgra, 3c273, j0529)
    for (int i = 1; i < argc - 1; ++i) {
        if (std::strcmp(argv[i], "--preset") == 0) {
            std::string id = argv[i + 1];
            if      (id == "ton618") profileIdx = 0;
            else if (id == "sgra")   profileIdx = 1;
            else if (id == "3c273")  profileIdx = 2;
            else if (id == "j0529")  profileIdx = 3;
            else std::cerr << "Unknown preset: " << id << ", using default.\n";
            break;
        }
    }

    cfg::SimConfig config = bhProfiles[profileIdx].config;
    ResourceManager resources;

    /*--------- Initialize orbital bodies based on profile ---------*/
    std::vector<OrbitalBody> orbBodies;
    if (bhProfiles[profileIdx].name == "TON 618") {
        // TON 618: supermassive black hole with surrounding stars and structures
        // Add multiple orbiting bodies at different distances
        cfg::OrbitalConfig star1 = config.orbital;
        star1.semiMajor = 80.0f;  // Close star
        star1.eccentricity = 0.1f;
        star1.bodyRadius = 1.2f;
        star1.bodyColor = glm::vec3(1.0f, 0.9f, 0.7f);  // Yellow-white star
        orbBodies.emplace_back(star1);

        cfg::OrbitalConfig star2 = config.orbital;
        star2.semiMajor = 120.0f;  // Medium distance
        star2.eccentricity = 0.2f;
        star2.inclination = 0.5f;  // Different inclination
        star2.bodyRadius = 1.0f;
        star2.bodyColor = glm::vec3(0.8f, 0.8f, 1.0f);  // Blue-white star
        orbBodies.emplace_back(star2);

        cfg::OrbitalConfig cluster = config.orbital;
        cluster.semiMajor = 200.0f;  // Farther out
        cluster.eccentricity = 0.05f;
        cluster.bodyRadius = 2.5f;  // Larger cluster
        cluster.bodyColor = glm::vec3(0.9f, 0.7f, 0.5f);  // Orange cluster
        orbBodies.emplace_back(cluster);

        cfg::OrbitalConfig nebula = config.orbital;
        nebula.semiMajor = 150.0f;
        nebula.eccentricity = 0.3f;
        nebula.inclination = -0.3f;
        nebula.bodyRadius = 3.0f;  // Large nebula
        nebula.bodyColor = glm::vec3(0.6f, 0.8f, 1.0f);  // Blue nebula
        orbBodies.emplace_back(nebula);

        // Additional smaller stars orbiting in the BLR region
        cfg::OrbitalConfig blrStar1 = config.orbital;
        blrStar1.semiMajor = 1500.0f;  // BLR distance (~0.01 pc)
        blrStar1.eccentricity = 0.4f;
        blrStar1.bodyRadius = 0.5f;  // Smaller star
        blrStar1.bodyColor = glm::vec3(1.0f, 0.8f, 0.6f);  // Warm star
        orbBodies.emplace_back(blrStar1);

        cfg::OrbitalConfig blrStar2 = config.orbital;
        blrStar2.semiMajor = 3000.0f;  // Further out in BLR
        blrStar2.eccentricity = 0.2f;
        blrStar2.inclination = 0.8f;
        blrStar2.bodyRadius = 0.4f;  // Even smaller
        blrStar2.bodyColor = glm::vec3(0.9f, 0.9f, 1.0f);  // Cool blue star
        orbBodies.emplace_back(blrStar2);

        cfg::OrbitalConfig blrStar3 = config.orbital;
        blrStar3.semiMajor = 5000.0f;  // Outer BLR
        blrStar3.eccentricity = 0.6f;
        blrStar3.inclination = -0.6f;
        blrStar3.bodyRadius = 0.6f;
        blrStar3.bodyColor = glm::vec3(1.0f, 1.0f, 0.8f);  // Yellow star
        orbBodies.emplace_back(blrStar3);
        // Default: single body for other profiles
        orbBodies.emplace_back(config.orbital);
    }

    /*--------- Window & GL context ---------*/
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 3;
    settings.minorVersion = 3;
    settings.attributeFlags = sf::ContextSettings::Core;

    sf::RenderWindow window(
        sf::VideoMode({cfg::DEFAULT_WIDTH, cfg::DEFAULT_HEIGHT}),
        "GLSL Black Hole",
        sf::Style::Default,
        sf::State::Windowed,
        settings);
    window.setVerticalSyncEnabled(false);  // Disable VSync — measure true FPS
    window.setKeyRepeatEnabled(false);
    if (!window.setActive(true)) {
        std::cerr << "Failed to activate OpenGL context.\n";
        return 1;
    }

    /*--------- Bitmap Font (pure GL, no SFML in render loop) ---------*/
    GLBitmapFont glFont;
    bool fontLoaded = false;
    {
        sf::Font tmpFont;
        // Pre-check existence before calling openFromFile so SFML never prints
        // a "Failed to load font" error for paths absent on the current platform.
        // Flatpak runtime: /usr/share/fonts/dejavu/  (no truetype/ subdir)
        auto tryFont = [&](const std::string& path) -> bool {
            std::ifstream probe(path);
            return probe.good() && tmpFont.openFromFile(path);
        };
        bool loaded =
            tryFont("/usr/share/fonts/dejavu/DejaVuSansMono.ttf")                       ||
            tryFont("/usr/share/fonts/liberation-fonts/LiberationMono-Regular.ttf")     ||
            tryFont("/run/host/fonts/truetype/dejavu/DejaVuSansMono.ttf")               ||
            tryFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf")              ||
            tryFont("/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf")  ||
            tryFont("/System/Library/Fonts/Menlo.ttc")                                  ||
            tryFont("/System/Library/Fonts/SFNSMono.ttf");
        if (loaded) {
            // Reactivate main context before creating GL objects
            (void)window.setActive(true);
            fontLoaded = glFont.initFromFont(tmpFont, 14);
        }
        if (!fontLoaded) std::cerr << "Warning: Could not load font for HUD.\n";
    }
    // Ensure main context is active after font init
    (void)window.setActive(true);

    glViewport(0, 0, cfg::DEFAULT_WIDTH, cfg::DEFAULT_HEIGHT);

    /*--------- Compile shaders: photoreal (cinematic) + simple (fast) ---------*/
    GLShader sharedVS(GL_VERTEX_SHADER, vertexShaderSrc);
    if (!sharedVS) return 1;

    // Try to compile the photoreal shader
    GLProgram photorealProgram;
    bool havePhotoreal = false;
    {
        std::string fragPath = resources.find("BlackHole3D_PhotorealDisk.frag");
        if (!fragPath.empty()) {
            std::string fragSrc = readFile(fragPath.c_str());
            if (!fragSrc.empty()) {
                GLShader fs(GL_FRAGMENT_SHADER, fragSrc.c_str());
                if (fs && photorealProgram.link(sharedVS.id(), fs.id())) {
                    havePhotoreal = true;
                    std::cerr << "Compiled photoreal shader: " << fragPath << std::endl;
                }
            }
        }
    }

    // Try to compile the simple/fast shader
    GLProgram simpleProgram;
    bool haveSimple = false;
    {
        std::string fragPath = resources.find("BlackHole3D.frag");
        if (!fragPath.empty()) {
            std::string fragSrc = readFile(fragPath.c_str());
            if (!fragSrc.empty()) {
                GLShader fs(GL_FRAGMENT_SHADER, fragSrc.c_str());
                if (fs && simpleProgram.link(sharedVS.id(), fs.id())) {
                    haveSimple = true;
                    std::cerr << "Compiled simple shader: " << fragPath << std::endl;
                }
            }
        }
    }

    if (!havePhotoreal && !haveSimple) {
        std::cerr << "FATAL: No fragment shader compiled! Expected BlackHole3D*.frag\n";
        std::cerr << "Searched: " << resources.paths() << "\n";
        sf::sleep(sf::seconds(3.0f));
        return 1;
    }

    // Uniform lookups for each available program
    SceneUniforms photorealUfs{}, simpleUfs{};
    if (havePhotoreal) photorealUfs = SceneUniforms::lookup(photorealProgram);
    if (haveSimple)    simpleUfs    = SceneUniforms::lookup(simpleProgram);

    // Active program pointers — start cinematic (photoreal) if available
    bool cinematicMode = havePhotoreal;
    GLProgram*    activeProgram = cinematicMode ? &photorealProgram : &simpleProgram;
    SceneUniforms activeUfs     = cinematicMode ? photorealUfs      : simpleUfs;

    /*--------- Shared full-screen quad ---------*/
    GLVertexArray quad = GLVertexArray::makeFullScreenQuad();

    /*--------- Textures (using resource manager) ---------*/
    GLTexture2D bgTex, photorealDiskTex, simpleDiskTex;
    {
        std::string bgPath = resources.find("background.png");
        if (!bgPath.empty()) bgTex = loadTexture2D(bgPath.c_str());
        if (!bgTex) {
            std::cerr << "No background.png found; using white fallback.\n";
            bgTex = createFallbackWhiteTexture();
        }
    }
    {
        // Photoreal shader generates disk color procedurally — just needs a white placeholder
        photorealDiskTex = createFallbackWhiteTexture();
    }
    {
        // Simple shader samples disk texture directly — needs a real texture
        std::string diskPath = resources.find("disk_texture.png");
        if (!diskPath.empty()) simpleDiskTex = loadTexture2D(diskPath.c_str());
        if (!simpleDiskTex) {
            std::cerr << "No disk_texture.png found; generating procedural.\n";
            simpleDiskTex = createDiskTextureProcedural(512);
        }
    }

    /*--------- Bloom pipeline ---------*/
    BloomPipeline bloom;
    if (!bloom.init(cfg::DEFAULT_WIDTH, cfg::DEFAULT_HEIGHT)) {
        std::cerr << "Failed to initialize bloom pipeline!\n";
        return 1;
    }

    // (HUD uses GLBitmapFont — pure GL, no SFML RenderTexture needed)

    /*--------- Controllers ---------*/
    CameraController camera(config.camera);
    KeyState          keys;

    /*--------- Toggle states (initialized from profile defaults) ---------*/
    bool jetsEnabled    = bhProfiles[profileIdx].defaultJets;
    bool blrEnabled     = bhProfiles[profileIdx].defaultBLR;
    bool orbBodyEnabled = bhProfiles[profileIdx].defaultOrbBody;
    bool dopplerEnabled = bhProfiles[profileIdx].defaultDoppler;
    bool hostGalaxyEnabled = bhProfiles[profileIdx].defaultHostGalaxy;
    bool labEnabled = bhProfiles[profileIdx].defaultLAB;
    bool cgmEnabled = bhProfiles[profileIdx].defaultCGM;
    bool blueshiftEnabled = true;
    bool showHUD        = true;
    bool showDebugHUD   = false;

    /*--------- Animation speed control ---------*/
    // Presets: 1× (default), 3× (visible), 8× (dramatic)
    constexpr float animSpeedPresets[] = { 1.0f, 3.0f, 8.0f };
    constexpr int   animSpeedCount     = 3;
    int   animSpeedIdx = 1;                        // start at 3× for visibility
    float animSpeed    = animSpeedPresets[animSpeedIdx];
    float animTime     = 0.0f;                     // scaled animation clock

    bool looking = false;
    sf::Vector2i lastMousePos(0, 0);
    float totalTime = 0.0f;

    /*--------- FPS counter ---------*/
    sf::Clock frameClock, fpsClock;
    int fpsFrameCount = 0;
    float fpsDisplay  = 0.0f;

    ActionKeybinds actionKeys = loadActionKeybinds();

    // Persistent snapshot: re-populated each frame without constructing/destructing
    // the internal std::vector members (orbBodyPositions/Radii/Colors) every frame.
    PhysicsSnapshot snap{};
    snap.orbBodyPositions.reserve(10);
    snap.orbBodyRadii.reserve(10);
    snap.orbBodyColors.reserve(10);

    // ════════════════════════════════════════════════════════
    // =                   MAIN FUNCTION LOOP                 =
    // ════════════════════════════════════════════════════════
    while (window.isOpen()) { // while the window is open, run the main loop
        const float dt = frameClock.restart().asSeconds();

        /*--------- FPS ---------*/
        fpsFrameCount++;
        if (fpsClock.getElapsedTime().asSeconds() >= 0.5f) {
            fpsDisplay = float(fpsFrameCount) / fpsClock.restart().asSeconds();
            fpsFrameCount = 0;
        }

        /*--------- Events ---------*/
        while (auto evOpt = window.pollEvent()) {
            const auto& event = *evOpt;
            if (event.is<sf::Event::Closed>()) window.close();

            if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
                keys.onKeyPressed(kp->code);
                if (kp->code == actionKeys.toggleFreelook) {
                    camera.toggleMode();
                } else if (kp->code == actionKeys.toggleJets) {
                    jetsEnabled = !jetsEnabled;
                } else if (kp->code == actionKeys.toggleBLR) {
                    blrEnabled = !blrEnabled;
                } else if (kp->code == actionKeys.toggleOrbBody) {
                    orbBodyEnabled = !orbBodyEnabled;
                } else if (kp->code == actionKeys.toggleDoppler) {
                    dopplerEnabled = !dopplerEnabled;
                } else if (kp->code == actionKeys.toggleBlueshift) {
                    blueshiftEnabled = !blueshiftEnabled;
                } else if (kp->code == actionKeys.toggleHostGalaxy) {
                    hostGalaxyEnabled = !hostGalaxyEnabled;
                } else if (kp->code == actionKeys.toggleLAB) {
                    labEnabled = !labEnabled;
                } else if (kp->code == actionKeys.toggleCGM) {
                    cgmEnabled = !cgmEnabled;
                } else if (kp->code == actionKeys.cycleAnimSpeed) {
                    animSpeedIdx = (animSpeedIdx + 1) % animSpeedCount;
                    animSpeed = animSpeedPresets[animSpeedIdx];
                } else if (kp->code == actionKeys.toggleHUD) {
                    showHUD = !showHUD;
                    std::cerr << "HUD: " << (showHUD ? "ON" : "OFF") << std::endl;
                } else if (kp->code == actionKeys.toggleDebugHUD) {
                    showDebugHUD = !showDebugHUD;
                    std::cerr << "Debug HUD: " << (showDebugHUD ? "ON" : "OFF") << std::endl;
                } else if (kp->code == actionKeys.resetTilt) {
                    camera.resetRoll();
                } else if (kp->code == actionKeys.nextProfile) {
                        profileIdx = (profileIdx + 1) % profiles::NUM_PROFILES;
                        const auto& prof = bhProfiles[profileIdx];
                        config = prof.config;
                        jetsEnabled    = prof.defaultJets;
                        blrEnabled     = prof.defaultBLR;
                        orbBodyEnabled = prof.defaultOrbBody;
                        dopplerEnabled = prof.defaultDoppler;
                        camera = CameraController(config.camera);
                        // Reinitialize orbital bodies for new profile
                        orbBodies.clear();
                        if (prof.name == "TON 618") {
                            cfg::OrbitalConfig star1 = config.orbital;
                            star1.semiMajor = 80.0f;
                            star1.eccentricity = 0.1f;
                            star1.bodyRadius = 1.2f;
                            star1.bodyColor = glm::vec3(1.0f, 0.9f, 0.7f);
                            orbBodies.emplace_back(star1);

                            cfg::OrbitalConfig star2 = config.orbital;
                            star2.semiMajor = 120.0f;
                            star2.eccentricity = 0.2f;
                            star2.inclination = 0.5f;
                            star2.bodyRadius = 1.0f;
                            star2.bodyColor = glm::vec3(0.8f, 0.8f, 1.0f);
                            orbBodies.emplace_back(star2);

                            cfg::OrbitalConfig cluster = config.orbital;
                            cluster.semiMajor = 200.0f;
                            cluster.eccentricity = 0.05f;
                            cluster.bodyRadius = 2.5f;
                            cluster.bodyColor = glm::vec3(0.9f, 0.7f, 0.5f);
                            orbBodies.emplace_back(cluster);

                            cfg::OrbitalConfig nebula = config.orbital;
                            nebula.semiMajor = 150.0f;
                            nebula.eccentricity = 0.3f;
                            nebula.inclination = -0.3f;
                            nebula.bodyRadius = 3.0f;
                            nebula.bodyColor = glm::vec3(0.6f, 0.8f, 1.0f);
                            orbBodies.emplace_back(nebula);
                        } else {
                            orbBodies.emplace_back(config.orbital);
                        }
                        std::cerr << "Profile: " << prof.name
                                  << " — " << prof.description << std::endl;
                } else if (kp->code == actionKeys.toggleRender) {
                    if (havePhotoreal && haveSimple) {
                        cinematicMode = !cinematicMode;
                        activeProgram = cinematicMode ? &photorealProgram : &simpleProgram;
                        activeUfs     = cinematicMode ? photorealUfs      : simpleUfs;
                    }
                } else if (kp->code == actionKeys.releaseMouse) {
                    looking = false;
                    window.setMouseCursorGrabbed(false);
                    window.setMouseCursorVisible(true);
                }
            }
            if (const auto* kr = event.getIf<sf::Event::KeyReleased>()) {
                keys.onKeyReleased(kr->code);
            }
            if (const auto* resized = event.getIf<sf::Event::Resized>()) {
                glViewport(0, 0, (GLsizei)resized->size.x, (GLsizei)resized->size.y);
            }
            if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button == sf::Mouse::Button::Right) {
                    looking = true;
                    window.setMouseCursorGrabbed(true);
                    window.setMouseCursorVisible(false);
                    lastMousePos = mb->position;
                }
            }
            if (const auto* mb = event.getIf<sf::Event::MouseButtonReleased>()) {
                if (mb->button == sf::Mouse::Button::Right) {
                    looking = false;
                    window.setMouseCursorGrabbed(false);
                    window.setMouseCursorVisible(true);
                }
            }
            if (const auto* mm = event.getIf<sf::Event::MouseMoved>()) {
                if (looking) {
                    sf::Vector2i delta = mm->position - lastMousePos;
                    lastMousePos = mm->position;
                    camera.onMouseDelta(float(delta.x), float(delta.y));
                }
            }
        }

        // ═══════════════════════════════════════════════════
        // =              PHYSICS UPDATE                       =
        // ═══════════════════════════════════════════════════
        totalTime += dt;
        animTime  += dt * animSpeed;
        camera.update(dt, keys);
        for (auto& body : orbBodies) {
            body.update(dt);
        }

        int w = std::max(1, (int)window.getSize().x);
        int h = std::max(1, (int)window.getSize().y);

        /*--------- Build physics snapshot (the ONLY data crossing into rendering) ---------*/
        snap.cameraPos    = camera.position();
        snap.cameraDir    = camera.direction();
        snap.cameraUp     = camera.up();
        snap.fov          = camera.fov();
        snap.roll         = camera.roll();
        snap.freelook     = camera.isFreelook();
        snap.totalTime    = totalTime;
        snap.animTime     = animTime;
        snap.animSpeed    = animSpeed;
        snap.dt           = dt;
        snap.jetsEnabled    = jetsEnabled;
        snap.blrEnabled     = blrEnabled;
        snap.dopplerEnabled = dopplerEnabled;
        snap.blueshiftEnabled = blueshiftEnabled;
        snap.cinematicMode  = cinematicMode;
        snap.bhRadius         = config.blackHole.radius;
        snap.bhPosition       = config.blackHole.position;
        snap.bhSpin           = config.blackHole.spinParameter;
        snap.diskInnerRadius  = config.disk.innerRadius;
        snap.diskOuterRadius  = config.disk.outerRadius;
        snap.diskHalfThickness= config.disk.halfThickness;
        snap.diskPeakTemp         = config.disk.peakTemp;
        snap.diskDisplayTempInner = config.disk.displayTempInner;
        snap.diskDisplayTempOuter = config.disk.displayTempOuter;
        snap.diskSatBoostInner    = config.disk.saturationBoostInner;
        snap.diskSatBoostOuter    = config.disk.saturationBoostOuter;
        snap.jetRadius        = config.jet.radius;
        snap.jetLength        = config.jet.length;
        snap.jetColor         = config.jet.color;
        snap.blrInnerRadius   = config.blr.innerRadius;
        snap.blrOuterRadius   = config.blr.outerRadius;
        snap.blrThickness     = config.blr.thickness;
        snap.orbBodyPositions.clear();
        snap.orbBodyRadii.clear();
        snap.orbBodyColors.clear();
        for (const auto& body : orbBodies) {
            snap.orbBodyPositions.push_back(body.position());
            snap.orbBodyRadii.push_back(body.bodyRadius());
            snap.orbBodyColors.push_back(body.bodyColor());
        }
        snap.orbBodyEnabled   = orbBodyEnabled;
        snap.hostGalaxyEnabled = hostGalaxyEnabled;
        snap.labEnabled = labEnabled;
        snap.cgmEnabled = cgmEnabled;
        snap.maxSteps         = cinematicMode ? 300 : 200;
        snap.profileName      = bhProfiles[profileIdx].name;
        snap.windowW          = w;
        snap.windowH          = h;
        snap.fps              = fpsDisplay;

        // ═══════════════════════════════════════════════════
        // =              RENDERING (reads only snap)          =
        // ═══════════════════════════════════════════════════
        static int lastW = 0, lastH = 0;
        if (w != lastW || h != lastH) {
            bloom.resize(w, h);
            lastW = w; lastH = h;
        }

        /*--------- Render Pass 1: scene → framebuffer ---------*/
        if (cinematicMode) {
            // Cinematic: render to HDR FBO then run full bloom pipeline
            bloom.sceneFBO.bind();
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            setSceneUniforms(*activeProgram, activeUfs, snap,
                             bgTex.id(), photorealDiskTex.id());
            quad.drawQuad();

            bloom.execute(quad, cfg::cinematicBloom(), true, totalTime);
        } else {
            // Fast mode: render directly to screen — skip all 10 HDR FBOs to
            // save VRAM bandwidth (critical on iGPUs where FBO == system RAM).
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, w, h);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            setSceneUniforms(*activeProgram, activeUfs, snap,
                             bgTex.id(), simpleDiskTex.id());
            quad.drawQuad();
        }

        /*--------- HUD overlay (pure GL bitmap font) ---------*/
        // Always rebind FBO 0 before the HUD (or display) — bloom.execute() saves+restores
        // prevFBO which ends up as sceneFBO, not 0. Without this, display() runs with
        // sceneFBO bound, causing a stall / blank frame on macOS Metal-backed GL.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w, h);

        bool drawAnyHUD = (showHUD || showDebugHUD) && fontLoaded;
        if (drawAnyHUD) {

            // Clean up texture units from bloom
            for (int i = 0; i < 8; ++i) {
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            float rollDeg = snap.roll * (180.0f / 3.14159265f);

            if (showHUD) {
                drawHUD(glFont,
                        actionKeys,
                        snap.freelook,
                        snap.jetsEnabled, snap.blrEnabled, snap.orbBodyEnabled, snap.dopplerEnabled, snap.blueshiftEnabled,
                        false, // orbit speed removed
                        snap.hostGalaxyEnabled, snap.labEnabled, snap.cgmEnabled,
                        snap.animSpeed,
                        snap.fps,
                        snap.cinematicMode,
                        rollDeg,
                        snap.profileName,
                        snap.windowW, snap.windowH);
            }

            if (showDebugHUD) {
                DebugInfo dbg{};
                dbg.cinematic      = snap.cinematicMode;
                dbg.freelook       = snap.freelook;
                dbg.jets            = snap.jetsEnabled;
                dbg.blr             = snap.blrEnabled;
                dbg.doppler         = snap.dopplerEnabled;
                dbg.havePhotoreal   = havePhotoreal;
                dbg.haveSimple      = haveSimple;
                dbg.animSpeed       = snap.animSpeed;
                dbg.fps             = snap.fps;
                dbg.totalTime       = snap.totalTime;
                dbg.animTime        = snap.animTime;
                dbg.maxSteps        = snap.maxSteps;
                dbg.windowW         = snap.windowW;
                dbg.windowH         = snap.windowH;
                dbg.camPos          = snap.cameraPos;
                dbg.camDir          = snap.cameraDir;
                dbg.rollDeg         = rollDeg;
                drawDebugHUD(glFont, dbg);
            }

            glDisable(GL_BLEND);
        }

        window.display();
        while (glGetError() != GL_NO_ERROR) {}
    }

    // All RAII objects auto-clean, but verify
    std::cerr << "All GL resources cleaned automatically.\n";
    return 0;
}
