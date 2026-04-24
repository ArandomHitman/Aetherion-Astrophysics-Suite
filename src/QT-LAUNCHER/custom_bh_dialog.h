#ifndef CUSTOM_BH_DIALOG_H
#define CUSTOM_BH_DIALOG_H

#include <QDialog>
#include <QJsonObject>

class QDoubleSpinBox;
class QSpinBox;
class QSlider;
class QCheckBox;
class QLineEdit;
class QLabel;
class QTabWidget;
class QGroupBox;

/**
 * CustomBlackHoleDialog
 *
 * A modal dialog that lets the user configure every meaningful parameter
 * of a custom black hole before launching it in either the 2D or 3D
 * simulation widget.
 *
 * Outputs:
 *   - is3D()          — whether the 3D renderer was requested
 *   - tabName()       — user-supplied tab title
 *   - bhMass()        — 2D: geometric mass M (metres, ≥ 1e3)
 *   - toJsonState()   — full 2D state blob for Simulation2DWidget::setPendingState()
 *   - to3DConfig()    — struct usable by Simulation3DWidget::setPendingConfig()
 */

// ── 3D config bundle (mirrors cfg::SimConfig subset, no GL headers needed) ──
struct CustomBH3DConfig {
    // Black hole
    float spinParameter   = 0.8f;   // Kerr a ∈ [0, 1]
    float bhRadius        = 1.0f;

    // Accretion disk
    float diskInnerRadius   = 2.0f;
    float diskOuterRadius   = 20.0f;
    float diskHalfThickness = 0.02f;
    float diskPeakTemp      = 30000.0f;
    float diskTempInner     = 4500.0f;
    float diskTempOuter     = 1800.0f;

    // Jets
    bool  jetsEnabled = false;
    float jetRadius   = 0.35f;
    float jetLength   = 40.0f;

    // BLR
    bool  blrEnabled     = false;
    float blrInnerRadius = 18.0f;
    float blrOuterRadius = 35.0f;

    // Effects
    bool dopplerEnabled  = true;
    bool hostGalaxyEnabled = false;
};

class CustomBlackHoleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomBlackHoleDialog(QWidget *parent = nullptr);

    // ── Query results after exec() == Accepted ──
    bool    is3D()    const;
    QString tabName() const;

    // 2D: returns state blob for Simulation2DWidget::setPendingState()
    QJsonObject toJsonState() const;

    // 3D: returns plain config struct (no GL/SFML headers)
    CustomBH3DConfig to3DConfig() const;

private:
    void buildCommonSection(QWidget *parent);
    void build2DSection(QWidget *parent);
    void build3DSection(QWidget *parent);
    void applyPreviewLabel();

    // ── Common ──
    QLineEdit       *m_nameEdit      = nullptr;
    QCheckBox       *m_3DCheck       = nullptr;

    // ── 2D ──
    QDoubleSpinBox  *m_bhMass2D      = nullptr;   // geometric M (metres)
    QDoubleSpinBox  *m_semiMajor2D   = nullptr;   // orbital semi-major axis [M]
    QDoubleSpinBox  *m_ecc2D         = nullptr;   // eccentricity
    QSpinBox        *m_numRays2D     = nullptr;
    QCheckBox       *m_showRays2D    = nullptr;
    QCheckBox       *m_showPS2D      = nullptr;   // photon sphere

    // ── 3D ──
    QDoubleSpinBox  *m_spin3D        = nullptr;   // Kerr a
    QDoubleSpinBox  *m_diskInner3D   = nullptr;
    QDoubleSpinBox  *m_diskOuter3D   = nullptr;
    QDoubleSpinBox  *m_diskThick3D   = nullptr;
    QDoubleSpinBox  *m_diskTempPeak3D= nullptr;
    QCheckBox       *m_jets3D        = nullptr;
    QDoubleSpinBox  *m_jetLen3D      = nullptr;
    QCheckBox       *m_blr3D         = nullptr;
    QCheckBox       *m_doppler3D     = nullptr;
    QCheckBox       *m_hostGalaxy3D  = nullptr;

    // ── Summary label ──
    QLabel          *m_summaryLabel  = nullptr;

    // ── Convenience helpers ──
    static QGroupBox *makeGroup(const QString &title, QWidget *parent);
};

#endif // CUSTOM_BH_DIALOG_H
