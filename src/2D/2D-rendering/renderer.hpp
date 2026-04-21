#pragma once
#include <SFML/Graphics.hpp>
#include "../2D-core/body_visual.hpp"
#include <string>
#include <iostream>
#include <cmath>
#include <cstdint>
#include <algorithm>

// Renderer — keeps it dumb.  NEVER computes physics.
// All visual data is pre-computed by the visualization layer.
class Renderer {
    sf::RenderWindow& window_;
    sf::CircleShape   horizonShape_;
    sf::CircleShape   photonRing_;
    sf::Font          font_;
    sf::Text          infoText_;
    sf::Texture       diskTexture_;
    sf::Sprite        diskSprite_;
    bool              diskTextureLoaded_;

public:
    explicit Renderer(sf::RenderWindow& win)
        : window_(win)
        , horizonShape_(10.0f)
        , photonRing_(15.0f)
        , infoText_(font_, "", 14)
        , diskSprite_(diskTexture_)
        , diskTextureLoaded_(false)
    {
        // Pre-check existence before calling openFromFile so SFML never prints
        // a "Failed to load font" error for paths that simply don't exist on
        // the current platform (e.g. Linux paths on macOS and vice versa).
        // Flatpak runtime: /usr/share/fonts/dejavu/  (no truetype/ subdir)
        // Host system:     /usr/share/fonts/truetype/dejavu/
        // Host via flatpak bridge: /run/host/fonts/truetype/dejavu/
        auto tryFont = [&](const std::string& path) -> bool {
            std::ifstream probe(path);
            return probe.good() && font_.openFromFile(path);
        };
        bool fontLoaded =
            tryFont("/usr/share/fonts/dejavu/DejaVuSans.ttf")                       ||
            tryFont("/usr/share/fonts/liberation-fonts/LiberationSans-Regular.ttf") ||
            tryFont("/run/host/fonts/truetype/dejavu/DejaVuSans.ttf")               ||
            tryFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")              ||
            tryFont("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf") ||
            tryFont("/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf")               ||
            tryFont("/System/Library/Fonts/Helvetica.ttc");
        if (!fontLoaded)
            std::cerr << "Warning: could not load system font; text may not display.\n";
        infoText_ = sf::Text(font_, "", 14);
        infoText_.setFillColor(sf::Color::White);
        infoText_.setPosition({8.0f, 8.0f});

        // Build a list of candidate paths for the disk texture,
        // covering: dev builds (relative), flatpak (/app/share), XDG installs.
        // Pre-check existence with ifstream so SFML doesn't log spurious errors.
        auto tryDiskTexture = [&](const std::string& p) {
            if (!diskTextureLoaded_) {
                std::ifstream probe(p);
                if (probe.good())
                    diskTextureLoaded_ = diskTexture_.loadFromFile(p);
            }
        };
        tryDiskTexture("disk_texture.png");
        tryDiskTexture("src/disk_texture.png");
        tryDiskTexture("../src/disk_texture.png");
        tryDiskTexture("/app/share/aetherionsuite/disk_texture.png");
        tryDiskTexture("/usr/local/share/aetherionsuite/disk_texture.png");
        tryDiskTexture("/usr/share/aetherionsuite/disk_texture.png");
        // Also walk XDG_DATA_DIRS (set by Flatpak and system installs)
        if (!diskTextureLoaded_) {
            const char* xdg = std::getenv("XDG_DATA_DIRS");
            if (xdg && xdg[0]) {
                std::string dirs(xdg);
                std::string::size_type start = 0, end;
                while (!diskTextureLoaded_ && (end = dirs.find(':', start)) != std::string::npos) {
                    tryDiskTexture(dirs.substr(start, end - start) + "/aetherionsuite/disk_texture.png");
                    start = end + 1;
                }
                if (!diskTextureLoaded_)
                    tryDiskTexture(dirs.substr(start) + "/aetherionsuite/disk_texture.png");
            }
        }
        if (diskTextureLoaded_) {
            diskSprite_ = sf::Sprite(diskTexture_);
            diskSprite_.setOrigin(sf::Vector2f(
                diskTexture_.getSize().x / 2.f,
                diskTexture_.getSize().y / 2.f));
        }

        horizonShape_.setFillColor(sf::Color::Black);
        horizonShape_.setOutlineThickness(0.0f);

        photonRing_.setFillColor(sf::Color::Transparent);
        photonRing_.setOutlineThickness(2.0f);
        photonRing_.setOutlineColor(sf::Color(180, 180, 255, 200));
    }

    /*--------- Frame lifecycle ---------*/
    void beginFrame() { window_.clear(sf::Color(10, 10, 30)); }
    void endFrame()   { window_.display(); }

    /*--------- Background ---------*/
    void drawStarfield() {
        auto winSz = window_.getSize();
        for (int i = 0; i < 200; ++i) {
            float sx = std::fmod(i * 37.1234f + 300.5f, (float)winSz.x);
            float sy = std::fmod(i * 71.4321f + 100.5f, (float)winSz.y);
            sf::CircleShape s(1.0f);
            s.setPosition({sx, sy});
            s.setFillColor(sf::Color(220, 220, 255, 60));
            window_.draw(s);
        }
    }

    /*--------- Black hole visuals (pre-computed pixel values) ---------*/
    void drawHorizon(sf::Vector2f center, float radiusPx) {
        horizonShape_.setRadius(radiusPx);
        horizonShape_.setOrigin({radiusPx, radiusPx});
        horizonShape_.setPosition(center);
        window_.draw(horizonShape_);
    }

    void drawPhotonSphere(sf::Vector2f center, float radiusPx) {
        photonRing_.setRadius(radiusPx);
        photonRing_.setOrigin({radiusPx, radiusPx});
        photonRing_.setPosition(center);
        window_.draw(photonRing_);
    }

    void drawAccretionDisk(sf::Vector2f center, float radiusPx) {
        if (!diskTextureLoaded_) return;
        float texRadius = diskTexture_.getSize().x / 2.0f;
        float scale = radiusPx / texRadius;
        diskSprite_.setScale({scale, scale});
        diskSprite_.setPosition(center);
        window_.draw(diskSprite_);
    }

    /*--------- Rays (vertices pre-computed by RayVisualizer) ---------*/
    void drawRayPath(const std::vector<sf::Vertex>& verts) {
        if (verts.size() >= 2)
            window_.draw(verts.data(), verts.size(), sf::PrimitiveType::LineStrip);
    }

    void drawCapturedRay(const sf::Vertex& v0, const sf::Vertex& v1) {
        sf::Vertex line[] = { v0, v1 };
        window_.draw(line, 2, sf::PrimitiveType::Lines);
    }

    /*--------- Bodies (BodyVisual pre-computed by OrbitVisualizer) ---------*/
    void drawBody(const BodyVisual& vis) {
        sf::RectangleShape rect(sf::Vector2f(vis.width, vis.height));
        rect.setOrigin(rect.getSize() * 0.5f);
        rect.setPosition(vis.screenPos);
        rect.setRotation(sf::degrees(vis.angleDeg));
        rect.setFillColor(vis.color);
        window_.draw(rect);
    }

    // ── Orbit path (VertexArray pre-computed by OrbitVisualizer)
    void drawOrbitPath(const sf::VertexArray& path) {
        if (path.getVertexCount() > 1) window_.draw(path);
    }

    /*--------- Influence zones (labeled radial rings) ---------*/
    void drawInfluenceZone(sf::Vector2f center, float radiusPx,
                           sf::Color color, const std::string& label) {
        // Dashed-style ring
        sf::CircleShape ring(radiusPx);
        ring.setOrigin({radiusPx, radiusPx});
        ring.setPosition(center);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineThickness(1.5f);
        ring.setOutlineColor(color);
        window_.draw(ring);

        // Label at top of ring
        sf::Text lbl(font_, label, 11);
        lbl.setFillColor(color);
        lbl.setPosition({center.x - (float)label.size() * 3.0f,
                         center.y - radiusPx - 14.0f});
        window_.draw(lbl);
    }

    /*--------- Relativistic jet cones ---------*/
    void drawJetCones(sf::Vector2f center, float lengthPx) {
        // Upward jet
        sf::ConvexShape jetUp;
        jetUp.setPointCount(3);
        jetUp.setPoint(0, {center.x - 4.0f, center.y});
        jetUp.setPoint(1, {center.x + 4.0f, center.y});
        jetUp.setPoint(2, {center.x, center.y - lengthPx});
        jetUp.setFillColor(sf::Color(120, 180, 255, 50));
        jetUp.setOutlineThickness(1.0f);
        jetUp.setOutlineColor(sf::Color(150, 200, 255, 100));
        window_.draw(jetUp);

        // Downward jet
        sf::ConvexShape jetDown;
        jetDown.setPointCount(3);
        jetDown.setPoint(0, {center.x - 4.0f, center.y});
        jetDown.setPoint(1, {center.x + 4.0f, center.y});
        jetDown.setPoint(2, {center.x, center.y + lengthPx});
        jetDown.setFillColor(sf::Color(120, 180, 255, 50));
        jetDown.setOutlineThickness(1.0f);
        jetDown.setOutlineColor(sf::Color(150, 200, 255, 100));
        window_.draw(jetDown);
    }

    /*--------- Body label text ---------*/
    void drawBodyLabel(sf::Vector2f screenPos, const std::string& text, sf::Color color) {
        sf::Text lbl(font_, text, 10);
        lbl.setFillColor(color);
        lbl.setPosition({screenPos.x + 8.0f, screenPos.y - 6.0f});
        window_.draw(lbl);
    }

    /*--------- HUD ---------*/
    void drawHUD(const std::string& text) {
        infoText_.setString(text);
        window_.draw(infoText_);
    }

    /*--------- Data panel (right side, uses view coords) ---------*/
    void drawDataPanel(const std::string& text, float viewW, float viewH) {
        float panelW = 260.0f;
        float panelX = viewW - panelW - 4.0f;

        sf::RectangleShape bg(sf::Vector2f(panelW, viewH - 8.0f));
        bg.setPosition({panelX, 4.0f});
        bg.setFillColor(sf::Color(0, 0, 0, 180));
        bg.setOutlineThickness(1.0f);
        bg.setOutlineColor(sf::Color(80, 120, 200, 150));
        window_.draw(bg);

        sf::Text panelText(font_, text, 11);
        panelText.setFillColor(sf::Color(200, 220, 255));
        panelText.setPosition({panelX + 6.0f, 8.0f});
        window_.draw(panelText);
    }

    /*--------- Controls panel (left side, toggleable) ---------*/
    void drawControlsPanel(float viewH) {
        float panelW = 280.0f;
        float panelX = 4.0f;
        float panelY = 4.0f;

        sf::RectangleShape bg(sf::Vector2f(panelW, viewH - 8.0f));
        bg.setPosition({panelX, panelY});
        bg.setFillColor(sf::Color(0, 0, 0, 190));
        bg.setOutlineThickness(1.0f);
        bg.setOutlineColor(sf::Color(120, 140, 180, 150));
        window_.draw(bg);

        // Header
        sf::Text header(font_, "CONTROLS  (? to hide)", 13);
        header.setFillColor(sf::Color(255, 220, 120));
        header.setPosition({panelX + 8.0f, panelY + 6.0f});
        window_.draw(header);

        // Primary controls
        const char* primaryLines[] = {
            "",
            "--- General ---",
            "Space     Pause / resume",
            "R         Reset simulation",
            "- / =     Slow down / speed up",
            "?         Toggle this panel",
            "",
            "--- View ---",
            "Up/Down   Zoom (preset) / Mass",
            "S         Toggle ray display",
            "P         Toggle photon sphere",
            "I         Toggle influence zones",
            "",
            "--- Orbit ---",
            "Left/Right  Semi-major axis",
            "Q / E       Eccentricity +/-",
            "",
            "--- Presets ---",
            "T         Toggle preset mode",
            "[ / ]     Cycle presets",
            "G         Toggle galaxy system",
        };
        constexpr int nPrimary = sizeof(primaryLines) / sizeof(primaryLines[0]);

        float y = panelY + 26.0f;
        for (int i = 0; i < nPrimary; ++i) {
            const char* line = primaryLines[i];
            bool isSectionHeader = (line[0] == '-');
            sf::Text lt(font_, line, 11);
            lt.setFillColor(isSectionHeader
                ? sf::Color(160, 200, 255, 220)
                : sf::Color(200, 200, 210, 200));
            lt.setPosition({panelX + 10.0f, y});
            window_.draw(lt);
            y += isSectionHeader ? 16.0f : 14.0f;
        }

        // Divider before advanced section
        y += 4.0f;
        sf::RectangleShape divider(sf::Vector2f(panelW - 20.0f, 1.0f));
        divider.setPosition({panelX + 10.0f, y});
        divider.setFillColor(sf::Color(180, 120, 60, 140));
        window_.draw(divider);
        y += 5.0f;

        sf::Text advHdr(font_, "--- Research / Debug ---", 11);
        advHdr.setFillColor(sf::Color(220, 160, 80, 220));
        advHdr.setPosition({panelX + 10.0f, y});
        window_.draw(advHdr);
        y += 16.0f;

        const char* advLines[] = {
            "F         Time-dilation map",
            "C         Caustic highlights",
            "D         Data panel",
            "H         Cycle selected body",
            "N         Numerical error display",
            "X         Export CSV data",
            ", / .     Gas temp down/up",
            "",
            "--- Scenarios ---",
            "1         ISCO validation test",
            "2         Photon sphere test",
            "3         Radial infall test",
            "4         Tidal disruption demo",
            "5         Pulsar orbital sim",
        };
        constexpr int nAdv = sizeof(advLines) / sizeof(advLines[0]);
        for (int i = 0; i < nAdv; ++i) {
            const char* line = advLines[i];
            bool isSectionHeader = (line[0] == '-');
            sf::Text lt(font_, line, 11);
            lt.setFillColor(isSectionHeader
                ? sf::Color(160, 200, 255, 200)
                : sf::Color(170, 170, 185, 180));
            lt.setPosition({panelX + 10.0f, y});
            window_.draw(lt);
            y += isSectionHeader ? 16.0f : 14.0f;
        }
    }

    /*--------- Pulsar body (neutron star with dynamic jets, field lines, LC ring) ---------*/
    // spinPhase_rad  — accumulated spin rotation (drives jet direction + field line rotation)
    // precPhase_rad  — accumulated geodetic precession (slowly tilts the spin axis)
    // lcRadius_px    — light cylinder radius in screen pixels (<=0 = don't draw)
    // inLC           — orbital radius is inside the light cylinder
    // bhPos          — BH screen position (for unipolar flux tube visualization)
    // magPower_ergs  — unipolar inductor power; controls flux tube brightness
    void drawPulsarBody(sf::Vector2f screenPos,
                        float spinPhase_rad,
                        float precPhase_rad,
                        float lcRadius_px,
                        bool  inLC,
                        sf::Vector2f bhPos,
                        double magPower_ergs) {

        constexpr float NS_R       = 9.0f;   // NS core radius (px) — bright, unmistakable
        constexpr float JET_LEN    = 110.0f; // jet half-length (px)
        constexpr float JET_BASE   = 9.0f;   // jet half-width at NS surface (px)
        constexpr int   N_ARC_PTS  = 20;     // arc segment count for field lines
        constexpr float PI         = 3.14159265f;

        // Jet direction: fast spin phase + slow geodetic precession tilt.
        // spinPhase rotates the jet rapidly (tied to spin period);
        // precPhase drifts the spin-axis orientation (geodetic precession).
        float jetRad = spinPhase_rad + precPhase_rad;
        float jcos   = std::cos(jetRad);
        float jsin   = std::sin(jetRad);

        // ── NS core — bright cyan disc so the pulsar is unmistakably distinct ──
        float coreR = inLC ? NS_R + 2.0f : NS_R;
        // Outer soft glow halo
        float haloR = coreR + 6.0f;
        sf::CircleShape halo(haloR);
        halo.setOrigin({haloR, haloR});
        halo.setPosition(screenPos);
        halo.setFillColor(sf::Color(80, 200, 255, 55));
        halo.setOutlineThickness(0.0f);
        window_.draw(halo);

        sf::Color coreCol = inLC ? sf::Color(255, 255, 255, 255)
                                 : sf::Color(200, 245, 255, 255);
        sf::Color rimCol  = inLC ? sf::Color(0, 230, 255, 255)
                                 : sf::Color(0, 200, 255, 230);
        sf::CircleShape core(coreR);
        core.setOrigin({coreR, coreR});
        core.setPosition(screenPos);
        core.setFillColor(coreCol);
        core.setOutlineThickness(2.5f);
        core.setOutlineColor(rimCol);
        window_.draw(core);

        // ── Relativistic jets ──────────────────────────────────────────────────
        // Two bright tapered triangles fired from the magnetic poles, rotating
        // with spinPhase. Uses explicit setPointCount() for SFML 3.x compatibility.
        for (int s = 0; s < 2; ++s) {
            float sx     = (s == 0) ? 1.0f : -1.0f;
            float base_x = screenPos.x + sx * coreR * jcos;
            float base_y = screenPos.y - sx * coreR * jsin;
            float tip_x  = screenPos.x + sx * JET_LEN * jcos;
            float tip_y  = screenPos.y - sx * JET_LEN * jsin;

            // perpendicular to jet axis (Y-axis is down in screen space)
            float px =  jsin * JET_BASE;
            float py =  jcos * JET_BASE;

            // Bright filled triangle — white-cyan, high alpha
            sf::ConvexShape jet;
            jet.setPointCount(3);
            jet.setPoint(0, {tip_x,           tip_y          });
            jet.setPoint(1, {base_x + px, base_y + py});
            jet.setPoint(2, {base_x - px, base_y - py});
            jet.setFillColor(sf::Color(180, 230, 255, 210));
            jet.setOutlineThickness(1.0f);
            jet.setOutlineColor(sf::Color(0, 210, 255, 200));
            window_.draw(jet);

            // Hot spine: bright white near NS, fading to blue at tip
            sf::Vertex spine[2];
            spine[0].position = {base_x, base_y};
            spine[0].color    = sf::Color(255, 255, 255, 240);
            spine[1].position = {tip_x,  tip_y};
            spine[1].color    = sf::Color(40, 140, 255,  60);
            window_.draw(spine, 2, sf::PrimitiveType::Lines);
        }

        // ── Dipole magnetic field arcs ─────────────────────────────────────────
        // Four dipole loops (each split into two half-arcs) that rotate with the
        // spin phase, showing the sweeping co-rotating magnetic dipole.
        // Arcs are gold/amber to contrast sharply with the blue jet and background.
        for (int loop = 0; loop < 4; ++loop) {
            float loopOffset = loop * (PI / 4.0f);
            float arcBaseRad = jetRad + loopOffset;

            // Inner loops tighter, outer loops wider — gives a 3-D layered feel.
            float arcR = 22.0f + loop * 8.0f;
            uint8_t arcAlpha = (uint8_t)(200 - loop * 30);  // 200, 170, 140, 110

            // Alternate gold and cyan so overlapping arcs remain distinguishable.
            sf::Color arcCol = (loop % 2 == 0)
                ? sf::Color(255, 200,  60, arcAlpha)   // gold
                : sf::Color( 60, 200, 255, arcAlpha);  // cyan

            for (int hemi = 0; hemi < 2; ++hemi) {
                std::vector<sf::Vertex> pts;
                pts.reserve(N_ARC_PTS + 1);

                for (int j = 0; j <= N_ARC_PTS; ++j) {
                    float t      = (float)j / (float)N_ARC_PTS;
                    float lambda = t * PI;
                    float dr     = arcR * std::sin(lambda);   // dipole bulge
                    float da     = arcBaseRad
                                 + (hemi == 0 ? 0.0f : PI)
                                 + (t - 0.5f) * PI;           // sweep from pole→eq→pole

                    sf::Vertex v;
                    v.position = {
                        screenPos.x + dr * std::cos(da),
                        screenPos.y - dr * std::sin(da)
                    };
                    // Fade at tips (where dr≈0) to look like field lines closing at poles
                    float fade = std::sin(t * PI);
                    v.color = sf::Color(arcCol.r, arcCol.g, arcCol.b,
                                        (uint8_t)(arcAlpha * fade));
                    pts.push_back(v);
                }
                window_.draw(pts.data(), pts.size(), sf::PrimitiveType::LineStrip);
            }
        }

        // ── Light cylinder ring ────────────────────────────────────────────────
        // Drawn as a pulsing dashed circle around the NS at screen radius lcRadius_px.
        if (lcRadius_px > 50.0f && lcRadius_px < 2800.0f) {
            // Animated pulse using a class-level counter (static is fine for single NS).
            static float lcPulsePhase = 0.0f;
            lcPulsePhase += 0.025f;
            float pulseAlpha = 30.0f + 22.0f * std::sin(lcPulsePhase);
            uint8_t aLC = (uint8_t)std::clamp(pulseAlpha, 8.0f, 80.0f);
            sf::Color lcCol(100, 170, 255, aLC);

            // Draw as a thin circle outline.
            sf::CircleShape lc(lcRadius_px);
            lc.setOrigin({lcRadius_px, lcRadius_px});
            lc.setPosition(screenPos);
            lc.setFillColor(sf::Color::Transparent);
            lc.setOutlineThickness(inLC ? 1.5f : 1.0f);
            lc.setOutlineColor(lcCol);
            window_.draw(lc);

            // Small "R_LC" label just outside the ring.
            sf::Text lcLabel(font_, "R\u2113\u1d84", 8);
            lcLabel.setFillColor(sf::Color(120, 180, 255, 110));
            lcLabel.setPosition({screenPos.x + lcRadius_px + 3.0f, screenPos.y - 8.0f});
            window_.draw(lcLabel);
        }

        // ── Unipolar flux tubes: field lines from NS magnetic poles to the BH ──
        // Represents the magnetic coupling between the NS magnetosphere and the BH
        // (Goldreich-Lynden-Bell unipolar inductor mechanism).
        // Two tubes — one from each magnetic pole — arc toward the BH.
        // Brightness scales logarithmically with magnetic power.
        if (magPower_ergs > 1.0e20) {
            // log10(P) mapped from ~20 (floor) to ~40 (bright compact binary)
            float brightness = std::clamp(
                (float)(std::log10(magPower_ergs) - 20.0) / 20.0f, 0.0f, 1.0f);
            uint8_t tubeAlpha = (uint8_t)(20 + 100 * brightness);

            sf::Vector2f ns2bh = bhPos - screenPos;
            float dist = std::sqrt(ns2bh.x * ns2bh.x + ns2bh.y * ns2bh.y);

            // Perpendicular direction (for arc bulge).
            sf::Vector2f perp(0.0f, 0.0f);
            if (dist > 1e-3f) perp = sf::Vector2f(-ns2bh.y / dist, ns2bh.x / dist);

            // Draw two tubes, one from each pole.  The connection point on the NS
            // rotates with the spin phase so tubes appear to sweep as the NS spins.
            for (int pole = 0; pole < 2; ++pole) {
                float poleAngle = jetRad + pole * PI;   // each pole is 180° apart
                // Start point: slightly off the NS surface at the pole position.
                sf::Vector2f start = screenPos
                    + sf::Vector2f(std::cos(poleAngle), -std::sin(poleAngle)) * coreR;

                // Control point for quadratic Bézier: bulge perpendicular to NS-BH line.
                float bulge = dist * 0.25f * std::sin(spinPhase_rad + pole * PI);
                sf::Vector2f ctrl = screenPos + ns2bh * 0.5f + perp * bulge;

                // End point: BH horizon surface.
                sf::Vector2f toBH = (dist > 1e-3f) ? ns2bh / dist : sf::Vector2f(1.0f, 0.0f);
                float bhR = std::max(6.0f, dist * 0.05f);  // crude horizon radius estimate
                sf::Vector2f end = bhPos - toBH * bhR;

                // Quadratic Bézier: P(t) = (1-t)² P0 + 2(1-t)t P1 + t² P2
                const int N_TUBE = 14;
                std::vector<sf::Vertex> pts;
                pts.reserve(N_TUBE + 1);
                for (int k = 0; k <= N_TUBE; ++k) {
                    float t  = (float)k / (float)N_TUBE;
                    float mt = 1.0f - t;
                    sf::Vector2f p = mt * mt * start + 2.0f * mt * t * ctrl + t * t * end;
                    // Alpha: brighter in the middle of the arc.
                    uint8_t alpha = (uint8_t)(tubeAlpha * std::sin(t * PI));
                    sf::Vertex v;
                    v.position = p;
                    // Colour: cyan-blue, slightly more orange near the BH (hot end).
                    uint8_t r_ch = (uint8_t)(80 + (uint8_t)(60 * t));
                    v.color = sf::Color(r_ch, 160, (uint8_t)(255 - 60 * t), alpha);
                    pts.push_back(v);
                }
                window_.draw(pts.data(), pts.size(), sf::PrimitiveType::LineStrip);
            }

            // Small glow at the BH endpoint (flux tube attachment to horizon).
            float glowR = 4.0f + 3.0f * brightness;
            sf::CircleShape glowDot(glowR);
            glowDot.setOrigin({glowR, glowR});
            glowDot.setPosition(bhPos);
            glowDot.setFillColor(sf::Color(160, 200, 255, tubeAlpha / 2));
            glowDot.setOutlineThickness(0.0f);
            window_.draw(glowDot);
        }
    }

    /*--------- Time dilation heatmap (concentric rings) ---------*/
    void drawTimeDilationMap(sf::Vector2f center, double M, double pixelsPerM) {
        const int nRings = 30;
        double rMax = 20.0 * M;
        double rMin = 2.1 * M;

        for (int i = 0; i < nRings; ++i) {
            double t = (double)i / (double)nRings;
            double r = rMax * (1.0 - t) + rMin * t;

            double fr = 1.0 - 2.0 * M / r;
            if (fr <= 0.0) continue;
            double gamma = 1.0 / std::sqrt(fr);

            // Map gamma [1, ~4] to color: blue → yellow → red
            double norm = std::min((gamma - 1.0) / 3.0, 1.0);
            uint8_t rc = (uint8_t)(norm * 255);
            uint8_t gc = (uint8_t)(std::max(0.0, 1.0 - std::abs(norm - 0.4) * 3.0) * 200);
            uint8_t bc = (uint8_t)((1.0 - norm) * 200);

            float radiusPx = (float)(r * pixelsPerM);
            sf::CircleShape ring(radiusPx);
            ring.setOrigin({radiusPx, radiusPx});
            ring.setPosition(center);
            ring.setFillColor(sf::Color(rc, gc, bc, 25));
            ring.setOutlineThickness(0.0f);
            window_.draw(ring);
        }

        // Labels at key radii
        auto drawLabel = [&](double r, const std::string& lbl) {
            float px = (float)(r * pixelsPerM);
            sf::Text t(font_, lbl, 9);
            t.setFillColor(sf::Color(200, 200, 255, 160));
            t.setPosition({center.x + px + 3.0f, center.y - 8.0f});
            window_.draw(t);
        };
        drawLabel(6.0 * M, "ISCO 6M");
        drawLabel(3.0 * M, "Photon 3M");
        drawLabel(2.0 * M, "Horizon 2M");
    }

    /*--------- Caustic point highlighting ---------*/
    void drawCausticMarker(sf::Vector2f pos, float intensity) {
        float radius = 3.0f + intensity * 2.0f;
        sf::CircleShape marker(radius);
        marker.setOrigin({radius, radius});
        marker.setPosition(pos);
        uint8_t alpha = (uint8_t)std::min(255.0f, 80.0f + intensity * 40.0f);
        marker.setFillColor(sf::Color(255, 255, 100, alpha));
        window_.draw(marker);
    }

    /*--------- Export status notification ---------*/
    void drawNotification(const std::string& text, float viewW, float viewH) {
        sf::Text notif(font_, text, 14);
        notif.setFillColor(sf::Color(100, 255, 100));
        notif.setPosition({viewW * 0.3f, viewH - 30.0f});
        window_.draw(notif);
    }

    /*--------- Reset view after window resize ---------*/
    void resetView(float viewW, float viewH) {
        sf::View view(sf::FloatRect({0.f, 0.f}, {viewW, viewH}));
        window_.setView(view);
    }
};
