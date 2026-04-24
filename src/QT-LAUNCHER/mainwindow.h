#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QJsonObject>
#include "custom_bh_dialog.h"

class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QTabWidget;
class QLineEdit;
class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QTimer;
class Simulation2DWidget;
class Simulation3DWidget;
class KeyBindButton;

/**
 * MainWindow - Unified Aetherion Application
 * Manages all windows and simulations as part of a single application instance.
 * All tasks are looped under one umbrella process.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on2DSimulationClicked();
    void on3DSimulationClicked();
    void onWorkspaceSelectionChanged(int row);
    void onToolsSelectionChanged(int row);
    void onRecentFileSelected(QListWidgetItem *item);
    void onRecentItemContextMenu(const QPoint &pos);
    void updateMemoryDisplay();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void setupUI();
    void applyStyles();
    void loadSettings();
    void saveSettings();
    void openSimulationTab2D(const QString &tabTitle, const QJsonObject &state = {});
    void openSimulationTab3D(const QString &tabTitle, const CustomBH3DConfig *cfg = nullptr);
    void updateSimulationEmptyState();
    void refocusSimCanvas();
    QWidget *createOverviewPage();
    QWidget *createSimulationPage();
    QWidget *createDataAnalysisPage();
    QWidget *createObjectLibraryPage();
    QWidget *createExportPage();
    QWidget *createSettingsPage();

    // Workspace persistence
    QString workspacesDir() const;
    void    saveWorkspaceFile(const QString &name, const QString &type,
                              const QJsonObject &simState = {});
    void    populateRecentList();

    // Data analysis
    QString exportsBaseDir() const;

    // Core layout
    QWidget        *centralWidget;
    QListWidget    *workspaceList;
    QListWidget    *toolsList;
    QListWidget    *recentList    = nullptr;
    QStackedWidget *contentStack;
    QTabWidget     *simTabs;
    QLabel         *simEmptyStateLabel;
    QLabel         *statusLabel;
    QLabel         *memoryLabel;
    QTimer         *memoryTimer;
    QPushButton    *sim2DButton;
    QPushButton    *sim3DButton;

    // Settings page widgets (populated in createSettingsPage, read in saveSettings)
    QCheckBox  *showTooltipsCheck  = nullptr;
    QCheckBox  *memoryDisplayCheck = nullptr;
    QLineEdit  *exportFolderEdit   = nullptr;
    QComboBox  *exportTypeCombo    = nullptr;

    // Runtime state loaded from QSettings
    bool memoryDisplayEnabled = true;
};

#endif // MAINWINDOW_H
