#pragma once
#include <cmath>

// 2D-only black hole presets (no glm dependency).
// Mirrors the legacy BH2D_PRESETS from src/presets.hpp for the 2D simulator.

/*--------- Body type for galaxy system members ---------*/
enum class GalaxyBodyType {
    Star,           // Individual star orbiting the SMBH
    GasCloud,       // Gas cloud / accretion clump
    StellarCluster, // Dense stellar cluster
    DwarfGalaxy,    // Dwarf galaxy / satellite
    NeutronStar,    // Neutron star (compact remnant)
    WhiteDwarf,     // White dwarf (compact remnant)
    CompanionStar   // Companion star in a binary system
};

inline const char* bodyTypeName(GalaxyBodyType t) {
    switch (t) {
        case GalaxyBodyType::Star:           return "Star";
        case GalaxyBodyType::GasCloud:       return "Gas Cloud";
        case GalaxyBodyType::StellarCluster: return "Stellar Cluster";
        case GalaxyBodyType::DwarfGalaxy:    return "Dwarf Galaxy";
        case GalaxyBodyType::NeutronStar:    return "Neutron Star";
        case GalaxyBodyType::WhiteDwarf:     return "White Dwarf";
        case GalaxyBodyType::CompanionStar:  return "Companion Star";
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

// Cygnus X-1 — 21.2 Msun, HMXB with O-supergiant companion HDE 226868
inline constexpr GalaxySystemBody CYGNUSX1_BODIES[] = {
    {"HDE 226868 (O-supergiant)",  GalaxyBodyType::CompanionStar, 25.0, 0.018},
};

// GW150914 — 62 Msun merger remnant; show pre-merger binary for educational context
inline constexpr GalaxySystemBody GW150914_BODIES[] = {
    {"Pre-merger BH-1 (~36 Msun)", GalaxyBodyType::Star,          15.0, 0.15},
    {"Pre-merger BH-2 (~29 Msun)", GalaxyBodyType::Star,          22.0, 0.12},
};

// Intermediate-mass — 1e4 Msun; globular cluster environment
inline constexpr GalaxySystemBody INTERMEDIATE_BODIES[] = {
    {"Globular cluster star A",     GalaxyBodyType::Star,       20.0, 0.35},
    {"Globular cluster star B",     GalaxyBodyType::Star,       35.0, 0.20},
    {"Compact remnant",             GalaxyBodyType::WhiteDwarf, 15.0, 0.50},
};

// Gaia BH1 — 9.62 Msun, quiescent, G-dwarf companion in wide orbit
inline constexpr GalaxySystemBody GAIABH1_BODIES[] = {
    {"G-dwarf companion",           GalaxyBodyType::CompanionStar, 28.0, 0.45},
};

// Gaia BH2 — 8.94 Msun, quiescent, red giant companion
inline constexpr GalaxySystemBody GAIABH2_BODIES[] = {
    {"Red giant companion",         GalaxyBodyType::CompanionStar, 30.0, 0.52},
};

// Gaia BH3 — 32.7 Msun, quiescent, metal-poor giant companion
inline constexpr GalaxySystemBody GAIABH3_BODIES[] = {
    {"Metal-poor giant companion",  GalaxyBodyType::CompanionStar, 22.0, 0.39},
};

// V404 Cygni — 9.0 Msun, X-ray nova microquasar, K-giant companion
inline constexpr GalaxySystemBody V404CYG_BODIES[] = {
    {"K-giant companion",           GalaxyBodyType::CompanionStar, 26.0, 0.034},
};

// A0620-00 — 6.61 Msun, quiescent X-ray transient, K-dwarf companion
inline constexpr GalaxySystemBody A062000_BODIES[] = {
    {"K-dwarf companion",           GalaxyBodyType::CompanionStar, 24.0, 0.0},
};

// GRO J1655-40 — 6.3 Msun, microquasar with relativistic jets, F-star companion
inline constexpr GalaxySystemBody GROJ1655_BODIES[] = {
    {"F-star companion",            GalaxyBodyType::CompanionStar, 22.0, 0.0},
};

// NGC 1277 — 1.7e10 Msun, overmassive SMBH in compact elliptical
inline constexpr GalaxySystemBody NGC1277_BODIES[] = {
    {"Inner stellar orbit A",       GalaxyBodyType::Star,           12.0, 0.30},
    {"Inner stellar orbit B",       GalaxyBodyType::Star,           20.0, 0.45},
    {"Gas cloud",                   GalaxyBodyType::GasCloud,       30.0, 0.20},
    {"Stellar cluster",             GalaxyBodyType::StellarCluster, 75.0, 0.10},
};

// OJ 287 — 1.8e10 Msun primary, known SMBH binary with a ~1.5e8 Msun secondary
inline constexpr GalaxySystemBody OJ287_BODIES[] = {
    {"Secondary SMBH (~1.5e8 Msun)", GalaxyBodyType::Star,     15.0, 0.66},
    {"Accretion blob",               GalaxyBodyType::GasCloud, 22.0, 0.30},
    {"Outer gas cloud",              GalaxyBodyType::GasCloud, 40.0, 0.20},
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

    {"Cygnus X-1",        21.2,    "Famous stellar-mass HMXB black hole (~21 Msun)",
     true,  CYGNUSX1_BODIES,    1, {8.0, 12.0, 30.0, false}},

    {"LIGO GW150914",     62.0,    "First gravitational wave merger remnant (~62 Msun)",
     true,  GW150914_BODIES,    2, {8.0, 15.0, 35.0, false}},

    {"Intermediate-mass", 1e4,     "Hypothetical intermediate-mass BH (~1e4 Msun)",
     true,  INTERMEDIATE_BODIES, 3, {10.0, 40.0, 60.0, false}},

    {"Primordial",        1e-5,    "Tiny primordial black hole (~1e-5 Msun)",
     false, nullptr, 0,    {0, 0, 0, false}},

    {"Gaia BH1",          9.62,    "Nearest dormant BH, G-dwarf companion in wide orbit (~9.6 Msun)",
     true,  GAIABH1_BODIES,     1, {8.0, 12.0, 32.0, false}},

    {"Gaia BH2",          8.94,    "Dormant BH with red giant companion (~8.9 Msun)",
     true,  GAIABH2_BODIES,     1, {8.0, 12.0, 35.0, false}},

    {"Gaia BH3",          32.7,    "Most massive nearby dormant BH, metal-poor giant companion (~33 Msun)",
     true,  GAIABH3_BODIES,     1, {8.0, 12.0, 28.0, false}},

    {"V404 Cygni",        9.0,     "X-ray nova microquasar with K-giant companion (~9 Msun)",
     true,  V404CYG_BODIES,     1, {8.0, 12.0, 30.0, true}},

    {"A0620-00",          6.61,    "First confirmed stellar BH, quiescent K-dwarf binary (~6.6 Msun)",
     true,  A062000_BODIES,     1, {8.0, 12.0, 28.0, false}},

    {"GRO J1655-40",      6.3,     "Microquasar with relativistic jets, F-star companion (~6.3 Msun)",
     true,  GROJ1655_BODIES,    1, {8.0, 12.0, 26.0, true}},

    {"NGC 1277",          1.7e10,  "Overmassive SMBH in compact elliptical galaxy (~1.7e10 Msun)",
     true,  NGC1277_BODIES,     4, {11.0, 88.0, 120.0, false}},

    {"OJ 287",            1.8e10,  "SMBH binary system with known orbital outbursts (~1.8e10 Msun primary)",
     true,  OJ287_BODIES,       3, {10.0, 85.0, 115.0, true}}
};
inline constexpr int NUM_BH2D_PRESETS = sizeof(BH2D_PRESETS) / sizeof(BH2D_PRESETS[0]);
