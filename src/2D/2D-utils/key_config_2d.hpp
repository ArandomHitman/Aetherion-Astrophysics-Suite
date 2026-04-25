#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// key_config_2d.hpp  —  configurable keybind mapping for the 2D simulation.
//
// Canonical key name format (same as the 3D .cfg format):
//   Single chars uppercase: A–Z, 0–9, [  ]  ,  .  /  =  -  ;  '  `  \
//   Multi-char: SPACE ESCAPE ENTER TAB BACKSPACE DELETE INSERT HOME END
//               PAGEUP PAGEDOWN LEFT RIGHT UP DOWN NUM0–NUM9 F1–F12
// ─────────────────────────────────────────────────────────────────────────────

#include <SFML/Window/Keyboard.hpp>
#include <string>
#include <fstream>

// ── Canonical string → sf::Keyboard::Key ────────────────────────────────────

inline sf::Keyboard::Key canonicalToSFMLKey(const std::string &name)
{
    if (name.size() == 1) {
        const char c = name[0];
        if (c >= 'A' && c <= 'Z')
            return static_cast<sf::Keyboard::Key>(
                static_cast<int>(sf::Keyboard::Key::A) + (c - 'A'));
        if (c >= '0' && c <= '9')
            return static_cast<sf::Keyboard::Key>(
                static_cast<int>(sf::Keyboard::Key::Num0) + (c - '0'));
        switch (c) {
            case '[':  return sf::Keyboard::Key::LBracket;
            case ']':  return sf::Keyboard::Key::RBracket;
            case ',':  return sf::Keyboard::Key::Comma;
            case '.':  return sf::Keyboard::Key::Period;
            case '/':  return sf::Keyboard::Key::Slash;
            case '\\': return sf::Keyboard::Key::Backslash;
            case '=':  return sf::Keyboard::Key::Equal;
            case '-':  return sf::Keyboard::Key::Hyphen;
            case ';':  return sf::Keyboard::Key::Semicolon;
            case '\'': return sf::Keyboard::Key::Apostrophe;
            case '`':  return sf::Keyboard::Key::Grave;
            default:   break;
        }
    }

    // Multi-char
    if (name == "SPACE")    return sf::Keyboard::Key::Space;
    if (name == "ESCAPE")   return sf::Keyboard::Key::Escape;
    if (name == "ENTER")    return sf::Keyboard::Key::Enter;
    if (name == "TAB")      return sf::Keyboard::Key::Tab;
    if (name == "BACKSPACE")return sf::Keyboard::Key::Backspace;
    if (name == "DELETE")   return sf::Keyboard::Key::Delete;
    if (name == "INSERT")   return sf::Keyboard::Key::Insert;
    if (name == "HOME")     return sf::Keyboard::Key::Home;
    if (name == "END")      return sf::Keyboard::Key::End;
    if (name == "PAGEUP")   return sf::Keyboard::Key::PageUp;
    if (name == "PAGEDOWN") return sf::Keyboard::Key::PageDown;
    if (name == "LEFT")     return sf::Keyboard::Key::Left;
    if (name == "RIGHT")    return sf::Keyboard::Key::Right;
    if (name == "UP")       return sf::Keyboard::Key::Up;
    if (name == "DOWN")     return sf::Keyboard::Key::Down;
    // Numpad
    if (name == "NUM0")     return sf::Keyboard::Key::Numpad0;
    if (name == "NUM1")     return sf::Keyboard::Key::Numpad1;
    if (name == "NUM2")     return sf::Keyboard::Key::Numpad2;
    if (name == "NUM3")     return sf::Keyboard::Key::Numpad3;
    if (name == "NUM4")     return sf::Keyboard::Key::Numpad4;
    if (name == "NUM5")     return sf::Keyboard::Key::Numpad5;
    if (name == "NUM6")     return sf::Keyboard::Key::Numpad6;
    if (name == "NUM7")     return sf::Keyboard::Key::Numpad7;
    if (name == "NUM8")     return sf::Keyboard::Key::Numpad8;
    if (name == "NUM9")     return sf::Keyboard::Key::Numpad9;
    // Function keys F1–F12
    if (name.size() >= 2 && name[0] == 'F') {
        try {
            int n = std::stoi(name.substr(1));
            if (n >= 1 && n <= 12)
                return static_cast<sf::Keyboard::Key>(
                    static_cast<int>(sf::Keyboard::Key::F1) + (n - 1));
        } catch (...) {}
    }

    return sf::Keyboard::Key::Unknown;
}

// ── KeyConfig2D struct ───────────────────────────────────────────────────────

struct KeyConfig2D {
    // ── Simulation ──────────────────────────────────────────────────────────
    sf::Keyboard::Key pause              = sf::Keyboard::Key::Space;
    sf::Keyboard::Key togglePreset       = sf::Keyboard::Key::T;
    sf::Keyboard::Key nextPreset         = sf::Keyboard::Key::RBracket;
    sf::Keyboard::Key prevPreset         = sf::Keyboard::Key::LBracket;
    sf::Keyboard::Key reset              = sf::Keyboard::Key::R;
    // ── Orbital control ─────────────────────────────────────────────────────
    sf::Keyboard::Key zoomIn             = sf::Keyboard::Key::Up;
    sf::Keyboard::Key zoomOut            = sf::Keyboard::Key::Down;
    sf::Keyboard::Key orbitIn            = sf::Keyboard::Key::Left;
    sf::Keyboard::Key orbitOut           = sf::Keyboard::Key::Right;
    sf::Keyboard::Key eccDecrease        = sf::Keyboard::Key::E;
    sf::Keyboard::Key eccIncrease        = sf::Keyboard::Key::Q;
    // ── Visualization ───────────────────────────────────────────────────────
    sf::Keyboard::Key toggleRays         = sf::Keyboard::Key::S;
    sf::Keyboard::Key togglePhoton       = sf::Keyboard::Key::P;
    sf::Keyboard::Key toggleGalaxy       = sf::Keyboard::Key::G;
    sf::Keyboard::Key toggleInfluence    = sf::Keyboard::Key::I;
    sf::Keyboard::Key toggleHighResLensing = sf::Keyboard::Key::L;
    sf::Keyboard::Key speedUp            = sf::Keyboard::Key::Equal;
    sf::Keyboard::Key speedDown          = sf::Keyboard::Key::Hyphen;
    sf::Keyboard::Key tempDown           = sf::Keyboard::Key::Comma;
    sf::Keyboard::Key tempUp             = sf::Keyboard::Key::Period;
    // ── Research ────────────────────────────────────────────────────────────
    sf::Keyboard::Key toggleData         = sf::Keyboard::Key::D;
    sf::Keyboard::Key toggleDilation     = sf::Keyboard::Key::F;
    sf::Keyboard::Key toggleCaustics     = sf::Keyboard::Key::C;
    sf::Keyboard::Key toggleError        = sf::Keyboard::Key::N;
    sf::Keyboard::Key cycleBody          = sf::Keyboard::Key::H;
    sf::Keyboard::Key toggleControls     = sf::Keyboard::Key::Slash;
    sf::Keyboard::Key exportData         = sf::Keyboard::Key::X;   // format set by exportFormat
    std::string       exportFormat       = "CSV";                   // "CSV" | "FITS" | "Binary"
    // ── Test scenarios ──────────────────────────────────────────────────────
    sf::Keyboard::Key testIsco           = sf::Keyboard::Key::Num1;
    sf::Keyboard::Key testPhoton         = sf::Keyboard::Key::Num2;
    sf::Keyboard::Key testInfall         = sf::Keyboard::Key::Num3;
    sf::Keyboard::Key testTidal          = sf::Keyboard::Key::Num4;
    sf::Keyboard::Key testPulsar         = sf::Keyboard::Key::Num5;
};

// ── Load from .cfg file ──────────────────────────────────────────────────────

inline KeyConfig2D loadKeyConfig2D(const std::string &path)
{
    KeyConfig2D cfg;
    std::ifstream f(path);
    if (!f.is_open()) return cfg;   // return defaults

    auto trim = [](std::string s) -> std::string {
        while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\n'))
            s.pop_back();
        size_t start = s.find_first_not_of(' ');
        return (start == std::string::npos) ? "" : s.substr(start);
    };

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string action = trim(line.substr(0, eq));
        const std::string keystr = trim(line.substr(eq + 1));

        // Non-key settings handled before key parsing
        if (action == "export_format") { cfg.exportFormat = keystr; continue; }
        // Backward compat: old single-format keybinds
        if (action == "export_csv")  { cfg.exportData = sf::Keyboard::Key::X;    cfg.exportFormat = "CSV";  continue; }
        if (action == "export_fits") { cfg.exportData = sf::Keyboard::Key::X;    cfg.exportFormat = "FITS"; continue; }

        sf::Keyboard::Key k = canonicalToSFMLKey(keystr);
        if (k == sf::Keyboard::Key::Unknown) continue;

        if      (action == "pause")           cfg.pause          = k;
        else if (action == "toggle_preset")   cfg.togglePreset   = k;
        else if (action == "next_preset")     cfg.nextPreset     = k;
        else if (action == "prev_preset")     cfg.prevPreset     = k;
        else if (action == "reset")           cfg.reset          = k;
        else if (action == "zoom_in")         cfg.zoomIn         = k;
        else if (action == "zoom_out")        cfg.zoomOut        = k;
        else if (action == "orbit_in")        cfg.orbitIn        = k;
        else if (action == "orbit_out")       cfg.orbitOut       = k;
        else if (action == "ecc_decrease")    cfg.eccDecrease    = k;
        else if (action == "ecc_increase")    cfg.eccIncrease    = k;
        else if (action == "toggle_rays")     cfg.toggleRays     = k;
        else if (action == "toggle_photon")   cfg.togglePhoton   = k;
        else if (action == "toggle_galaxy")   cfg.toggleGalaxy   = k;
        else if (action == "toggle_influence")cfg.toggleInfluence= k;
        else if (action == "speed_up")        cfg.speedUp        = k;
        else if (action == "speed_down")      cfg.speedDown      = k;
        else if (action == "temp_down")       cfg.tempDown       = k;
        else if (action == "temp_up")         cfg.tempUp         = k;
        else if (action == "toggle_data")     cfg.toggleData     = k;
        else if (action == "toggle_dilation") cfg.toggleDilation = k;
        else if (action == "toggle_caustics") cfg.toggleCaustics = k;
        else if (action == "toggle_error")    cfg.toggleError    = k;
        else if (action == "cycle_body")      cfg.cycleBody      = k;
        else if (action == "toggle_controls") cfg.toggleControls = k;
        else if (action == "export_data")     cfg.exportData     = k;
        // export_format is a plain string, handled separately below
        else if (action == "test_isco")       cfg.testIsco       = k;
        else if (action == "test_photon")     cfg.testPhoton     = k;
        else if (action == "test_infall")     cfg.testInfall     = k;
        else if (action == "test_tidal")      cfg.testTidal      = k;
        else if (action == "test_pulsar")     cfg.testPulsar     = k;
    }
    return cfg;
}
