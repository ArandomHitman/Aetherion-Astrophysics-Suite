#pragma once
#include <cmath>

// 2D-only black hole presets (no glm dependency).
// Mirrors the legacy BH2D_PRESETS from src/presets.hpp for the 2D simulator.

/*--------- Body type for galaxy system members ---------*/
enum class GalaxyBodyType {
    Star,           // Individual star orbiting the SMBH
    GasCloud,       // Gas cloud / accretion clump
    StellarCluster, // Dense stellar cluster
    DwarfGalaxy     // Dwarf galaxy / satellite
};

inline const char* bodyTypeName(GalaxyBodyType t) {
    switch (t) {
        case GalaxyBodyType::Star:           return "Star";
        case GalaxyBodyType::GasCloud:       return "Gas Cloud";
        case GalaxyBodyType::StellarCluster: return "Stellar Cluster";
        case GalaxyBodyType::DwarfGalaxy:    return "Dwarf Galaxy";
    }
    return "Unknown";
}

// A single orbiting member of the galaxy system around the SMBH.
// Radii in units of M (geometric mass).
struct GalaxySystemBody {
    const char*    label;         // e.g. "S2-like star"
    GalaxyBodyType type;
    double         semiMajorM;    // Semi-major axis in multiples of M
    double         eccentricity;  // Orbital eccentricity
};

// Influence zone radii in multiples of M.
struct InfluenceZones {
    double tidalDisruptionM;      // Tidal disruption radius (solar-type star)
    double bondiRadiusM;          // Bondi accretion radius
    double sphereOfInfluenceM;    // Gravitational sphere of influence
    bool   hasJetCones;           // Whether to show relativistic jet cones
};

struct BHPreset2D {
    const char* name;
    double      massSolar;
    const char* description;
    bool        isGalacticCenter;  // Has a host galaxy system

    // Galaxy system (only meaningful if isGalacticCenter == true)
    const GalaxySystemBody* galaxyBodies;
    int                     numGalaxyBodies;
    InfluenceZones          zones;
};

/*--------- Galaxy system body definitions per preset ---------*/
// NOTE: These must be inline constexpr (not static const) so that all
// translation units share a single canonical address for each array.
// Using static const would give each TU its own copy, causing an ODR
// violation in BH2D_PRESETS (which is inline and must be identical in every TU).

// TON 618 — most massive quasar, ~66 billion Msun
inline constexpr GalaxySystemBody TON618_BODIES[] = {
    {"Inner accretion clump",   GalaxyBodyType::GasCloud,        8.0,  0.15},
    {"Tidal-stripped star",     GalaxyBodyType::Star,            15.0,  0.55},
    {"Hot gas filament",        GalaxyBodyType::GasCloud,        25.0,  0.30},
    {"Plunging star",           GalaxyBodyType::Star,            12.0,  0.80},
    {"Outer stellar orbit",     GalaxyBodyType::Star,            50.0,  0.20},
    {"Satellite cluster",       GalaxyBodyType::StellarCluster,  80.0,  0.10},
};

// Sgr A* — Supermassive BH at the center of our Milky Way, ~4.3 million Msun
inline constexpr GalaxySystemBody SGRA_BODIES[] = {
    {"S2 analog",               GalaxyBodyType::Star,            20.0,  0.88},
    {"S14-like close orbit",    GalaxyBodyType::Star,            10.0,  0.95},
    {"IRS 16 cluster star",     GalaxyBodyType::Star,            40.0,  0.30},
    {"Circumnuclear gas clump", GalaxyBodyType::GasCloud,        60.0,  0.15},
    {"S-cluster member",        GalaxyBodyType::Star,            30.0,  0.50},
};

// 3C 273 — first quasar ever detected, ~890 million Msun
inline constexpr GalaxySystemBody QSO3C273_BODIES[] = {
    {"Inner jet-base cloud",    GalaxyBodyType::GasCloud,        10.0,  0.20},
    {"BLR cloud",               GalaxyBodyType::GasCloud,        20.0,  0.40},
    {"Stripped dwarf remnant",  GalaxyBodyType::DwarfGalaxy,     70.0,  0.25},
    {"Close stellar orbit",     GalaxyBodyType::Star,            35.0,  0.60},
};

// J0529-4351 — most luminous quasar, ~17 billion Msun
inline constexpr GalaxySystemBody J0529_BODIES[] = {
    {"Fast accretion blob",     GalaxyBodyType::GasCloud,         9.0,  0.10},
    {"UV-bright clump",         GalaxyBodyType::GasCloud,        18.0,  0.35},
    {"Tidally disrupting star", GalaxyBodyType::Star,            14.0,  0.70},
    {"Outer gas stream",        GalaxyBodyType::GasCloud,        45.0,  0.20},
    {"Infalling cluster",       GalaxyBodyType::StellarCluster,  65.0,  0.15},
};

// M87 — 6.5 billion Msun, EHT imaged
inline constexpr GalaxySystemBody M87_BODIES[] = {
    {"Jet-base knot",           GalaxyBodyType::GasCloud,        10.0,  0.12},
    {"Inner stellar orbit",     GalaxyBodyType::Star,            20.0,  0.45},
    {"Hot gas shell fragment",  GalaxyBodyType::GasCloud,        35.0,  0.25},
    {"Globular cluster",        GalaxyBodyType::StellarCluster,  90.0,  0.08},
    {"Infalling dwarf",         GalaxyBodyType::DwarfGalaxy,     70.0,  0.30},
};

/*--------- Preset table ---------*/
inline constexpr BHPreset2D BH2D_PRESETS[] = {
    {"TON 618",           6.6e10,  "Most massive known quasar BH (~6.6e10 Msun)",
     true,  TON618_BODIES, 6, {12.0, 100.0, 120.0, true}},

    {"Sgr A*",            4.3e6,   "Milky Way center (~4.3e6 Msun)",
     true,  SGRA_BODIES,   5, {15.0, 80.0,  100.0, false}},

    {"3C 273",            8.86e8,  "First quasar identified (~8.9e8 Msun)",
     true,  QSO3C273_BODIES, 4, {12.0, 90.0, 110.0, true}},

    {"J0529-4351",        1.7e10,  "Most luminous quasar (~1.7e10 Msun)",
     true,  J0529_BODIES,  5, {11.0, 95.0, 130.0, true}},

    {"M87",               6.5e9,   "M87 galaxy BH (~6.5e9 Msun, EHT image)",
     true,  M87_BODIES,    5, {13.0, 85.0, 115.0, true}},

    {"Cygnus X-1",        21.2,    "Famous stellar-mass black hole (~21 Msun)",
     false, nullptr, 0,    {0, 0, 0, false}},

    {"LIGO GW150914",     62.0,    "First gravitational wave merger remnant (~62 Msun)",
     false, nullptr, 0,    {0, 0, 0, false}},

    {"Intermediate-mass", 1e4,     "Hypothetical intermediate-mass BH (~1e4 Msun)",
     false, nullptr, 0,    {0, 0, 0, false}},

    {"Primordial",        1e-5,    "Tiny primordial black hole (~1e-5 Msun)",
     false, nullptr, 0,    {0, 0, 0, false}}
};
inline constexpr int NUM_BH2D_PRESETS = sizeof(BH2D_PRESETS) / sizeof(BH2D_PRESETS[0]);
