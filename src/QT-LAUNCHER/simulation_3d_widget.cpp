// ============================================================
// simulation_3d_widget.cpp — 3D black hole simulation embedded in Qt.
// Mirrors the BlackHole3D.cpp render loop inside onInit/onUpdate.
// ============================================================

#include "simulation_3d_widget.h"
#include "custom_bh_dialog.h"

// All GL / SFML / simulation headers go here, NOT in the .h
#include "platform.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

#include <SFML/Graphics/Font.hpp>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

// ─────────────────────────────────────────────────────────────
// Helper types and functions (mirrored from BlackHole3D.cpp,
// placed in anonymous namespace to avoid ODR conflicts)
// ─────────────────────────────────────────────────────────────
namespace {

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
        u.resolution           = prog.uniform("resolution");
        u.cameraPos            = prog.uniform("cameraPos");
        u.cameraDir            = prog.uniform("cameraDir");
        u.cameraUp             = prog.uniform("cameraUp");
        u.fov                  = prog.uniform("fov");
        u.blackHoleRadius      = prog.uniform("blackHoleRadius");
        u.blackHolePos         = prog.uniform("blackHolePos");
        u.backgroundTex        = prog.uniform("backgroundTex");
        u.diskTex              = prog.uniform("diskTex");
        u.diskInnerRadius      = prog.uniform("diskInnerRadius");
        u.diskOuterRadius      = prog.uniform("diskOuterRadius");
        u.diskHalfThickness    = prog.uniform("diskHalfThickness");
        u.spinParameter        = prog.uniform("spinParameter");
        u.jetRadius            = prog.uniform("jetRadius");
        u.jetLength            = prog.uniform("jetLength");
        u.jetColor             = prog.uniform("jetColor");
        u.showJets             = prog.uniform("showJets");
        u.showBLR              = prog.uniform("showBLR");
        u.showDoppler          = prog.uniform("showDoppler");
        u.showBlueshift        = prog.uniform("showBlueshift");
        u.blrInnerRadius       = prog.uniform("blrInnerRadius");
        u.blrOuterRadius       = prog.uniform("blrOuterRadius");
        u.blrThickness         = prog.uniform("blrThickness");
        u.uTime                = prog.uniform("uTime");
        u.showOrbBody          = prog.uniform("showOrbBody");
        for (int i = 0; i < 10; ++i) {
            char buf[64];
            snprintf(buf, sizeof(buf), "orbBodyPos[%d]", i);    u.orbBodyPos[i]    = prog.uniform(buf);
            snprintf(buf, sizeof(buf), "orbBodyRadius[%d]", i); u.orbBodyRadius[i] = prog.uniform(buf);
            snprintf(buf, sizeof(buf), "orbBodyColor[%d]", i);  u.orbBodyColor[i]  = prog.uniform(buf);
        }
        u.numOrbBodies         = prog.uniform("numOrbBodies");
        u.showHostGalaxy       = prog.uniform("showHostGalaxy");
        u.hostGalaxyRadius     = prog.uniform("hostGalaxyRadius");
        u.hostGalaxyColor      = prog.uniform("hostGalaxyColor");
        u.showLAB              = prog.uniform("showLAB");
        u.labPos               = prog.uniform("labPos");
        u.labRadius            = prog.uniform("labRadius");
        u.labColor             = prog.uniform("labColor");
        u.showCGM              = prog.uniform("showCGM");
        u.cgmRadius            = prog.uniform("cgmRadius");
        u.cgmColor             = prog.uniform("cgmColor");
        u.maxStepsOverride     = prog.uniform("maxStepsOverride");
        u.diskPeakTemp         = prog.uniform("diskPeakTemp");
        u.diskDisplayTempInner = prog.uniform("diskDisplayTempInner");
        u.diskDisplayTempOuter = prog.uniform("diskDisplayTempOuter");
        u.diskSatBoostInner    = prog.uniform("diskSatBoostInner");
        u.diskSatBoostOuter    = prog.uniform("diskSatBoostOuter");
        return u;
    }
};

static void setSceneUniforms(GLProgram& prog, const SceneUniforms& u,
                              const PhysicsSnapshot& snap, GLuint bgTexId, GLuint diskTexId)
{
    prog.use();
    glUniform2f(u.resolution,     float(snap.windowW), float(snap.windowH));
    glUniform3f(u.cameraPos,      snap.cameraPos.x, snap.cameraPos.y, snap.cameraPos.z);
    glUniform3f(u.cameraDir,      snap.cameraDir.x, snap.cameraDir.y, snap.cameraDir.z);
    glUniform3f(u.cameraUp,       snap.cameraUp.x,  snap.cameraUp.y,  snap.cameraUp.z);
    glUniform1f(u.fov,            snap.fov);
    glUniform1f(u.blackHoleRadius, snap.bhRadius);
    glUniform3f(u.blackHolePos,   snap.bhPosition.x, snap.bhPosition.y, snap.bhPosition.z);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, bgTexId);   glUniform1i(u.backgroundTex, 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, diskTexId); glUniform1i(u.diskTex, 1);

    glUniform1f(u.diskInnerRadius,       snap.diskInnerRadius);
    glUniform1f(u.diskOuterRadius,       snap.diskOuterRadius);
    glUniform1f(u.diskHalfThickness,     snap.diskHalfThickness);
    glUniform1f(u.diskPeakTemp,          snap.diskPeakTemp);
    glUniform1f(u.diskDisplayTempInner,  snap.diskDisplayTempInner);
    glUniform1f(u.diskDisplayTempOuter,  snap.diskDisplayTempOuter);
    glUniform1f(u.diskSatBoostInner,     snap.diskSatBoostInner);
    glUniform1f(u.diskSatBoostOuter,     snap.diskSatBoostOuter);
    glUniform1f(u.spinParameter,         snap.bhSpin);
    glUniform1f(u.jetRadius,             snap.jetRadius);
    glUniform1f(u.jetLength,             snap.jetLength);
    glUniform3f(u.jetColor,              snap.jetColor.x, snap.jetColor.y, snap.jetColor.z);
    glUniform1i(u.showJets,              snap.jetsEnabled    ? 1 : 0);
    glUniform1i(u.showBLR,              snap.blrEnabled     ? 1 : 0);
    glUniform1i(u.showDoppler,           snap.dopplerEnabled  ? 1 : 0);
    glUniform1i(u.showBlueshift,         snap.blueshiftEnabled ? 1 : 0);
    glUniform1f(u.blrInnerRadius,        snap.blrInnerRadius);
    glUniform1f(u.blrOuterRadius,        snap.blrOuterRadius);
    glUniform1f(u.blrThickness,          snap.blrThickness);
    glUniform1f(u.uTime,                 snap.animTime);
    glUniform1i(u.showOrbBody,           snap.orbBodyEnabled ? 1 : 0);
    glUniform1i(u.numOrbBodies,          (GLint)snap.orbBodyPositions.size());
    for (size_t i = 0; i < snap.orbBodyPositions.size() && i < 10; ++i) {
        glUniform3f(u.orbBodyPos[i],    snap.orbBodyPositions[i].x, snap.orbBodyPositions[i].y, snap.orbBodyPositions[i].z);
        glUniform1f(u.orbBodyRadius[i], snap.orbBodyRadii[i]);
        glUniform3f(u.orbBodyColor[i],  snap.orbBodyColors[i].x,    snap.orbBodyColors[i].y,    snap.orbBodyColors[i].z);
    }
    glUniform1i(u.showHostGalaxy,   snap.hostGalaxyEnabled ? 1 : 0);
    glUniform1f(u.hostGalaxyRadius, 10.0f);
    glUniform3f(u.hostGalaxyColor,  0.8f, 0.6f, 0.4f);
    glUniform1i(u.showLAB,          snap.labEnabled ? 1 : 0);
    glUniform3f(u.labPos,           50.0f, 0.0f, 0.0f);
    glUniform1f(u.labRadius,        100.0f);
    glUniform3f(u.labColor,         0.4f, 0.7f, 1.0f);
    glUniform1i(u.showCGM,          snap.cgmEnabled ? 1 : 0);
    glUniform1f(u.cgmRadius,        200.0f);
    glUniform3f(u.cgmColor,         0.3f, 0.5f, 0.8f);
    glUniform1i(u.maxStepsOverride,  snap.maxSteps);
}

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

static std::string trimCopy(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::string upperCopy(std::string s) {
    for (char& c : s) c = char(std::toupper((unsigned char)c));
    return s;
}

static const char* keyToString(sf::Keyboard::Key k) {
    switch (k) {
        case sf::Keyboard::Key::A: return "A"; case sf::Keyboard::Key::B: return "B";
        case sf::Keyboard::Key::C: return "C"; case sf::Keyboard::Key::D: return "D";
        case sf::Keyboard::Key::E: return "E"; case sf::Keyboard::Key::F: return "F";
        case sf::Keyboard::Key::G: return "G"; case sf::Keyboard::Key::H: return "H";
        case sf::Keyboard::Key::I: return "I"; case sf::Keyboard::Key::J: return "J";
        case sf::Keyboard::Key::K: return "K"; case sf::Keyboard::Key::L: return "L";
        case sf::Keyboard::Key::M: return "M"; case sf::Keyboard::Key::N: return "N";
        case sf::Keyboard::Key::O: return "O"; case sf::Keyboard::Key::P: return "P";
        case sf::Keyboard::Key::Q: return "Q"; case sf::Keyboard::Key::R: return "R";
        case sf::Keyboard::Key::S: return "S"; case sf::Keyboard::Key::T: return "T";
        case sf::Keyboard::Key::U: return "U"; case sf::Keyboard::Key::V: return "V";
        case sf::Keyboard::Key::W: return "W"; case sf::Keyboard::Key::X: return "X";
        case sf::Keyboard::Key::Y: return "Y"; case sf::Keyboard::Key::Z: return "Z";
        case sf::Keyboard::Key::Num0:     return "NUM0"; case sf::Keyboard::Key::Num1: return "NUM1";
        case sf::Keyboard::Key::Num2:     return "NUM2"; case sf::Keyboard::Key::Num3: return "NUM3";
        case sf::Keyboard::Key::Num4:     return "NUM4"; case sf::Keyboard::Key::Num5: return "NUM5";
        case sf::Keyboard::Key::Num6:     return "NUM6"; case sf::Keyboard::Key::Num7: return "NUM7";
        case sf::Keyboard::Key::Num8:     return "NUM8"; case sf::Keyboard::Key::Num9: return "NUM9";
        case sf::Keyboard::Key::Space:    return "SPACE";
        case sf::Keyboard::Key::LControl: return "LCTRL";
        case sf::Keyboard::Key::LShift:   return "LSHIFT";
        case sf::Keyboard::Key::Escape:   return "ESCAPE";
        default: return "UNKNOWN";
    }
}

static bool keyFromString(const std::string& raw, sf::Keyboard::Key& out) {
    const std::string s = upperCopy(trimCopy(raw));
    if (s.empty()) return false;
    if (s=="A"){out=sf::Keyboard::Key::A;return true;} if (s=="B"){out=sf::Keyboard::Key::B;return true;}
    if (s=="C"){out=sf::Keyboard::Key::C;return true;} if (s=="D"){out=sf::Keyboard::Key::D;return true;}
    if (s=="E"){out=sf::Keyboard::Key::E;return true;} if (s=="F"){out=sf::Keyboard::Key::F;return true;}
    if (s=="G"){out=sf::Keyboard::Key::G;return true;} if (s=="H"){out=sf::Keyboard::Key::H;return true;}
    if (s=="I"){out=sf::Keyboard::Key::I;return true;} if (s=="J"){out=sf::Keyboard::Key::J;return true;}
    if (s=="K"){out=sf::Keyboard::Key::K;return true;} if (s=="L"){out=sf::Keyboard::Key::L;return true;}
    if (s=="M"){out=sf::Keyboard::Key::M;return true;} if (s=="N"){out=sf::Keyboard::Key::N;return true;}
    if (s=="O"){out=sf::Keyboard::Key::O;return true;} if (s=="P"){out=sf::Keyboard::Key::P;return true;}
    if (s=="Q"){out=sf::Keyboard::Key::Q;return true;} if (s=="R"){out=sf::Keyboard::Key::R;return true;}
    if (s=="S"){out=sf::Keyboard::Key::S;return true;} if (s=="T"){out=sf::Keyboard::Key::T;return true;}
    if (s=="U"){out=sf::Keyboard::Key::U;return true;} if (s=="V"){out=sf::Keyboard::Key::V;return true;}
    if (s=="W"){out=sf::Keyboard::Key::W;return true;} if (s=="X"){out=sf::Keyboard::Key::X;return true;}
    if (s=="Y"){out=sf::Keyboard::Key::Y;return true;} if (s=="Z"){out=sf::Keyboard::Key::Z;return true;}
    if (s=="NUM0"||s=="0"){out=sf::Keyboard::Key::Num0;return true;}
    if (s=="NUM1"||s=="1"){out=sf::Keyboard::Key::Num1;return true;}
    if (s=="NUM2"||s=="2"){out=sf::Keyboard::Key::Num2;return true;}
    if (s=="NUM3"||s=="3"){out=sf::Keyboard::Key::Num3;return true;}
    if (s=="NUM4"||s=="4"){out=sf::Keyboard::Key::Num4;return true;}
    if (s=="NUM5"||s=="5"){out=sf::Keyboard::Key::Num5;return true;}
    if (s=="NUM6"||s=="6"){out=sf::Keyboard::Key::Num6;return true;}
    if (s=="NUM7"||s=="7"){out=sf::Keyboard::Key::Num7;return true;}
    if (s=="NUM8"||s=="8"){out=sf::Keyboard::Key::Num8;return true;}
    if (s=="NUM9"||s=="9"){out=sf::Keyboard::Key::Num9;return true;}
    if (s=="SPACE"){out=sf::Keyboard::Key::Space;return true;}
    if (s=="LCTRL"||s=="LCONTROL"||s=="CTRL"){out=sf::Keyboard::Key::LControl;return true;}
    if (s=="LSHIFT"||s=="SHIFT"){out=sf::Keyboard::Key::LShift;return true;}
    if (s=="ESC"||s=="ESCAPE"){out=sf::Keyboard::Key::Escape;return true;}
    return false;
}

static std::filesystem::path keybindConfigPath() {
    const char* home = std::getenv("HOME");
    std::filesystem::path base = home ? home : ".";
#ifdef __APPLE__
    return base / "Library" / "Application Support" / "Aetherion" / "blackhole3d_keybinds.cfg";
#else
    return base / ".local" / "share" / "Aetherion" / "blackhole3d_keybinds.cfg";
#endif
}

static void enforceKeybindConflicts(ActionKeybinds& keys) {
    const ActionKeybinds defaults{};
    auto isMovementKey = [](sf::Keyboard::Key k) {
        return k == sf::Keyboard::Key::W  || k == sf::Keyboard::Key::A  ||
               k == sf::Keyboard::Key::S  || k == sf::Keyboard::Key::D  ||
               k == sf::Keyboard::Key::Q  || k == sf::Keyboard::Key::E  ||
               k == sf::Keyboard::Key::Space    ||
               k == sf::Keyboard::Key::LControl ||
               k == sf::Keyboard::Key::LShift;
    };
    struct Entry { const char* name; sf::Keyboard::Key* key; sf::Keyboard::Key def; };
    std::array<Entry, 16> entries = {{
        {"toggle_freelook",   &keys.toggleFreelook,   defaults.toggleFreelook},
        {"toggle_jets",       &keys.toggleJets,       defaults.toggleJets},
        {"toggle_blr",        &keys.toggleBLR,        defaults.toggleBLR},
        {"toggle_orb_body",   &keys.toggleOrbBody,    defaults.toggleOrbBody},
        {"toggle_doppler",    &keys.toggleDoppler,    defaults.toggleDoppler},
        {"toggle_blueshift",  &keys.toggleBlueshift,  defaults.toggleBlueshift},
        {"toggle_host_galaxy",&keys.toggleHostGalaxy, defaults.toggleHostGalaxy},
        {"toggle_lab",        &keys.toggleLAB,        defaults.toggleLAB},
        {"toggle_cgm",        &keys.toggleCGM,        defaults.toggleCGM},
        {"cycle_anim_speed",  &keys.cycleAnimSpeed,   defaults.cycleAnimSpeed},
        {"toggle_hud",        &keys.toggleHUD,        defaults.toggleHUD},
        {"toggle_debug_hud",  &keys.toggleDebugHUD,   defaults.toggleDebugHUD},
        {"reset_tilt",        &keys.resetTilt,        defaults.resetTilt},
        {"next_profile",      &keys.nextProfile,      defaults.nextProfile},
        {"toggle_render",     &keys.toggleRender,     defaults.toggleRender},
        {"release_mouse",     &keys.releaseMouse,     defaults.releaseMouse}
    }};
    for (auto& e : entries) {
        if (isMovementKey(*e.key)) { *e.key = e.def; }
    }
    std::unordered_map<int, const char*> seen;
    for (auto& e : entries) {
        const int code = static_cast<int>(*e.key);
        auto it = seen.find(code);
        if (it != seen.end()) { *e.key = e.def; }
        seen[static_cast<int>(*e.key)] = e.name;
    }
}

static void writeDefaultKeybindFile(const std::filesystem::path& path, const ActionKeybinds& k) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    if (!out) return;
    out << "# Aetherion 3D keybinds\n# Format: action=KEY\n\n";
    out << "toggle_freelook="   << keyToString(k.toggleFreelook)   << "\n";
    out << "toggle_jets="       << keyToString(k.toggleJets)       << "\n";
    out << "toggle_blr="        << keyToString(k.toggleBLR)        << "\n";
    out << "toggle_orb_body="   << keyToString(k.toggleOrbBody)    << "\n";
    out << "toggle_doppler="    << keyToString(k.toggleDoppler)    << "\n";
    out << "toggle_blueshift="  << keyToString(k.toggleBlueshift)  << "\n";
    out << "toggle_host_galaxy="<< keyToString(k.toggleHostGalaxy) << "\n";
    out << "toggle_lab="        << keyToString(k.toggleLAB)        << "\n";
    out << "toggle_cgm="        << keyToString(k.toggleCGM)        << "\n";
    out << "cycle_anim_speed="  << keyToString(k.cycleAnimSpeed)   << "\n";
    out << "toggle_hud="        << keyToString(k.toggleHUD)        << "\n";
    out << "toggle_debug_hud="  << keyToString(k.toggleDebugHUD)   << "\n";
    out << "reset_tilt="        << keyToString(k.resetTilt)        << "\n";
    out << "next_profile="      << keyToString(k.nextProfile)      << "\n";
    out << "toggle_render="     << keyToString(k.toggleRender)     << "\n";
    out << "release_mouse="     << keyToString(k.releaseMouse)     << "\n";
}

static ActionKeybinds loadActionKeybinds() {
    ActionKeybinds keys;
    const auto cfgPath = keybindConfigPath();
    if (!std::filesystem::exists(cfgPath)) {
        writeDefaultKeybindFile(cfgPath, keys);
        return keys;
    }
    std::ifstream in(cfgPath);
    if (!in) return keys;

    std::unordered_map<std::string, sf::Keyboard::Key*> targets = {
        {"toggle_freelook",   &keys.toggleFreelook},
        {"toggle_jets",       &keys.toggleJets},
        {"toggle_blr",        &keys.toggleBLR},
        {"toggle_orb_body",   &keys.toggleOrbBody},
        {"toggle_doppler",    &keys.toggleDoppler},
        {"toggle_blueshift",  &keys.toggleBlueshift},
        {"toggle_host_galaxy",&keys.toggleHostGalaxy},
        {"toggle_lab",        &keys.toggleLAB},
        {"toggle_cgm",        &keys.toggleCGM},
        {"cycle_anim_speed",  &keys.cycleAnimSpeed},
        {"toggle_hud",        &keys.toggleHUD},
        {"toggle_debug_hud",  &keys.toggleDebugHUD},
        {"reset_tilt",        &keys.resetTilt},
        {"next_profile",      &keys.nextProfile},
        {"toggle_render",     &keys.toggleRender},
        {"release_mouse",     &keys.releaseMouse}
    };
    std::string line;
    while (std::getline(in, line)) {
        const auto trimmed = trimCopy(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;
        const auto eqPos = trimmed.find('=');
        if (eqPos == std::string::npos) continue;
        const std::string key   = trimCopy(trimmed.substr(0, eqPos));
        const std::string value = trimCopy(trimmed.substr(eqPos + 1));
        auto it = targets.find(key);
        if (it == targets.end()) continue;
        sf::Keyboard::Key parsed{};
        if (keyFromString(value, parsed)) *it->second = parsed;
    }
    enforceKeybindConflicts(keys);
    return keys;
}

static void drawHUD(const GLBitmapFont& font, const ActionKeybinds& kb,
                    bool freelook, bool jets, bool blr, bool orbBody, bool doppler,
                    bool blueshift, bool /*fastOrbit*/, bool hostGalaxy, bool lab, bool cgm,
                    float animSpd, float fps, bool cinematic, float rollDeg,
                    const std::string& profileName, int w, int h)
{
    // Primary controls line
    std::string primary =
        "[WASD] Move  [Q/E] Tilt  [" + std::string(keyToString(kb.resetTilt)) + "] Reset Tilt"
        "  [Space/LCTRL] Up/Down  [LSHIFT] Fast  [RMB] Look\n"
        "[" + std::string(keyToString(kb.toggleFreelook)) + "] Freelook/Orbit"
        "  [" + std::string(keyToString(kb.toggleJets))    + "] Jets"
        "  [" + std::string(keyToString(kb.toggleBLR))     + "] BLR"
        "  [" + std::string(keyToString(kb.toggleOrbBody)) + "] Orb Body"
        "  [" + std::string(keyToString(kb.toggleDoppler)) + "] Doppler"
        "  [" + std::string(keyToString(kb.toggleBlueshift)) + "] Blueshift"
        "  [" + std::string(keyToString(kb.toggleRender))  + "] Cinematic\n"
        "[" + std::string(keyToString(kb.toggleHostGalaxy)) + "] Host Galaxy"
        "  [" + std::string(keyToString(kb.toggleLAB))     + "] LAB"
        "  [" + std::string(keyToString(kb.toggleCGM))     + "] CGM"
        "  [" + std::string(keyToString(kb.cycleAnimSpeed)) + "] Anim Speed"
        "  [" + std::string(keyToString(kb.nextProfile))   + "] Next Profile"
        "  [" + std::string(keyToString(kb.toggleHUD))     + "] Hide HUD"
        "  [" + std::string(keyToString(kb.releaseMouse))  + "] Release\n\n";

    primary += "Profile: " + profileName;
    primary += "  |  Render: " + std::string(cinematic ? "CINEMATIC" : "FAST");
    primary += "  |  Mode: "   + std::string(freelook  ? "Freelook"  : "Orbit");
    primary += "  |  Jets: "   + std::string(jets      ? "ON" : "OFF");
    primary += "  |  BLR: "    + std::string(blr       ? "ON" : "OFF");
    primary += "  |  Orb Body: " + std::string(orbBody ? "ON" : "OFF");
    primary += "  |  Doppler: " + std::string(doppler  ? "ON" : "OFF");
    primary += "  |  Blueshift: " + std::string(blueshift ? "ON" : "OFF");
    primary += "  |  Host Galaxy: " + std::string(hostGalaxy ? "ON" : "OFF");
    primary += "  |  LAB: "    + std::string(lab ? "ON" : "OFF");
    primary += "  |  CGM: "    + std::string(cgm ? "ON" : "OFF");
    primary += "  |  Anim: "   + std::to_string(static_cast<int>(animSpd + 0.5f)) + "x";
    primary += "  |  Tilt: "   + std::to_string(static_cast<int>(rollDeg + 0.5f)) + " deg";
    primary += "  |  FPS: "    + std::to_string(static_cast<int>(fps     + 0.5f));
    font.drawText(primary, 10.f, 10.f, 0.86f, 0.86f, 0.86f, 0.78f, w, h);

    // Debug/research key hint — dimmer, shown at the bottom of the HUD block
    const std::string debugHint =
        "[" + std::string(keyToString(kb.toggleDebugHUD)) + "] Debug overlay";
    // Approximate line height ~14px per line; primary has ~5 lines
    font.drawText(debugHint, 10.f, 10.f + 5 * 14.f + 6.f, 0.55f, 0.55f, 0.65f, 0.55f, w, h);
}

struct DebugInfo {
    bool cinematic, freelook, jets, blr, doppler;
    bool havePhotoreal, haveSimple;
    float animSpeed, fps, totalTime, animTime, rollDeg;
    int maxSteps, windowW, windowH;
    glm::vec3 camPos, camDir;
};

static void drawDebugHUD(const GLBitmapFont& font, const DebugInfo& d) {
    auto onOff = [](bool v) -> const char* { return v ? "ON" : "OFF"; };
    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        "=== DEBUG ===\nShader: %s\n  photoreal: %s  |  simple: %s\n"
        "Camera: %s\n  pos: (%.1f, %.1f, %.1f)\n  dir: (%.2f, %.2f, %.2f)\n  tilt: %.1f deg\n"
        "Render:\n  maxSteps: %d\n  cinematic: %s\n"
        "Features:\n  jets: %s\n  BLR: %s\n  doppler: %s\n"
        "Animation:\n  speed: %.0fx\n  animTime: %.1fs\n  totalTime: %.1fs\n"
        "Window: %dx%d\nFPS: %.0f",
        d.cinematic ? "PHOTOREAL" : "SIMPLE",
        d.havePhotoreal ? "compiled" : "N/A",
        d.haveSimple    ? "compiled" : "N/A",
        d.freelook ? "Freelook" : "Orbit",
        d.camPos.x, d.camPos.y, d.camPos.z,
        d.camDir.x, d.camDir.y, d.camDir.z,
        d.rollDeg, d.maxSteps, onOff(d.cinematic),
        onOff(d.jets), onOff(d.blr), onOff(d.doppler),
        d.animSpeed, d.animTime, d.totalTime,
        d.windowW, d.windowH, d.fps);
    font.drawRect(float(d.windowW) - 350.f, float(d.windowH) - 370.f,
                  340.f, 360.f, 0.f, 0.f, 0.f, 0.67f, d.windowW, d.windowH);
    font.drawText(buf, float(d.windowW) - 340.f, float(d.windowH) - 360.f,
                  0.63f, 1.0f, 0.63f, 0.86f, d.windowW, d.windowH);
}

// Initialise orbital bodies for a given profile (matches nextProfile handler in BlackHole3D.cpp)
static std::vector<OrbitalBody> makeBodies(const BlackHoleProfile& prof) {
    std::vector<OrbitalBody> bodies;
    if (prof.name == "TON 618") {
        auto base = prof.config.orbital;
        auto make = [&](float a, float e, float incl, float r, glm::vec3 col) {
            cfg::OrbitalConfig c = base;
            c.semiMajor = a; c.eccentricity = e; c.inclination = incl;
            c.bodyRadius = r; c.bodyColor = col;
            return OrbitalBody(c);
        };
        bodies.push_back(make(80.f,   0.1f,  0.0f, 1.2f, glm::vec3(1.0f, 0.9f, 0.7f)));
        bodies.push_back(make(120.f,  0.2f,  0.5f, 1.0f, glm::vec3(0.8f, 0.8f, 1.0f)));
        bodies.push_back(make(200.f,  0.05f, 0.0f, 2.5f, glm::vec3(0.9f, 0.7f, 0.5f)));
        bodies.push_back(make(150.f,  0.3f, -0.3f, 3.0f, glm::vec3(0.6f, 0.8f, 1.0f)));
        bodies.push_back(make(1500.f, 0.4f,  0.0f, 0.5f, glm::vec3(1.0f, 0.8f, 0.6f)));
        bodies.push_back(make(3000.f, 0.2f,  0.8f, 0.4f, glm::vec3(0.9f, 0.9f, 1.0f)));
        bodies.push_back(make(5000.f, 0.6f, -0.6f, 0.6f, glm::vec3(1.0f, 1.0f, 0.8f)));
    } else {
        bodies.emplace_back(prof.config.orbital);
    }
    return bodies;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────
// Sim3DState — all simulation + GL state for one widget instance
// ─────────────────────────────────────────────────────────────
struct Sim3DState {
    // Profiles and current config
    std::array<BlackHoleProfile, 4> profiles;
    int                           profileIdx = 0;
    cfg::SimConfig                config;

    // Resource discovery
    ResourceManager resources;

    // Orbital bodies
    std::vector<OrbitalBody> orbBodies;

    // GL programs and cached uniforms
    GLProgram    photorealProgram;
    GLProgram    simpleProgram;
    bool         havePhotoreal = false;
    bool         haveSimple    = false;
    SceneUniforms photorealUfs{};
    SceneUniforms simpleUfs{};
    GLProgram*    activeProgram = nullptr;
    SceneUniforms activeUfs{};
    bool          cinematicMode = false;

    // Shared geometry
    GLVertexArray quad;

    // Textures
    GLTexture2D bgTex;
    GLTexture2D photorealDiskTex;
    GLTexture2D simpleDiskTex;

    // Bloom
    BloomPipeline bloom;
    int lastW = 0, lastH = 0;

    // HUD font
    GLBitmapFont glFont;
    bool         fontLoaded = false;

    // Camera and input
    CameraController camera;
    KeyState          keys;

    // Feature toggles
    bool jetsEnabled      = false;
    bool blrEnabled       = false;
    bool orbBodyEnabled   = false;
    bool dopplerEnabled   = false;
    bool hostGalaxyEnabled= false;
    bool labEnabled       = false;
    bool cgmEnabled       = false;
    bool blueshiftEnabled = true;
    bool showHUD          = true;
    bool showDebugHUD     = false;

    // Animation
    static constexpr float animSpeedPresets[] = { 1.0f, 3.0f, 8.0f };
    static constexpr int   animSpeedCount     = 3;
    int   animSpeedIdx = 1;
    float animSpeed    = 3.0f;
    float animTime     = 0.0f;
    float totalTime    = 0.0f;

    // Mouse look
    bool  looking      = false;
    float lastMouseX   = 0.0f;
    float lastMouseY   = 0.0f;

    // FPS
    sf::Clock frameClock;
    sf::Clock fpsClock;
    int   fpsFrameCount = 0;
    float fpsDisplay    = 0.0f;

    // Keybinds
    ActionKeybinds actionKeys;

    // Persistent per-frame snapshot (avoids reallocation)
    PhysicsSnapshot snap{};

    explicit Sim3DState(const cfg::CameraConfig& camCfg)
        : camera(camCfg)
    {
        snap.orbBodyPositions.reserve(10);
        snap.orbBodyRadii.reserve(10);
        snap.orbBodyColors.reserve(10);
    }
};

// ─────────────────────────────────────────────────────────────
// Widget lifecycle
// ─────────────────────────────────────────────────────────────

Simulation3DWidget::Simulation3DWidget(QWidget *parent)
    : QSFMLCanvas(parent, QSize(1000, 700), []{sf::ContextSettings s; s.depthBits=24; s.stencilBits=8; s.majorVersion=3; s.minorVersion=3; s.attributeFlags=sf::ContextSettings::Core; return s;}())
{}

Simulation3DWidget::~Simulation3DWidget() = default;

void Simulation3DWidget::setPendingConfig(const CustomBH3DConfig &cfg)
{
    pendingConfig_    = cfg;
    hasPendingConfig_ = true;
}

void Simulation3DWidget::onInit()
{
    (void)setActive(true);
    platformInitGL();

    // ── Load profiles ──
    auto bhProfiles = profiles::allProfiles();
    const int profileIdx = 0;   // Start with TON 618
    cfg::SimConfig config = bhProfiles[profileIdx].config;

    state_ = std::make_unique<Sim3DState>(config.camera);
    auto& s = *state_;

    s.profiles   = std::move(bhProfiles);
    s.profileIdx = profileIdx;
    s.config     = config;
    s.orbBodies  = makeBodies(s.profiles[s.profileIdx]);

    // Feature flags from profile defaults
    const auto& prof = s.profiles[s.profileIdx];
    s.jetsEnabled       = prof.defaultJets;
    s.blrEnabled        = prof.defaultBLR;
    s.orbBodyEnabled    = prof.defaultOrbBody;
    s.dopplerEnabled    = prof.defaultDoppler;
    s.hostGalaxyEnabled = prof.defaultHostGalaxy;
    s.labEnabled        = prof.defaultLAB;
    s.cgmEnabled        = prof.defaultCGM;

    // ── Apply custom config if one was provided before onInit() ──
    if (hasPendingConfig_) {
        const auto& c = pendingConfig_;
        s.config.blackHole.spinParameter  = c.spinParameter;
        s.config.disk.innerRadius         = c.diskInnerRadius;
        s.config.disk.outerRadius         = c.diskOuterRadius;
        s.config.disk.halfThickness       = c.diskHalfThickness;
        s.config.disk.peakTemp            = c.diskPeakTemp;
        s.config.jet.length               = c.jetLength;
        s.jetsEnabled                     = c.jetsEnabled;
        s.blrEnabled                      = c.blrEnabled;
        s.dopplerEnabled                  = c.dopplerEnabled;
        s.hostGalaxyEnabled               = c.hostGalaxyEnabled;
        hasPendingConfig_ = false;
    }

    // ── Viewport ──
    const auto sz = getSize();
    const int w = std::max((int)sz.x, 1);
    const int h = std::max((int)sz.y, 1);
    glViewport(0, 0, w, h);

    // ── Font ──
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
            (void)setActive(true);
            s.fontLoaded = s.glFont.initFromFont(tmpFont, 14);
        }
        (void)setActive(true);
    }

    // ── Compile shaders ──
    GLShader sharedVS(GL_VERTEX_SHADER, vertexShaderSrc);
    if (!sharedVS) { std::cerr << "[3D widget] vertex shader failed\n"; return; }

    {
        std::string fragPath = s.resources.find("BlackHole3D_PhotorealDisk.frag");
        if (!fragPath.empty()) {
            std::string src = readFile(fragPath.c_str());
            if (!src.empty()) {
                GLShader fs(GL_FRAGMENT_SHADER, src.c_str());
                if (fs && s.photorealProgram.link(sharedVS.id(), fs.id())) {
                    s.havePhotoreal = true;
                    std::cerr << "[3D widget] photoreal shader compiled\n";
                }
            }
        }
    }
    {
        std::string fragPath = s.resources.find("BlackHole3D.frag");
        if (!fragPath.empty()) {
            std::string src = readFile(fragPath.c_str());
            if (!src.empty()) {
                GLShader fs(GL_FRAGMENT_SHADER, src.c_str());
                if (fs && s.simpleProgram.link(sharedVS.id(), fs.id())) {
                    s.haveSimple = true;
                    std::cerr << "[3D widget] simple shader compiled\n";
                }
            }
        }
    }

    if (!s.havePhotoreal && !s.haveSimple) {
        std::cerr << "[3D widget] FATAL: no fragment shader compiled\n";
        return;
    }

    if (s.havePhotoreal) s.photorealUfs = SceneUniforms::lookup(s.photorealProgram);
    if (s.haveSimple)    s.simpleUfs    = SceneUniforms::lookup(s.simpleProgram);

    s.cinematicMode  = s.havePhotoreal;
    s.activeProgram  = s.cinematicMode ? &s.photorealProgram : &s.simpleProgram;
    s.activeUfs      = s.cinematicMode ? s.photorealUfs      : s.simpleUfs;

    // ── Fullscreen quad ──
    s.quad = GLVertexArray::makeFullScreenQuad();

    // ── Textures ──
    {
        std::string bgPath = s.resources.find("background.png");
        if (!bgPath.empty()) s.bgTex = loadTexture2D(bgPath);
        if (!s.bgTex) s.bgTex = createFallbackWhiteTexture();
    }
    {
        s.photorealDiskTex = createFallbackWhiteTexture();
    }
    {
        std::string diskPath = s.resources.find("disk_texture.png");
        if (!diskPath.empty()) s.simpleDiskTex = loadTexture2D(diskPath);
        if (!s.simpleDiskTex) s.simpleDiskTex = createDiskTextureProcedural(512);
    }

    // ── Bloom ──
    if (!s.bloom.init(w, h))
        std::cerr << "[3D widget] bloom init failed\n";
    s.lastW = w; s.lastH = h;

    // ── Keybinds ──
    s.actionKeys = loadActionKeybinds();

    s.frameClock.restart();
    s.fpsClock.restart();
}

// ─────────────────────────────────────────────────────────────
// Render loop — mirrors BlackHole3D.cpp's main loop body.
// QSFMLCanvas::paintEvent calls onUpdate() then display().
// ─────────────────────────────────────────────────────────────
void Simulation3DWidget::onUpdate()
{
    if (!state_ || !state_->activeProgram) return;
    auto& s = *state_;

    (void)setActive(true);

    // ── Frame timing ──
    const float dt = s.frameClock.restart().asSeconds();
    s.fpsFrameCount++;
    if (s.fpsClock.getElapsedTime().asSeconds() >= 0.5f) {
        s.fpsDisplay  = float(s.fpsFrameCount) / s.fpsClock.restart().asSeconds();
        s.fpsFrameCount = 0;
    }

    // ── Physics update ──
    s.totalTime += dt;
    s.animTime  += dt * s.animSpeed;
    s.camera.update(dt, s.keys);
    for (auto& body : s.orbBodies) body.update(dt);

    const auto sz = getSize();
    const int w = std::max((int)sz.x, 1);
    const int h = std::max((int)sz.y, 1);

    // ── Build PhysicsSnapshot ──
    auto& snap = s.snap;
    snap.cameraPos    = s.camera.position();
    snap.cameraDir    = s.camera.direction();
    snap.cameraUp     = s.camera.up();
    snap.fov          = s.camera.fov();
    snap.roll         = s.camera.roll();
    snap.freelook     = s.camera.isFreelook();
    snap.totalTime    = s.totalTime;
    snap.animTime     = s.animTime;
    snap.animSpeed    = s.animSpeed;
    snap.dt           = dt;
    snap.jetsEnabled       = s.jetsEnabled;
    snap.blrEnabled        = s.blrEnabled;
    snap.dopplerEnabled    = s.dopplerEnabled;
    snap.blueshiftEnabled  = s.blueshiftEnabled;
    snap.cinematicMode     = s.cinematicMode;
    snap.bhRadius          = s.config.blackHole.radius;
    snap.bhPosition        = s.config.blackHole.position;
    snap.bhSpin            = s.config.blackHole.spinParameter;
    snap.diskInnerRadius   = s.config.disk.innerRadius;
    snap.diskOuterRadius   = s.config.disk.outerRadius;
    snap.diskHalfThickness = s.config.disk.halfThickness;
    snap.diskPeakTemp          = s.config.disk.peakTemp;
    snap.diskDisplayTempInner  = s.config.disk.displayTempInner;
    snap.diskDisplayTempOuter  = s.config.disk.displayTempOuter;
    snap.diskSatBoostInner     = s.config.disk.saturationBoostInner;
    snap.diskSatBoostOuter     = s.config.disk.saturationBoostOuter;
    snap.jetRadius         = s.config.jet.radius;
    snap.jetLength         = s.config.jet.length;
    snap.jetColor          = s.config.jet.color;
    snap.blrInnerRadius    = s.config.blr.innerRadius;
    snap.blrOuterRadius    = s.config.blr.outerRadius;
    snap.blrThickness      = s.config.blr.thickness;
    snap.orbBodyPositions.clear();
    snap.orbBodyRadii.clear();
    snap.orbBodyColors.clear();
    for (const auto& body : s.orbBodies) {
        snap.orbBodyPositions.push_back(body.position());
        snap.orbBodyRadii.push_back(body.bodyRadius());
        snap.orbBodyColors.push_back(body.bodyColor());
    }
    snap.orbBodyEnabled    = s.orbBodyEnabled;
    snap.hostGalaxyEnabled = s.hostGalaxyEnabled;
    snap.labEnabled        = s.labEnabled;
    snap.cgmEnabled        = s.cgmEnabled;
    snap.maxSteps          = s.cinematicMode ? 300 : 200;
    snap.profileName       = s.profiles[s.profileIdx].name;
    snap.windowW           = w;
    snap.windowH           = h;
    snap.fps               = s.fpsDisplay;

    // ── Resize bloom if needed ──
    if (w != s.lastW || h != s.lastH) {
        glViewport(0, 0, w, h);
        s.bloom.resize(w, h);
        s.lastW = w; s.lastH = h;
    }

    // ── Render pass ──
    if (s.cinematicMode) {
        s.bloom.sceneFBO.bind();
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        setSceneUniforms(*s.activeProgram, s.activeUfs, snap,
                         s.bgTex.id(), s.photorealDiskTex.id());
        s.quad.drawQuad();
        s.bloom.execute(s.quad, cfg::cinematicBloom(), true, s.totalTime);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w, h);
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        setSceneUniforms(*s.activeProgram, s.activeUfs, snap,
                         s.bgTex.id(), s.simpleDiskTex.id());
        s.quad.drawQuad();
    }

    // ── Ensure default framebuffer is bound before HUD / display ──
    // bloom.execute() saves+restores prevFBO, which ends up as sceneFBO (not 0).
    // If we skip the HUD block, display() would be called with sceneFBO still bound,
    // causing a stall / black frame on macOS Metal-backed GL. Always rebind FBO 0 here.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);

    // ── HUD overlay ──
    if ((s.showHUD || s.showDebugHUD) && s.fontLoaded) {
        for (int i = 0; i < 8; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const float rollDeg = snap.roll * (180.f / 3.14159265f);

        if (s.showHUD) {
            drawHUD(s.glFont, s.actionKeys,
                    snap.freelook, snap.jetsEnabled, snap.blrEnabled,
                    snap.orbBodyEnabled, snap.dopplerEnabled, snap.blueshiftEnabled,
                    false, snap.hostGalaxyEnabled, snap.labEnabled, snap.cgmEnabled,
                    snap.animSpeed, snap.fps, snap.cinematicMode,
                    rollDeg, snap.profileName, w, h);
        }
        if (s.showDebugHUD) {
            DebugInfo dbg{};
            dbg.cinematic    = snap.cinematicMode;
            dbg.freelook     = snap.freelook;
            dbg.jets         = snap.jetsEnabled;
            dbg.blr          = snap.blrEnabled;
            dbg.doppler      = snap.dopplerEnabled;
            dbg.havePhotoreal= s.havePhotoreal;
            dbg.haveSimple   = s.haveSimple;
            dbg.animSpeed    = snap.animSpeed;
            dbg.fps          = snap.fps;
            dbg.totalTime    = snap.totalTime;
            dbg.animTime     = snap.animTime;
            dbg.maxSteps     = snap.maxSteps;
            dbg.windowW      = w;
            dbg.windowH      = h;
            dbg.camPos       = snap.cameraPos;
            dbg.camDir       = snap.cameraDir;
            dbg.rollDeg      = rollDeg;
            drawDebugHUD(s.glFont, dbg);
        }
        glDisable(GL_BLEND);
    }

    while (glGetError() != GL_NO_ERROR) {}
    // paintEvent calls display() after onUpdate() — do NOT call window.display() here
}

// ─────────────────────────────────────────────────────────────
// Input — keyboard
// ─────────────────────────────────────────────────────────────

void Simulation3DWidget::onKeyPressed(sf::Keyboard::Key code)
{
    if (!state_) return;
    auto& s = *state_;

    // WASD/QE/Space/Ctrl/Shift → camera movement (KeyState tracks hold-state)
    s.keys.onKeyPressed(code);

    // Action key dispatch
    const auto& kb = s.actionKeys;
    if      (code == kb.toggleFreelook)    s.camera.toggleMode();
    else if (code == kb.toggleJets)        s.jetsEnabled       = !s.jetsEnabled;
    else if (code == kb.toggleBLR)         s.blrEnabled        = !s.blrEnabled;
    else if (code == kb.toggleOrbBody)     s.orbBodyEnabled    = !s.orbBodyEnabled;
    else if (code == kb.toggleDoppler)     s.dopplerEnabled    = !s.dopplerEnabled;
    else if (code == kb.toggleBlueshift)   s.blueshiftEnabled  = !s.blueshiftEnabled;
    else if (code == kb.toggleHostGalaxy)  s.hostGalaxyEnabled = !s.hostGalaxyEnabled;
    else if (code == kb.toggleLAB)         s.labEnabled        = !s.labEnabled;
    else if (code == kb.toggleCGM)         s.cgmEnabled        = !s.cgmEnabled;
    else if (code == kb.cycleAnimSpeed) {
        s.animSpeedIdx = (s.animSpeedIdx + 1) % Sim3DState::animSpeedCount;
        s.animSpeed    = Sim3DState::animSpeedPresets[s.animSpeedIdx];
    }
    else if (code == kb.toggleHUD)         s.showHUD      = !s.showHUD;
    else if (code == kb.toggleDebugHUD)    s.showDebugHUD = !s.showDebugHUD;
    else if (code == kb.resetTilt)         s.camera.resetRoll();
    else if (code == kb.nextProfile) {
        s.profileIdx = (s.profileIdx + 1) % (int)s.profiles.size();
        const auto& prof = s.profiles[s.profileIdx];
        s.config            = prof.config;
        s.jetsEnabled       = prof.defaultJets;
        s.blrEnabled        = prof.defaultBLR;
        s.orbBodyEnabled    = prof.defaultOrbBody;
        s.dopplerEnabled    = prof.defaultDoppler;
        s.hostGalaxyEnabled = prof.defaultHostGalaxy;
        s.labEnabled        = prof.defaultLAB;
        s.cgmEnabled        = prof.defaultCGM;
        s.camera            = CameraController(prof.config.camera);
        s.orbBodies         = makeBodies(prof);
        std::cerr << "[3D widget] profile: " << prof.name << "\n";
    }
    else if (code == kb.toggleRender) {
        if (s.havePhotoreal && s.haveSimple) {
            s.cinematicMode  = !s.cinematicMode;
            s.activeProgram  = s.cinematicMode ? &s.photorealProgram : &s.simpleProgram;
            s.activeUfs      = s.cinematicMode ? s.photorealUfs      : s.simpleUfs;
        }
    }
    else if (code == kb.releaseMouse) {
        s.looking = false;
    }
}

void Simulation3DWidget::onKeyReleased(sf::Keyboard::Key code)
{
    if (!state_) return;
    state_->keys.onKeyReleased(code);
}

// ─────────────────────────────────────────────────────────────
// Input — mouse
// ─────────────────────────────────────────────────────────────

void Simulation3DWidget::onMouseMoved(float x, float y)
{
    if (!state_) return;
    auto& s = *state_;
    if (s.looking) {
        const float dx = x - s.lastMouseX;
        const float dy = y - s.lastMouseY;
        s.camera.onMouseDelta(dx, dy);
    }
    s.lastMouseX = x;
    s.lastMouseY = y;
}

void Simulation3DWidget::onMousePressed(sf::Mouse::Button button, float x, float y)
{
    if (!state_) return;
    if (button == sf::Mouse::Button::Right) {
        state_->looking   = true;
        state_->lastMouseX = x;
        state_->lastMouseY = y;
    }
}

void Simulation3DWidget::onMouseReleased(sf::Mouse::Button button, float /*x*/, float /*y*/)
{
    if (!state_) return;
    if (button == sf::Mouse::Button::Right)
        state_->looking = false;
}

void Simulation3DWidget::onMouseWheelScrolled(float delta, float /*x*/, float /*y*/)
{
    // Zoom: forward/backward nudge along camera direction
    if (!state_) return;
    // Positive delta = scroll up = zoom in (move camera forward)
    // CameraController::update does movement via KeyState; we synthesise a brief nudge
    // by directly translating the camera via a short simulated press is complex, so
    // instead just adjust FOV slightly as a simple zoom substitute.
    (void)delta;
}

