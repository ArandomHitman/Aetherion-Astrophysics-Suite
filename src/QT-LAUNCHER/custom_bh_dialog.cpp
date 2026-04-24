// ============================================================
// custom_bh_dialog.cpp — "Build a Black Hole" configuration dialog
// Lets users tune every meaningful parameter before launching a
// custom simulation tab in either 2D or 3D.
//
// Yes, you can make your own black hole from this dialogue menu
// Karl Schwarzschild died in a war trench to give you these spinboxes.
// ============================================================

#include "custom_bh_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QJsonObject>
#include <QFrame>
#include <QScrollArea>
#include <cmath>

// ─────────────────────────────────────────────────────────────
// Helper — because apparently Qt's spinbox constructor takes 7
// arguments and nobody wanted to type that 15 times.
// ─────────────────────────────────────────────────────────────
static QDoubleSpinBox *makeDSpin(double lo, double hi, double val,
                                  double step, int decimals,
                                  QWidget *parent = nullptr)
{
    auto *s = new QDoubleSpinBox(parent);
    s->setRange(lo, hi);
    s->setValue(val);
    s->setSingleStep(step);
    s->setDecimals(decimals);
    s->setMinimumWidth(120);
    return s;
}

static QSpinBox *makeISpin(int lo, int hi, int val, QWidget *parent = nullptr)
{
    auto *s = new QSpinBox(parent);
    s->setRange(lo, hi);
    s->setValue(val);
    s->setMinimumWidth(100);
    return s;
}

// ─────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────
CustomBlackHoleDialog::CustomBlackHoleDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Build a Custom Black Hole");
    setMinimumWidth(560);
    setMinimumHeight(540);  // tall enough to contain humanity's hubris
    setSizeGripEnabled(true);

    // ── Root layout ──────────────────────────────────────────
    auto *root = new QVBoxLayout(this);
    root->setSpacing(12);
    root->setContentsMargins(18, 18, 18, 14);

    // ── Header ───────────────────────────────────────────────
    auto *headerLabel = new QLabel("Build a Custom Black Hole");
    {
        QFont f = headerLabel->font();
        f.setPointSize(16);
        f.setBold(true);
        headerLabel->setFont(f);
    }
    root->addWidget(headerLabel);

    //Yes, we know a real black hole has zero "settings". Nature doesn't ship a config dialog. We do.
    auto *subLabel = new QLabel(
        "Configure the physical properties of your black hole. "
        "Settings are applied when the simulation tab opens.");
    subLabel->setWordWrap(true);
    subLabel->setObjectName("subtext");
    root->addWidget(subLabel);

    // ── Tab name + renderer row ───────────────────────────────
    {
        auto *row = new QHBoxLayout();
        auto *nameLabel = new QLabel("Tab name:");
        m_nameEdit = new QLineEdit("Custom Black Hole");
        m_nameEdit->setMinimumWidth(220);
        m_3DCheck = new QCheckBox("Use 3D renderer");
        row->addWidget(nameLabel);
        row->addWidget(m_nameEdit, 1);
        row->addSpacing(12);
        row->addWidget(m_3DCheck);
        root->addLayout(row);
    }

    // ── Divider ───────────────────────────────────────────────
    {
        auto *line = new QFrame();
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        root->addWidget(line);
    }

    // ── Parameter tabs ────────────────────────────────────────
    // Two tabs: one for people who want a nice 2D orbit diagram,
    // and one for people who want to argue about their Kerr spin
    auto *tabs = new QTabWidget();

    // 2D tab
    auto *tab2D = new QWidget();
    auto *scroll2D = new QScrollArea();
    scroll2D->setWidgetResizable(true);
    auto *inner2D = new QWidget();
    build2DSection(inner2D);
    scroll2D->setWidget(inner2D);
    auto *lay2D = new QVBoxLayout(tab2D);
    lay2D->setContentsMargins(0,0,0,0);
    lay2D->addWidget(scroll2D);
    tabs->addTab(tab2D, "2D Parameters");

    // 3D tab
    auto *tab3D = new QWidget();
    auto *scroll3D = new QScrollArea();
    scroll3D->setWidgetResizable(true);
    auto *inner3D = new QWidget();
    build3DSection(inner3D);
    scroll3D->setWidget(inner3D);
    auto *lay3D = new QVBoxLayout(tab3D);
    lay3D->setContentsMargins(0,0,0,0);
    lay3D->addWidget(scroll3D);
    tabs->addTab(tab3D, "3D Parameters");

    root->addWidget(tabs, 1);

    // ── Summary label ─────────────────────────────────────────
    m_summaryLabel = new QLabel();
    m_summaryLabel->setObjectName("subtext");
    m_summaryLabel->setWordWrap(true);
    applyPreviewLabel();
    root->addWidget(m_summaryLabel);

    // ── Buttons ───────────────────────────────────────────────
    auto *btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btnBox->button(QDialogButtonBox::Ok)->setText("Launch Simulation");
    btnBox->button(QDialogButtonBox::Ok)->setObjectName("launchButton");
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(btnBox);

    // ── Live summary update ───────────────────────────────────
    auto refresh = [this]{ applyPreviewLabel(); };
    connect(m_bhMass2D,      QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, refresh);
    connect(m_spin3D,        QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, refresh);
    connect(m_diskInner3D,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, refresh);
    connect(m_diskOuter3D,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, refresh);
    connect(m_jets3D,        &QCheckBox::toggled, this, refresh);
    connect(m_blr3D,         &QCheckBox::toggled, this, refresh);
    connect(m_3DCheck,       &QCheckBox::toggled, this, [tabs, refresh](bool v){
        tabs->setCurrentIndex(v ? 1 : 0);
        refresh();
    });
}

// ─────────────────────────────────────────────────────────────
// 2D parameter panel
// ─────────────────────────────────────────────────────────────
void CustomBlackHoleDialog::build2DSection(QWidget *parent)
{
    auto *lay = new QVBoxLayout(parent);
    lay->setSpacing(12);
    lay->setContentsMargins(8, 8, 8, 8);

    // ── Black hole ────────────────────────────────────────────
    auto *grpBH = makeGroup("Black Hole (Schwarzschild)", parent);
    auto *fBH   = new QFormLayout(grpBH);
    fBH->setSpacing(8);

    // fun fact: The range is 1e3 to 1e15 metres. The lower end is a black hole so small it would evaporate before you finish clicking
    m_bhMass2D = makeDSpin(1e3, 1e15, 1e9, 1e7, 2);
    m_bhMass2D->setSuffix(" m (geometric M)");
    m_bhMass2D->setToolTip(
        "Geometric mass M in metres (G=c=1 units).\n"
        "Schwarzschild radius r_s = 2M.\n"
        "Solar mass ≈ 1477 m. Sgr A* ≈ 6.2×10⁹ m.");
    fBH->addRow("Mass M:", m_bhMass2D);
    lay->addWidget(grpBH);

    // ── Orbiting body ─────────────────────────────────────────
    auto *grpOrb = makeGroup("Initial Orbiting Body", parent);
    auto *fOrb   = new QFormLayout(grpOrb);
    fOrb->setSpacing(8);

    m_semiMajor2D = makeDSpin(3.0, 1000.0, 10.0, 1.0, 2);
    m_semiMajor2D->setSuffix("  M");
    m_semiMajor2D->setToolTip("Semi-major axis in multiples of M. Must be > ISCO (6M for Schwarzschild).");
    fOrb->addRow("Semi-major axis:", m_semiMajor2D);

    m_ecc2D = makeDSpin(0.0, 0.99, 0.3, 0.05, 3);
    m_ecc2D->setToolTip("Orbital eccentricity (0 = circular, <1 = bound ellipse).");
    fOrb->addRow("Eccentricity:", m_ecc2D);  // 0 = perfectly circular = physically unrealistic = default for demos
    lay->addWidget(grpOrb);

    // ── Ray tracing ──────────────────────────────────────────
    auto *grpRays = makeGroup("Ray Tracing", parent);
    auto *fRays   = new QFormLayout(grpRays);
    fRays->setSpacing(8);

    m_numRays2D = makeISpin(10, 360, 120);
    m_numRays2D->setToolTip("Number of photon rays cast from the black hole. More = finer lensing detail, higher CPU cost.");
    fRays->addRow("Photon ray count:", m_numRays2D);  // 360 rays: beautiful. 10 rays: impressionist.

    m_showRays2D = new QCheckBox("Show photon ray paths");
    m_showRays2D->setChecked(true);
    fRays->addRow("", m_showRays2D);

    m_showPS2D = new QCheckBox("Show photon sphere (r = 3M)");
    m_showPS2D->setChecked(true);
    fRays->addRow("", m_showPS2D);
    lay->addWidget(grpRays);

    lay->addStretch();
}

// ─────────────────────────────────────────────────────────────
// 3D parameter panel
// ─────────────────────────────────────────────────────────────
void CustomBlackHoleDialog::build3DSection(QWidget *parent)
{
    auto *lay = new QVBoxLayout(parent);
    lay->setSpacing(12);
    lay->setContentsMargins(8, 8, 8, 8);

    // ── Black hole ────────────────────────────────────────────
    auto *grpBH = makeGroup("Black Hole (Kerr)", parent);
    auto *fBH   = new QFormLayout(grpBH);
    fBH->setSpacing(8);

    // Capped at 0.998 because a* = 1 (extremal Kerr) is physically
    // forbidden by the cosmic censorship conjecture. We respect
    // Penrose's conjecture even if we don't fully understand it.
    m_spin3D = makeDSpin(0.0, 0.998, 0.8, 0.05, 3);
    m_spin3D->setToolTip(
        "Dimensionless Kerr spin parameter a* = a/M ∈ [0, 0.998].\n"
        "0 = Schwarzschild (non-rotating), 1 = extremal Kerr.");
    fBH->addRow("Spin parameter a*:", m_spin3D);
    lay->addWidget(grpBH);

    // ── Accretion disk ────────────────────────────────────────
    auto *grpDisk = makeGroup("Accretion Disk", parent);
    auto *fDisk   = new QFormLayout(grpDisk);
    fDisk->setSpacing(8);

    m_diskInner3D = makeDSpin(1.0, 20.0, 2.0, 0.25, 2);
    m_diskInner3D->setSuffix("  Rs");
    m_diskInner3D->setToolTip("Inner disk edge (Schwarzschild radii). Should be near the ISCO.");
    fDisk->addRow("Inner radius:", m_diskInner3D);

    m_diskOuter3D = makeDSpin(5.0, 200.0, 20.0, 1.0, 1);
    m_diskOuter3D->setSuffix("  Rs");
    m_diskOuter3D->setToolTip("Outer disk edge. Higher values create a more extended disk.");
    fDisk->addRow("Outer radius:", m_diskOuter3D);

    m_diskThick3D = makeDSpin(0.001, 0.5, 0.02, 0.005, 3);
    m_diskThick3D->setToolTip(
        "Disk half-thickness as a fraction of radius.\n"
        "Thin disks (0.02) look like real AGN. "
        "Thick disks (0.1+) resemble RIAF/ADAF flows.");
    fDisk->addRow("Half-thickness:", m_diskThick3D);

    m_diskTempPeak3D = makeDSpin(1000.0, 300000.0, 30000.0, 1000.0, 0);
    m_diskTempPeak3D->setSuffix(" K");
    m_diskTempPeak3D->setToolTip(
        "Novikov-Thorne peak temperature of the inner disk.\n"
        "Drives the blackbody colour. Quasars: 20000–100000 K. Sgr A*: ~500 K.");
    fDisk->addRow("Peak temperature:", m_diskTempPeak3D);  // 300,000 K
    lay->addWidget(grpDisk);

    // ── Jets ─────────────────────────────────────────────────
    auto *grpJets = makeGroup("Relativistic Jets", parent);
    auto *fJets   = new QFormLayout(grpJets);
    fJets->setSpacing(8);

    m_jets3D = new QCheckBox("Enable jets");
    m_jets3D->setChecked(false);  // off by default, because relativistic plasma columns are opt-in
    fJets->addRow("", m_jets3D);

    m_jetLen3D = makeDSpin(5.0, 200.0, 40.0, 5.0, 1);
    m_jetLen3D->setSuffix("  Rs");
    m_jetLen3D->setToolTip("Visual jet length. Does not affect the accretion physics.");
    fJets->addRow("Jet length:", m_jetLen3D);
    lay->addWidget(grpJets);

    // ── BLR ──────────────────────────────────────────────────
    auto *grpBLR = makeGroup("Broad Line Region (BLR)", parent);
    auto *fBLR   = new QFormLayout(grpBLR);
    fBLR->setSpacing(8);

    // The Broad Line Region: a torus of gas emitting spectral lines
    // that astronomers argue about. We render it as a blob. Close enough.
    m_blr3D = new QCheckBox("Enable BLR torus");
    m_blr3D->setChecked(false);
    fBLR->addRow("", m_blr3D);
    lay->addWidget(grpBLR);

    // ── Effects ───────────────────────────────────────────────
    auto *grpFx = makeGroup("Relativistic Effects", parent);
    auto *fFx   = new QFormLayout(grpFx);
    fFx->setSpacing(8);

    m_doppler3D = new QCheckBox("Doppler beaming");
    m_doppler3D->setChecked(true);
    m_doppler3D->setToolTip(
        "Relativistic Doppler beaming: brightens the approaching side "
        "of the disk and dims the receding side.");
    fFx->addRow("", m_doppler3D);

    m_hostGalaxy3D = new QCheckBox("Show host galaxy");
    m_hostGalaxy3D->setChecked(false);
    fFx->addRow("", m_hostGalaxy3D);
    lay->addWidget(grpFx);

    lay->addStretch();
}

// ─────────────────────────────────────────────────────────────
// Live summary — updates on every spinbox twitch because the user
// apparently needs real-time reassurance that their black hole
// exists. It does. Probably.
// ─────────────────────────────────────────────────────────────
void CustomBlackHoleDialog::applyPreviewLabel()
{
    if (!m_summaryLabel) return;

    const bool use3D = m_3DCheck && m_3DCheck->isChecked();
    QString txt;
    if (use3D) {
        const double a = m_spin3D ? m_spin3D->value() : 0.8;
        const double ri = m_diskInner3D ? m_diskInner3D->value() : 2.0;
        const double ro = m_diskOuter3D ? m_diskOuter3D->value() : 20.0;
        const bool jets = m_jets3D && m_jets3D->isChecked();
        const bool blr  = m_blr3D  && m_blr3D->isChecked();
        txt = QString("3D renderer — Kerr spin a*=%1 | disk %2–%3 Rs%4%5")
                .arg(a, 0, 'f', 3)
                .arg(ri, 0, 'f', 1)
                .arg(ro, 0, 'f', 1)
                .arg(jets ? " | jets ON" : "")
                .arg(blr  ? " | BLR ON"  : "");
    } else {
        const double M  = m_bhMass2D ? m_bhMass2D->value() : 1e9;
        const double rs = 2.0 * M;
        const double a  = m_semiMajor2D ? m_semiMajor2D->value() : 10.0;
        const double e  = m_ecc2D ? m_ecc2D->value() : 0.3;
        txt = QString("2D renderer — r_s = %1 m | orbit: a=%2 M, e=%3")
                .arg(rs, 0, 'e', 3)
                .arg(a,  0, 'f', 2)
                .arg(e,  0, 'f', 3);
    }
    m_summaryLabel->setText(txt);
}

// ─────────────────────────────────────────────────────────────
// Result accessors
// ─────────────────────────────────────────────────────────────
bool CustomBlackHoleDialog::is3D() const
{
    return m_3DCheck && m_3DCheck->isChecked();
}

QString CustomBlackHoleDialog::tabName() const
{
    if (m_nameEdit) {
        const QString t = m_nameEdit->text().trimmed();
        if (!t.isEmpty()) return t;
    }
    return "Custom Black Hole";
}

QJsonObject CustomBlackHoleDialog::toJsonState() const
{
    QJsonObject o;

    const double M   = m_bhMass2D    ? m_bhMass2D->value()    : 1e9;
    const double a   = m_semiMajor2D ? m_semiMajor2D->value() : 10.0;
    const double ecc = m_ecc2D       ? m_ecc2D->value()       : 0.3;
    const int rays   = m_numRays2D   ? m_numRays2D->value()   : 120;

    // Scale pixelsPerM so the Schwarzschild horizon always appears 120 px wide,
    // matching the preset zoom convention: pixelsPerM = horizonTarget / (2*M).
    // Yes, we hard-coded 120. No, we're not sorry.
    const double pixelsPerM = 120.0 / (2.0 * M);

    // Scale rMaxIntegrate with M: photon paths must reach the orbit and beyond.
    // 200 * a * M gives comfortable headroom past the semi-major axis.
    const double rMaxIntegrate = 200.0 * a * M;

    o["bhMass"]          = M;
    o["numRays"]         = rays;
    o["pixelsPerM"]      = pixelsPerM;
    o["rMaxIntegrate"]   = rMaxIntegrate;
    o["galaxySystemActive"] = false;
    o["activePresetIdx"] = -1;

    // Start in manual (non-preset) mode
    o["presetActive"]    = false;
    o["presetIdx"]       = 0;
    o["timeScale"]       = 1.0;
    o["showRays"]        = (m_showRays2D ? m_showRays2D->isChecked() : true);
    o["showPhotonSphere"]= (m_showPS2D   ? m_showPS2D->isChecked()   : true);
    o["showInfluenceZones"] = false;
    o["showGalaxySystem"]   = false;
    o["paused"]          = false;

    // Build initial circular orbit at the requested semi-major axis.
    // For a Schwarzschild metric M, L for circular orbit at r = a*M:
    // L = sqrt(M * r^2 / (r - 3M))  (Schwarzschild circular orbit formula)
    const double r = a * M;                          // orbital radius in metres
    const double denom = r - 3.0 * M;
    double L = 0.0, E = 1.0;
    if (denom > 0.0) {
        L = std::sqrt(M * r * r / denom);
        const double f = 1.0 - 2.0 * M / r;
        E = std::sqrt(std::max(0.0, f * f / (1.0 - 3.0 * M / r)));
    }

    o["body0_a"]   = r;   // nominalA is in absolute metres, not multiples of M
    o["body0_ecc"] = ecc;
    o["body0_r"]   = r;
    o["body0_phi"] = 0.0;
    o["body0_vr"]  = 0.0;
    o["body0_E"]   = E;
    o["body0_L"]   = L;

    return o;
}

CustomBH3DConfig CustomBlackHoleDialog::to3DConfig() const
{
    CustomBH3DConfig cfg;
    cfg.spinParameter     = static_cast<float>(m_spin3D      ? m_spin3D->value()      : 0.8);
    cfg.diskInnerRadius   = static_cast<float>(m_diskInner3D ? m_diskInner3D->value() : 2.0);
    cfg.diskOuterRadius   = static_cast<float>(m_diskOuter3D ? m_diskOuter3D->value() : 20.0);
    cfg.diskHalfThickness = static_cast<float>(m_diskThick3D ? m_diskThick3D->value() : 0.02);
    cfg.diskPeakTemp      = static_cast<float>(m_diskTempPeak3D ? m_diskTempPeak3D->value() : 30000.0);
    cfg.jetsEnabled       = m_jets3D      && m_jets3D->isChecked();
    cfg.jetLength         = static_cast<float>(m_jetLen3D ? m_jetLen3D->value() : 40.0);
    cfg.blrEnabled        = m_blr3D       && m_blr3D->isChecked();
    cfg.dopplerEnabled    = m_doppler3D   && m_doppler3D->isChecked();
    cfg.hostGalaxyEnabled = m_hostGalaxy3D && m_hostGalaxy3D->isChecked();
    return cfg;
}

// ─────────────────────────────────────────────────────────────
// Static helper — wraps content in a bold-titled box so it looks
// like we put serious thought into the UI layout. We did. Mostly.
// ─────────────────────────────────────────────────────────────
QGroupBox *CustomBlackHoleDialog::makeGroup(const QString &title, QWidget *parent)
{
    auto *g = new QGroupBox(title, parent);
    g->setStyleSheet("QGroupBox { font-weight: bold; }");
    return g;
}
