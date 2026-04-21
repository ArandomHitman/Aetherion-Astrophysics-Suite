#pragma once
#include <SFML/Graphics.hpp>
#include "../2D-simulation/photon.hpp"
#include "../2D-physics/schwarzschild.hpp"
#include "../2D-rendering/camera.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

class RayVisualizer {
public:
    // Build colored vertices for a single photon ray into a reusable buffer.
    // Color encodes the local gravitational redshift z = 1/√f - 1.
    // Takes an output reference to avoid per-frame heap allocation — pass the
    // same vector each frame and it will reuse its capacity after the first frame.
    static void colorByRedshift(
        const Photon& photon,
        const Schwarzschild& bh,
        const Camera& cam,
        std::vector<sf::Vertex>& verts)
    {
        verts.clear();
        if (photon.path.empty()) return;
        verts.reserve(photon.path.size());

        for (const auto& w : photon.path) {
            auto s = cam.worldToScreen(w.x, w.y);
            double r = std::hypot((double)w.x, (double)w.y);

            // Gravitational redshift: 1+z = 1/√(1-2M/r)
            double fr = bh.f(r);
            double z = (fr > 1e-6) ? (1.0 / std::sqrt(fr) - 1.0) : 10.0;

            // Map z ∈ [0, ~3] to a blue → white → red color ramp
            //   z ≈ 0:   white/blue (far from BH)
            //   z ≈ 0.5: warm white
            //   z > 1:   deep red (strong field)
            double t_red  = std::clamp(z / 1.5, 0.0, 1.0);
            double t_blue = std::clamp(1.0 - z / 0.8, 0.0, 1.0);

            uint8_t rc = static_cast<uint8_t>(180 + 75.0 * t_red);
            uint8_t gc = static_cast<uint8_t>(220.0 * (1.0 - 0.7 * t_red));
            uint8_t bc = static_cast<uint8_t>(255.0 * t_blue + 30.0 * (1.0 - t_blue));

            // Fade alpha near edge of integration domain
            double dist_t = std::clamp((r - bh.horizon()) / (50.0 * bh.M), 0.0, 1.0);
            uint8_t alpha = static_cast<uint8_t>(120.0 * dist_t + 50.0);

            verts.emplace_back(sf::Vertex{ s, sf::Color(rc, gc, bc, alpha) });
        }
    }

    // Build line endpoints for a captured ray (aimed at center).
    static std::pair<sf::Vertex, sf::Vertex> capturedRayLine(
        double b,
        const Camera& cam)
    {
        constexpr double cameraDist = 2000.0;
        sf::Vector2f p0 = cam.worldToScreen((float)-cameraDist, (float)b);
        sf::Vector2f ct = cam.center();
        return {
            sf::Vertex{ p0, sf::Color(200, 80, 80) },
            sf::Vertex{ ct, sf::Color(200, 30, 30) }
        };
    }
};
