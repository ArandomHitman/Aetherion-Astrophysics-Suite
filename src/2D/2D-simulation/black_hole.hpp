#pragma once // this prevents multiple inclusion of this header file
#include "../2D-physics/schwarzschild.hpp"

// yes, BlackHole is literally just a wrapper struct around Schwarzschild. at one point it was going
// to have more state (Kerr spin parameter a, charge Q, etc.) but I never implemented spin and
// the charge case is physically boring for astrophysics, so now it just... wraps Schwarzschild.
// it does at least give us a clean type to pass around without everything depending on Schwarzschild directly.
// future me's problem if we ever add Kerr.
struct BlackHole {
    Schwarzschild metric;
};
