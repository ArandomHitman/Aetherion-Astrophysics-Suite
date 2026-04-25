#pragma once
#include <SFML/Graphics.hpp>
#include "../2D-core/body_visual.hpp"
#include "../2D-simulation/orbiting_body.hpp"
#include "../2D-physics/schwarzschild.hpp"
#include "../2D-rendering/camera.hpp"
#include <cmath>
#include <algorithm>
#include <cstdint>

class OrbitVisualizer {
public:
    // Compute the body's visual appearance (tidal stretch + redshift color).
    static BodyVisual computeBodyVisual(
        const OrbitingBody& body,
        const Schwarzschild& bh,
        const Camera& cam)
    {
        BodyVisual vis;
        double r_current = body.radius();
        vis.screenPos = cam.worldToScreen((float)body.worldX(), (float)body.worldY());

        // Tidal stretching from geodesic deviation
        double tidal_factor = 2.0 * bh.M / std::pow(r_current, 3);
        double dt_vis  = 0.3;
        double stretch = std::exp(tidal_factor * dt_vis);
        double squeeze = 1.0 / std::exp(tidal_factor * dt_vis * 0.5);

        // Body type determines base size and color bias
        double baseW = 12.0, baseH = 12.0;
        if (body.isGalaxyBody) {
            switch (body.bodyType) {
                case GalaxyBodyType::Star:           baseW = 8.0;  baseH = 8.0;   break;
                case GalaxyBodyType::GasCloud:       baseW = 16.0; baseH = 14.0;  break;
                case GalaxyBodyType::StellarCluster:  baseW = 20.0; baseH = 20.0;  break;
                case GalaxyBodyType::DwarfGalaxy:    baseW = 24.0; baseH = 18.0;  break;
                case GalaxyBodyType::NeutronStar:    baseW = 5.0;  baseH = 5.0;   break;
                case GalaxyBodyType::WhiteDwarf:     baseW = 7.0;  baseH = 7.0;   break;
                case GalaxyBodyType::CompanionStar:  baseW = 11.0; baseH = 11.0;  break;
            }
        }
        vis.width  = (float)(baseW * stretch);
        vis.height = (float)(baseH * squeeze);

        double bodyAngle = std::atan2(body.worldY(), body.worldX());
        vis.angleDeg = (float)(bodyAngle * 180.0 / M_PI);

        // Redshift-based coloring from proper GR measurement (1+z).
        double z = body.measurement.redshift - 1.0;
        double t_red  = std::clamp(z / 0.5, 0.0, 1.0);
        double t_blue = std::clamp(-z / 0.3, 0.0, 1.0);

        // Body-type base colors
        double base_r = 200.0, base_g = 200.0, base_b = 180.0;
        if (body.isGalaxyBody) {
            switch (body.bodyType) {
                case GalaxyBodyType::Star:
                    base_r = 255; base_g = 240; base_b = 200; break;
                case GalaxyBodyType::GasCloud:
                    base_r = 255; base_g = 150; base_b = 100; break;
                case GalaxyBodyType::StellarCluster:
                    base_r = 200; base_g = 220; base_b = 255; break;
                case GalaxyBodyType::DwarfGalaxy:
                    base_r = 255; base_g = 200; base_b = 255; break;
                case GalaxyBodyType::NeutronStar:
                    base_r = 160; base_g = 200; base_b = 255; break;
                case GalaxyBodyType::WhiteDwarf:
                    base_r = 200; base_g = 215; base_b = 255; break;
                case GalaxyBodyType::CompanionStar:
                    base_r = 255; base_g = 200; base_b = 140; break;
            }
        }

        uint8_t rcol = static_cast<uint8_t>(std::min(255.0, base_r + 55.0 * t_red - 80.0 * t_blue));
        uint8_t gcol = static_cast<uint8_t>(std::min(255.0, base_g * (1.0 - 0.5 * t_red - 0.3 * t_blue)));
        uint8_t bcol = static_cast<uint8_t>(std::min(255.0, base_b + 75.0 * t_blue - 100.0 * t_red));
        vis.color = sf::Color(rcol, gcol, bcol);

        return vis;
    }

    // Draw orbit trail from numerically integrated geodesic history.
    // This naturally shows GR precession as the trail accumulates.
    static sf::VertexArray computeOrbitPath(
        const OrbitingBody& body,
        const Schwarzschild& /*bh*/,
        const Camera& cam)
    {
        const auto& trail = body.trail;
        if (trail.size() < 2)
            return sf::VertexArray(sf::PrimitiveType::LineStrip, 0);

        sf::VertexArray orbitLine(sf::PrimitiveType::LineStrip, trail.size());

        for (size_t i = 0; i < trail.size(); ++i) {
            float wx = (float)trail[i].first;
            float wy = (float)trail[i].second;
            orbitLine[i].position = cam.worldToScreen(wx, wy);

            // Gradient: recent positions brighter, older positions fade
            double t = (double)i / (double)(trail.size() - 1);
            uint8_t rc = static_cast<uint8_t>(80 + 120 * t);
            uint8_t gc = static_cast<uint8_t>(160 + 40 * t);
            uint8_t bc = static_cast<uint8_t>(180 + 75 * t);
            uint8_t alpha = static_cast<uint8_t>(40 + 100 * t);
            orbitLine[i].color = sf::Color(rc, gc, bc, alpha);
        }

        return orbitLine;
    }
};
