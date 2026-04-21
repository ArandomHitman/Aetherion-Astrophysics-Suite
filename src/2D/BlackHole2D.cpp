/*--------- BlackHole2D.cpp ---------*/
// Author: Liam N.
// Purpose: The main file for the 2D, more research & data-collection oriented half of the Aetherion Astrophysics Suite
// Status: In Beta development.


/*
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠀⢠⢀⡐⢄⢢⡐⢢⢁⠂⠄⠠⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠀⠀⠀⠀⠀⠀⡄⣌⠰⣘⣆⢧⡜⣮⣱⣎⠷⣌⡞⣌⡒⠤⣈⠠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠒⠊⠀⠀⠀⠀⢀⠢⠱⡜⣞⣳⠝⣘⣭⣼⣾⣷⣶⣶⣮⣬⣥⣙⠲⢡⢂⠡⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠃⠀⠀⠀⠀⠀⠀⢀⠢⣑⢣⠝⣪⣵⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣯⣻⢦⣍⠢⢅⢂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠀⠀⢆⡱⠌⣡⢞⣵⣿⣿⣿⠿⠛⠛⠉⠉⠛⠛⠿⢷⣽⣻⣦⣎⢳⣌⠆⡱⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⠂⠠⠌⢢⢃⡾⣱⣿⢿⡾⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⢻⣏⠻⣷⣬⡳⣤⡂⠜⢠⡀⣀⠀⠀⡀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⢀⠂⣌⢃⡾⢡⣿⢣⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢹⡇⡊⣿⣿⣾⣽⣛⠶⣶⣬⣭⣥⣙⣚⢷⣶⠦⡤⢀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⢁⠂⠰⡌⡼⠡⣼⢃⡿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⣿⣿⣿⣿⣿⣿⣿⣿⣾⡿⠿⣛⣯⡴⢏⠳⠁⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠠⠑⡌⠀⣉⣾⣩⣼⣿⣾⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣀⣠⣤⣤⣿⣿⣿⣿⡿⢛⣛⣯⣭⠶⣞⠻⣉⠒⠀⠂⠀⠀⠀
⠀⠀⠀⠀⠀⠀⢀⣀⡶⢝⣢⣾⣿⣼⣿⣿⣿⣿⣿⣀⣼⣀⣀⣀⣤⣴⣶⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⠿⣿⠿⡛⠏⠍⠂⠁⢠⠁⠀⠀⠀⠀⠀⠀⠀
⠀⠠⢀⢥⣰⣾⣿⣯⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠟⣽⠟⣿⠐⠨⠑⡀⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⡐⢢⣟⣾⣿⣿⣟⣛⣿⣿⣿⣿⢿⣝⠻⠿⢿⣯⣛⢿⣿⣿⣿⡛⠻⠿⣛⠻⠛⡛⠩⢁⣴⡾⢃⣾⠇⢀⠡⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠈⠁⠊⠙⠉⠩⠌⠉⠢⠉⠐⠈⠂⠈⠁⠉⠂⠐⠉⣻⣷⣭⠛⠿⣶⣦⣤⣤⣴⣴⡾⠟⣫⣾⣿⡏⠀⠂⠐⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠙⢻⢿⢶⣤⣬⣉⣉⣭⣤⣴⣿⣿⡿⠃⠄⡈⠁⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⠘⢊⠳⠭⡽⣿⠿⠿⠟⠛⠉⠀⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠀⠁⠈⠐⠀⠘⠀⠈⠀⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀

I know it has absolutely nothing to do with the program, but I just want to leave this beautiful art here <3 */
/*--------- Headers ---------*/
#include <SFML/Graphics.hpp>
#include <sstream>
#include <iomanip>
#include <cmath>

#include "2D-physics/units.hpp"
#include "2D-simulation/simulation.hpp"
#include "2D-rendering/camera.hpp"
#include "2D-rendering/renderer.hpp"
#include "2D-visualization/ray_visualizer.hpp"
#include "2D-visualization/orbit_visualizer.hpp"
#include "2D-ui/controls.hpp"
#include "2D-utils/presets_2d.hpp"
#include <cstring>

/*--------- HUD text builder ---------*/
// this function is doing a lot of string formatting inline. I know it should probably be a separate
// class or at least split into smaller functions but every time I start refactoring it I end up
// breaking something subtle in the formatting, so it stays as one big function. for now.
static std::string buildHUD(const Simulation& sim, const UIState& ui) {
    auto pretty = [](double v) -> std::string {
        std::ostringstream os;
        if (std::fabs(v) >= 1e6 || (std::fabs(v) > 0 && std::fabs(v) < 1e-3))
            os << std::scientific << std::setprecision(3) << v;
        else
            os << std::fixed << std::setprecision(4) << v;
        return os.str();
    };

    std::ostringstream ss;
    if (ui.presetActive) {
        double M = sim.bh.metric.M;
        double r_h_m = sim.bh.metric.horizon();
        double r_h_km = r_h_m / 1000.0;
        double r_h_AU = r_h_m / 1.495978707e11;
        double r_h_light_days = r_h_m / (units::c_SI * 86400.0);

        const auto& preset = BH2D_PRESETS[ui.presetIdx];
        ss << "Preset: " << preset.name
           << " (" << pretty(preset.massSolar) << " Msun)\n";
        ss << preset.description << "\n";
        ss << "M (geom) = " << pretty(M) << " m\n";
        ss << "Event horizon r_h = " << pretty(r_h_m) << " m ("
           << pretty(r_h_km) << " km, " << pretty(r_h_AU) << " AU, "
           << pretty(r_h_light_days) << " light-days)\n";
        ss << "Display scale: horizon ~ "
           << static_cast<int>(2.0 * M * sim.params.pixelsPerM + 0.5)
           << " px (pixelsPerMeter=" << std::scientific << std::setprecision(3)
           << sim.params.pixelsPerM << ")\n";

        // Galaxy system body summary
        if (sim.galaxySystemActive) {
            int nStars = 0, nGas = 0, nCluster = 0, nDwarf = 0;
            for (const auto& b : sim.bodies) {
                if (!b.isGalaxyBody) continue;
                switch (b.bodyType) {
                    case GalaxyBodyType::Star: nStars++; break;
                    case GalaxyBodyType::GasCloud: nGas++; break;
                    case GalaxyBodyType::StellarCluster: nCluster++; break;
                    case GalaxyBodyType::DwarfGalaxy: nDwarf++; break;
                }
            }
            ss << "Galaxy system: ";
            if (nStars)   ss << nStars << " stars ";
            if (nGas)     ss << nGas << " gas clouds ";
            if (nCluster) ss << nCluster << " clusters ";
            if (nDwarf)   ss << nDwarf << " dwarfs ";
            ss << "\n";

            if (ui.showInfluenceZones && preset.isGalacticCenter) {
                ss << "Zones: tidal=" << pretty(preset.zones.tidalDisruptionM * M)
                   << "m  Bondi=" << pretty(preset.zones.bondiRadiusM * M)
                   << "m  SoI=" << pretty(preset.zones.sphereOfInfluenceM * M) << "m\n";
            }
        }

        if (!sim.bodies.empty()) {
            const auto& body = sim.bodies[0];
            ss << "r = " << pretty(body.radius()) << "  v = "
               << pretty(body.measurement.orbitalVelocity) << "c  1+z = "
               << pretty(body.measurement.redshift) << "\n";
        }
        ss << "Presets: [ / ] to switch | T toggle preset | Up/Down zoom\n";
    } else {
        double M = sim.bh.metric.M;
        // Pulsar orbital scenario: show GW summary instead of generic orbital state
        if (sim.activeScenario == ResearchScenario::PulsarOrbital) {
            const auto& pd = sim.pulsarData;
            auto gw = [](double v) -> std::string {
                std::ostringstream os;
                os << std::scientific << std::setprecision(2) << v;
                return os.str();
            };
            ss << "Pulsar orbital sim  [" << std::fixed << std::setprecision(2)
               << sim.pulsarState.massSolar << " Msun NS @ "
               << std::setprecision(1) << pd.semiMajorAxis_M << "M]"
               << (pd.disrupted ? "  DISRUPTED" : "") << "\n";
            ss << "Shapiro: " << gw(pd.shapiroDelay_us) << " us  "
               << "nu/nu0: " << gw(pd.gravRedshift) << "\n";
            ss << "h(1kpc): " << gw(pd.gwStrain)
               << "  f_GW: " << gw(pd.gwFreq_Hz) << " Hz\n";
            ss << "r/r_Roche: " << pretty(pd.rocheMargin)
               << "  e: " << pretty(pd.eccentricity) << "\n";
            ss << "D: data panel  5: restart scenario\n";
        } else {
            ss << "M = " << pretty(M) << "  (r_h=" << pretty(sim.bh.metric.horizon())
               << ", ISCO=" << pretty(sim.bh.metric.isco()) << ")\n";
        if (!sim.bodies.empty()) {
            const auto& body = sim.bodies[0];
            ss << "E = " << pretty(body.E)
               << "  L = " << pretty(body.L)
               << "  r = " << pretty(body.radius())
               << "  v = " << pretty(body.measurement.orbitalVelocity) << "c\n";
            ss << "1+z = " << pretty(body.measurement.redshift)
               << "  gamma = " << pretty(body.measurement.timeDilation)
               << "  (a~" << pretty(body.nominalA)
               << " e~" << pretty(body.nominalEcc) << ")\n";
        }
        ss << "Photon sphere (r=3M) shown: " << (ui.showPhotonSphere ? "YES" : "NO")
           << "    Rays: " << (ui.showRays ? "ON" : "OFF") << "\n";
        }
    }
    if (ui.timeScale != 1.0) {
        std::ostringstream ts;
        if (ui.timeScale >= 1.0) ts << (int)ui.timeScale;
        else ts << std::fixed << std::setprecision(2) << ui.timeScale;
        ss << "Speed: " << ts.str() << "x  |  ";
    }
    ss << "Press / for controls";
    return ss.str();
}

/*--------- Entry point ---------*/
int main(int argc, char* argv[]) {
    const unsigned int WIN_W = 1200, WIN_H = 800;
    sf::RenderWindow window(sf::VideoMode(sf::Vector2u(WIN_W, WIN_H)),
                            "Schwarzschild BH - 2D demo");
    window.setFramerateLimit(60);

    Simulation sim;
    sim.rebuildPhotons(WIN_H);

    Camera   camera{ sim.params.pixelsPerM, (float)WIN_W, (float)WIN_H };
    Renderer renderer(window);
    UIState  ui;

    // CLI: --preset <id>  (ton618, sgra, 3c273, j0529, m87, cygnusx1, gw150914, intermediate, primordial)
    // useful for screenshots and demos — lets you launch directly into a specific black hole without
    // clicking through the menu. the string-to-index mapping below is clunky but there aren't that many
    // presets so I'm not going to build a proper lookup table for it
    for (int i = 1; i < argc - 1; ++i) {
        if (std::strcmp(argv[i], "--preset") == 0) {
            std::string id = argv[i + 1];
            int idx = -1;
            if      (id == "ton618")       idx = 0;
            else if (id == "sgra")         idx = 1;
            else if (id == "3c273")        idx = 2;
            else if (id == "j0529")        idx = 3;
            else if (id == "m87")          idx = 4;
            else if (id == "cygnusx1")     idx = 5;
            else if (id == "gw150914")     idx = 6;
            else if (id == "intermediate") idx = 7;
            else if (id == "primordial")   idx = 8;
            if (idx >= 0 && idx < NUM_BH2D_PRESETS) {
                ui.presetActive = true;
                ui.presetIdx    = idx;
                sim.bh.metric.M   = units::solarMassToGeomMeters(BH2D_PRESETS[idx].massSolar);
                sim.params.pixelsPerM = UIState::presetHorizonPixelsTarget / (2.0 * sim.bh.metric.M);
                sim.reinitBodies();
                if (ui.showGalaxySystem && BH2D_PRESETS[idx].isGalacticCenter)
                    sim.spawnGalaxySystem(idx);
                sim.rebuildPhotons(WIN_H);
            }
            break;
        }
    }

    // Set the initial view to match window size
    // this has to be done after the window is created and before the main loop.
    // if you forget it, SFML renders into a 1x1 pixel view or something equally absurd.
    // I forgot it once. it was a confusing 20 minutes.
    renderer.resetView(camera.viewWidth, camera.viewHeight);

    sf::Clock clock;
    std::vector<sf::Vertex> rayVertScratch; // reusable scratch buffer — before this existed we were allocating a new vector every frame for every ray.
                                             // at 120 rays × 60fps that was generating ~7200 heap allocations per second just for ray drawing.
                                             // embarrassing in retrospect but at least it's fixed now

    // HUD text caching to avoid expensive string formatting every frame
    std::string cachedHUD;
    int cachedPresetIdx = -1;
    bool cachedPresetActive = false;
    bool cachedGalaxySystemActive = false;
    size_t cachedBodyCount = 0;
    ResearchScenario cachedScenario = ResearchScenario::None;
    int cachedNotificationTimer = -1;
    double hudRefreshAccum = 0.0;
    constexpr double HUD_REFRESH_SECONDS = 0.2;

    auto hudNeedsRebuild = [&]() {
        return cachedPresetIdx != ui.presetIdx
            || cachedPresetActive != ui.presetActive
            || cachedGalaxySystemActive != sim.galaxySystemActive
            || cachedBodyCount != sim.bodies.size()
            || cachedScenario != sim.activeScenario
            || cachedNotificationTimer != ui.notificationTimer;
    };

    while (window.isOpen()) {
        /*--------- Input ---------*/
        while (auto evOpt = window.pollEvent()) {
            if (evOpt->is<sf::Event::Closed>()) { window.close(); continue; }

            // Handle window resize / fullscreen: update view + camera
            // SFML 3.x changed how resize events work vs 2.x — this tripped me up for a while.
            // the view needs to be explicitly reset or everything stays at the old dimensions
            // and you get stretched/clipped rendering. fun bug to track down at midnight.
            if (auto* resized = evOpt->getIf<sf::Event::Resized>()) {
                float w = (float)resized->size.x;
                float h = (float)resized->size.y;
                camera.updateSize(w, h);
                renderer.resetView(w, h);
            }

            handleInput(*evOpt, sim, ui, (unsigned int)camera.viewHeight);
        }

        /*--------- Update ---------*/
        double dt = clock.restart().asSeconds();
        if (!ui.paused)
            sim.update(dt * ui.timeScale);
        hudRefreshAccum += dt;

        camera.pixelsPerM = sim.params.pixelsPerM;
        sf::Vector2f center = camera.center();
        double M = sim.bh.metric.M;

        // Rebuild HUD text only when relevant simulation state changes
        if (hudRefreshAccum >= HUD_REFRESH_SECONDS || hudNeedsRebuild()) {
            cachedHUD = buildHUD(sim, ui);
            cachedPresetIdx = ui.presetIdx;
            cachedPresetActive = ui.presetActive;
            cachedGalaxySystemActive = sim.galaxySystemActive;
            cachedBodyCount = sim.bodies.size();
            cachedScenario = sim.activeScenario;
            cachedNotificationTimer = ui.notificationTimer;
            hudRefreshAccum = 0.0;
        }

        /*--------- Draw ---------*/
        renderer.beginFrame();
        renderer.drawStarfield();

        // Influence zones (behind everything except starfield)
        if (ui.presetActive && ui.showInfluenceZones && ui.showGalaxySystem) {
            const auto& preset = BH2D_PRESETS[ui.presetIdx];
            if (preset.isGalacticCenter) {
                float ppm = (float)camera.pixelsPerM;
                float soiPx   = (float)(preset.zones.sphereOfInfluenceM * M * ppm);
                float bondiPx = (float)(preset.zones.bondiRadiusM * M * ppm);
                float tidalPx = (float)(preset.zones.tidalDisruptionM * M * ppm);

                renderer.drawInfluenceZone(center, soiPx,
                    sf::Color(100, 255, 100, 80), "Sphere of Influence");
                renderer.drawInfluenceZone(center, bondiPx,
                    sf::Color(255, 200, 50, 90), "Bondi Radius");
                renderer.drawInfluenceZone(center, tidalPx,
                    sf::Color(255, 80, 80, 120), "Tidal Disruption");

                if (preset.zones.hasJetCones) { // draws relativistic jets if the preset has them (most don't)
                    float jetLen = soiPx * 1.5f;
                    renderer.drawJetCones(center, jetLen);
                }
            }
        }

        renderer.drawHorizon(center, (float)(sim.bh.metric.horizon() * camera.pixelsPerM));

        if (ui.presetActive) { //  && BH2D_PRESETS[ui.presetIdx].hasAccretionDisk maybe?
            // the accretion disk is just a glowing ring drawn on screen. it's not physically simulated.
            // please do not ask me when I'm adding MHD accretion disk dynamics. the answer is never.
            float diskRadius = (float)(6.0 * M * camera.pixelsPerM);
            renderer.drawAccretionDisk(center, diskRadius);
        }

        if (ui.showPhotonSphere)
            renderer.drawPhotonSphere(center, (float)(sim.bh.metric.photonSphere() * camera.pixelsPerM));

        // Rays — visualization layer computes colors
        if (ui.showRays) {
            for (const auto& photon : sim.photons) {
                if (photon.captured) {
                    auto [v0, v1] = RayVisualizer::capturedRayLine(photon.impactParameter, camera);
                    renderer.drawCapturedRay(v0, v1);
                } else {
                    RayVisualizer::colorByRedshift(photon, sim.bh.metric, camera, rayVertScratch);
                    renderer.drawRayPath(rayVertScratch);
                }
            }
        }

        // Caustic highlights (where rays converge strongly)
        if (ui.showCaustics && !sim.lensingData.causticPoints.empty()) {
            for (const auto& [avgB, gradient] : sim.lensingData.causticPoints) {
                // Mark caustic regions on both sides of the BH
                sf::Vector2f pos1 = camera.worldToScreen(0.0f, (float)avgB);
                sf::Vector2f pos2 = camera.worldToScreen(0.0f, (float)-avgB);
                renderer.drawCausticMarker(pos1, gradient);
                renderer.drawCausticMarker(pos2, gradient);
            }
        }

        // Time dilation heatmap
        if (ui.showTimeDilationMap)
            renderer.drawTimeDilationMap(center, M, camera.pixelsPerM);

        // Bodies + orbit paths
        for (size_t bodyIdx = 0; bodyIdx < sim.bodies.size(); ++bodyIdx) {
            const auto& body = sim.bodies[bodyIdx];
            auto bodyVis   = OrbitVisualizer::computeBodyVisual(body, sim.bh.metric, camera);
            renderer.drawBody(bodyVis);

            // Pulsar-specific visual decoration (jets, magnetic field lines, LC ring, flux tubes)
            if (sim.activeScenario == ResearchScenario::PulsarOrbital && bodyIdx == 0
                && !body.captured) {
                float lc_px = 0.0f;
                if (sim.pulsarState.lightCylRadius_m > 0.0)
                    lc_px = (float)(sim.pulsarState.lightCylRadius_m * camera.pixelsPerM);
                renderer.drawPulsarBody(
                    bodyVis.screenPos,
                    (float)sim.pulsarState.spinPhase,
                    (float)sim.pulsarState.precPhase,
                    lc_px,
                    sim.pulsarData.inLightCylinder,
                    center,                          // BH screen position (defined at line ~240)
                    sim.pulsarData.magPower_ergs
                );
            }

            auto orbitPath = OrbitVisualizer::computeOrbitPath(body, sim.bh.metric, camera);
            renderer.drawOrbitPath(orbitPath);

            // Label galaxy system bodies
            if (body.isGalaxyBody && ui.showGalaxySystem && !body.label.empty()) {
                sf::Color labelColor;
                switch (body.bodyType) {
                    case GalaxyBodyType::Star:           labelColor = sf::Color(255, 240, 200); break;
                    case GalaxyBodyType::GasCloud:       labelColor = sf::Color(255, 150, 100); break;
                    case GalaxyBodyType::StellarCluster:  labelColor = sf::Color(200, 220, 255); break;
                    case GalaxyBodyType::DwarfGalaxy:    labelColor = sf::Color(255, 200, 255); break;
                }
                renderer.drawBodyLabel(bodyVis.screenPos, body.label, labelColor);
            }

            // Label research scenario bodies
            if (!body.isGalaxyBody && !body.label.empty() &&
                sim.activeScenario != ResearchScenario::None) {
                renderer.drawBodyLabel(bodyVis.screenPos, body.label,
                    sf::Color(180, 220, 255));
            }
        }

        renderer.drawHUD(cachedHUD);

        // Controls panel (left side, toggleable with /)
        if (ui.showControlsPanel)
            renderer.drawControlsPanel(camera.viewHeight);

        // Data panel (right side)
        if (ui.showDataPanel)
            renderer.drawDataPanel(sim.formatDataPanel(),
                                   camera.viewWidth, camera.viewHeight);

        // Notification (bottom)
        if (ui.notificationTimer > 0) {
            renderer.drawNotification(ui.notification,
                                      camera.viewWidth, camera.viewHeight);
            ui.notificationTimer--;
        }

        renderer.endFrame();
    }

    // using exit(0) instead of return 0 because SFML's static cleanup on macOS can occasionally
    // hang waiting for threads that have already finished. exit() skips the destructors entirely.
    // ugly but functional, and I'd rather not track down a platform-specific cleanup hang right now.
    exit(0);
}
