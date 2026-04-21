/*
TODO: DWARF GALAXY & generic body SIMULATION
=====================================================

PHASE 1: CORE PHYSICS VALIDATION
--------------------------------
- Implement custom FITS unit parsing for 'rg', 'M' in astropy (for export compatibility)
- Export simulation parameters to PRIMARY HDU: M_BH, c_s, r_Bondi/rg, beta
- Add geodesic convergence tests (ISCO radius, frame-dragging angles)
- Compute/export ENERGY_DRIFT per timestep for all integrators

PHASE 2: DYNAMICAL DIAGNOSTICS  
-----------------------------
- Track stream morphology: WIDTH(rg), LENGTH(rg), DENSITY profile
- Loss-cone statistics: THETA_LC, FEED_RATE(stars/s), J_SCATTER
- Angular momentum histogram: J_MIN, J_ISCO per particle
- Precession module: PERIAPSIS_PHI evolution (fix empty table)

PHASE 3: ACCRETION & OBSERVABLES
-------------------------------
- Bondi-Hoyle accretion: MDOT_BONDI(t), MDOT_HOLE(t/M) 
- Gas dynamics grid: RHO_GAS(r,phi), VRAD(r), CS_PROFILE(r)
- Bolometric light curve: L_BOL(t), peak time/flux
- Spectral energy distribution mockup (SED)

PHASE 4: NUMERICAL CONVERGENCE  
-----------------------------
- Adaptive timestep convergence: DT_MIN history, CFL_NUMBER
- Spatial resolution sweep: N_PARTICLES=1e3→1e5
- Integrator comparison: RK4 vs GEOKON vs BULIRSCH-STOER  
- Particle noise analysis: shot noise vs dynamical friction

PHASE 5: PUBLICATION EXPORTS
---------------------------
- Full FITS suite with WCS headers (astropy-compliant)
- HDF5 particle dump for community codes (GADGET/AREPO)
- LaTeX table generator for paper (parameters, convergence)
- JWST/ELT mock images of stream (surface brightness)

SIMULATION BOUNDS (Sgr A* baseline):
- M_BH = 4e6 Msun → rg = 1.2e10 cm
- r_Bondi ≈ 1e4 rg (c_s=10 km/s)
- r_SOI ≈ 1e6 rg (3 pc)
- t_final = 10^5 M (~1 day)

za very critical bug: ENERGY_DRIFT < 1e-10, J_CONSERVATION < 1e-12


              _-o#&&*''''?d:>b\_
          _o/"`''  '',, dMF9MMMMMHo_
       .o&#'        `"MbHMMMMMMMMMMMHo.
     .o"" '         vodM*$&&HMMMMMMMMMM?.
    ,'              $M&ood,~'`(&##MMMMMMH\
   /               ,MMMMMMM#b?#bobMMMMHMMML
  &              ?MMMMMMMMMMMMMMMMM7MMM$R*Hk
 ?$.            :MMMMMMMMMMMMMMMMMMM/HMMM|`*L
|               |MMMMMMMMMMMMMMMMMMMMbMH'   T,
$H#:            `*MMMMMMMMMMMMMMMMMMMMb#}'  `?
]MMH#             ""*""""*#MMMMMMMMMMMMM'    -
MMMMMb_                   |MMMMMMMMMMMP'     :
HMMMMMMMHo                 `MMMMMMMMMT       .
?MMMMMMMMP                  9MMMMMMMM}       -
-?MMMMMMM                  |MMMMMMMMM?,d-    '
 :|MMMMMM-                 `MMMMMMMT .M|.   :
  .9MMM[                    &MMMMM*' `'    .
   :9MMk                    `MMM#"        -
     &M}                     `          .-
      `&.                             .
        `~,   .                     ./
            . _                  .-
              '`--._,dd###pp=""'

*/

/*---------- Header files ---------*/
#pragma once
#include <vector>
#include <utility>
#include <string>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*--------- Precession tracking for orbiting bodies ---------*/
struct PrecessionTracker {
    std::vector<double> periapsisAngles;
    std::vector<double> precessionPerOrbit;
    bool prevVrNegative = false;

    // periapsis detection: vr crosses from negative (falling in) to positive (heading out).
    // this works great for normal elliptical orbits. it gets confused if the body is captured
    // mid-infall and vr goes negative and stays negative forever — in that case we just get an
    // orphaned entry in periapsisAngles with no matching pair, which is fine, we just ignore it.
    void detectPeriapsis(double vr, double phi) {
        bool vrNeg = (vr < 0.0);
        if (prevVrNegative && !vrNeg) {
            periapsisAngles.push_back(phi);
            if (periapsisAngles.size() >= 2) {
                size_t n = periapsisAngles.size();
                double delta = periapsisAngles[n-1] - periapsisAngles[n-2] - 2.0 * M_PI;
                precessionPerOrbit.push_back(delta);
            }
        }
        prevVrNegative = vrNeg;
    }

    double lastPrecessionRad() const {
        if (precessionPerOrbit.empty()) return 0.0;
        return precessionPerOrbit.back();
    }

    double lastPrecessionDeg() const {
        return lastPrecessionRad() * 180.0 / M_PI;
    }

    double averagePrecessionDeg() const {
        if (precessionPerOrbit.empty()) return 0.0;
        double sum = 0.0;
        for (double p : precessionPerOrbit) sum += p;
        return (sum / (double)precessionPerOrbit.size()) * 180.0 / M_PI;
    }

    int orbitsCompleted() const {
        return (int)precessionPerOrbit.size();
    }

    void reset() {
        periapsisAngles.clear();
        precessionPerOrbit.clear();
        prevVrNegative = false;
    }
};

/*--------- Energy conservation tracking ---------*/
struct ConservationTracker {
    double E_initial = 0.0;
    double E_current = 0.0;
    double maxAbsDrift = 0.0;
    std::vector<std::pair<double, double>> driftHistory;
    static constexpr size_t MAX_HISTORY = 2000;
    int sampleCounter = 0;

    // NOTE: E_initial is set at orbit init, not at first step — so any numerical transient
    // at the very first step counts against drift. this is intentional: we want to catch
    // initialization errors too, not just integration drift.
    void init(double E0) {
        E_initial = E0;
        E_current = E0;
        maxAbsDrift = 0.0;
        driftHistory.clear();
        sampleCounter = 0;
    }

    void update(double E_now, double properTime) {
        E_current = E_now;
        double drift = (E_initial > 1e-15)
            ? (E_now - E_initial) / E_initial : 0.0;
        maxAbsDrift = std::max(maxAbsDrift, std::abs(drift));

        // only record every 50 steps — at 4000 sub-steps/frame we'd generate thousands of
        // history points per second otherwise, which would fill MAX_HISTORY almost instantly.
        // 50 is a rough balance between resolution and not eating all your RAM.
        sampleCounter++;
        if (sampleCounter % 50 == 0) {
            driftHistory.emplace_back(properTime, drift);
            if (driftHistory.size() > MAX_HISTORY)
                driftHistory.erase(driftHistory.begin());
        }
    }

    double relativeDrift() const {
        if (E_initial < 1e-15) return 0.0;
        return (E_current - E_initial) / E_initial;
    }

    void reset() {
        E_initial = E_current = 0.0;
        maxAbsDrift = 0.0;
        driftHistory.clear();
        sampleCounter = 0;
    }
};

/*--------- RK4 error estimation (step-doubling) ---------*/
// step-doubling: take one big step, then two half-steps, compare. error ≈ |big - double_half| / 15.
// the 15 is from Richardson extrapolation for a 4th-order method (2^4 - 1 = 15).
// we don't do this every step because it triples the computation cost.
struct NumericalErrorTracker {
    double lastError = 0.0;
    double maxError  = 0.0;
    double avgError  = 0.0;
    int stepCount = 0;
    int checkInterval = 100;
    int stepsSinceCheck = 0;

    void recordError(double error) {
        lastError = error;
        maxError = std::max(maxError, error);
        avgError = (avgError * stepCount + error) / (stepCount + 1);
        stepCount++;
    }

    bool shouldCheck() {
        stepsSinceCheck++;
        if (stepsSinceCheck >= checkInterval) {
            stepsSinceCheck = 0;
            return true;
        }
        return false;
    }

    void reset() {
        lastError = maxError = avgError = 0.0;
        stepCount = 0;
        stepsSinceCheck = 0;
    }
};

/*--------- Photon deflection data ---------*/
struct PhotonDeflection {
    double impactParameter = 0.0;
    double deflectionAngle = 0.0;
    double deflectionDeg   = 0.0;
    bool   captured = false;
};

/*--------- Lensing analysis ---------*/
struct LensingData {
    double criticalImpactParam = 0.0;
    std::vector<PhotonDeflection> deflectionTable;
    std::vector<std::pair<float, float>> causticPoints;
};

/*--------- Photon sphere test ---------*/
struct PhotonSphereTestResult {
    double impactParameter    = 0.0;
    double orbitsBeforeDecay  = 0.0;
    bool   escaped  = false;
    bool   captured = false;
    double stabilityAngle = 0.0;   // total φ swept before escape or capture
    // NOTE: for b exactly at b_crit, the theoretical stabilityAngle is infinite (photon orbits forever).
    // in practice we always get a finite number because floating point isn't exact and the
    // unstable orbit eventually tips one way or the other. the nearer to b_crit, the bigger the number.
};

/*--------- ISCO test ---------*/
struct ISCOTestResult {
    double testRadius_M = 0.0;
    std::string classification;     // "Stable", "Critical", "Unstable"
    double radiusDrift = 0.0;       // how much r has drifted from starting radius (in units of M)
    bool   captured = false;
    double survivalTime = 0.0;
    // if radiusDrift is growing monotonically — the orbit is unstable, as expected.
    // if it oscillates around zero — stable orbit, just normal orbital motion.
    // in practice even the "stable" 7M orbit shows a tiny secular drift due to RK4 not being
    // a symplectic integrator. it's small enough that it doesn't matter for demo purposes, but
    // if you run it for long enough it will eventually drift. this is a known RK4 limitation.
};

/*--------- Data panel display info ---------*/
struct DataPanelInfo {
    // Body state
    double radius_M     = 0.0;
    double velocity_c   = 0.0;
    double timeDilation  = 0.0;
    double redshift_z    = 0.0;
    double energy_E      = 0.0;
    double angularMomentum_L = 0.0;
    double properTime    = 0.0;
    double coordinateTime = 0.0;
    double escapeVelocity_c = 0.0;
    bool   isBound = true;
    std::string stabilityClass;

    // Tidal
    double tidalForce    = 0.0;
    double tidalStress1m = 0.0;

    // Conservation / error
    double energyDrift    = 0.0;
    double maxEnergyDrift = 0.0;
    double rk4LastError = 0.0;
    double rk4MaxError  = 0.0;
    double rk4AvgError  = 0.0;

    // Precession
    double precessionDeg = 0.0;
    double theoreticalPrecessionDeg = 0.0;
    int    orbitsCompleted = 0;

    // Bondi
    double bondiRadius_M     = 0.0;
    double gasTemperatureK   = 1e7;
    double soundSpeed_c      = 0.0;

    // Additional fields can be added whenever you feel like it. I need ideas tbh.
};
