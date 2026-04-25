
/* ---------- TO DO: please for the love of god implement a header file for this stuff. It saves lives ---------- */
#include "mainwindow.h"
#include "simulation_2d_widget.h"
#include "simulation_3d_widget.h"
#include "keybindbuttonwidget.h"
#include "custom_bh_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QApplication>
#include <QScreen>
#include <QFont>
#include <QTimer>
#include <QSplitter>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStackedWidget>
#include <QTabWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPalette>
#include <QStyleFactory>
#include <QSettings>
#include <QShortcut>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QGridLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QImage>
#include <QPixmap>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDialog>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QSplitter>
#include <QPainter>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QScatterSeries>
#include <QValueAxis>
#include <QtEndian>

// Returns the Aetherion app-data root, matching simulation.hpp's makeExportDir().
// Must agree with the C++ getenv("HOME") path used in simulation code.
static QString aetherionDataDir() {
#ifdef Q_OS_MACOS
    return QDir::homePath() + "/Library/Application Support/Aetherion";
#else
    return QDir::homePath() + "/.local/share/Aetherion";
#endif
}

#if defined(Q_OS_MAC)
#  include <mach/mach.h> // Thanks, Tim Apple....
static qint64 processResidentKB() {
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
        return static_cast<qint64>(info.resident_size / 1024);
    return -1; // query failed — caller will show "N/A", no reason to nuke the whole app
}

#elif defined(Q_OS_LINUX)
#  include <fstream>
static qint64 processResidentKB() {
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("VmRSS:", 0) == 0)
            return std::stoll(line.substr(6));
    return -1;
}
#else
static qint64 processResidentKB() { return -1; }
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      simTabs(nullptr),
      simEmptyStateLabel(nullptr),
      memoryLabel(nullptr),
      memoryTimer(nullptr)
{
    setWindowTitle("Aetherion Astrophysics Suite");
    // Best-effort window icon — exe-adjacent icon.png is POST_BUILD copied by CMake.
    {
        QStringList iconCandidates = {
            QApplication::applicationDirPath() + "/icon.png",
            QApplication::applicationDirPath() + "/../icon.png",
#ifdef Q_OS_MACOS
            QApplication::applicationDirPath() + "/../Resources/icon.png",
#endif
            QString("icon.png"), QString("../icon.png")
        };
        for (const QString &ic : iconCandidates) {
            if (QFile::exists(ic)) { setWindowIcon(QIcon(ic)); break; }
        }
    }

    resize(1200, 800);

    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }

    loadSettings();   // populate memoryDisplayEnabled + any saved prefs
    setupUI();
    applyStyles();

    // Intercept key events at the application level so that Cmd+Q (and F11)
    // work even when the native SFML NSView has OS-level keyboard focus.
    qApp->installEventFilter(this);

    // Start memory polling timer (fires every 2 s)
    memoryTimer = new QTimer(this);
    connect(memoryTimer, &QTimer::timeout, this, &MainWindow::updateMemoryDisplay);
    if (memoryDisplayEnabled) {
        memoryTimer->start(2000);
        updateMemoryDisplay();
    }

    // Auto-update check: runs 4 s after startup so it never delays the UI.
    // Respects the "auto-check" preference stored in QSettings.
    m_updater = new Updater(this);
    {
        QSettings s("Aetherion", "AetherionSuite");
        if (s.value("updates/autoCheck", true).toBool()) {
            QTimer::singleShot(4000, m_updater, [this]() {
                m_updater->checkForUpdates(/*silent=*/true);
            });
        }

        // If we just relaunched after an automatic update, show the changelog
        // once the window is fully visible (slight delay avoids drawing over
        // the startup animation).
        const QString postTag   = s.value("updates/postUpdateTag").toString();
        const QString postNotes = s.value("updates/postUpdateChangelog").toString();
        if (!postTag.isEmpty()) {
            s.remove("updates/postUpdateTag");
            s.remove("updates/postUpdateChangelog");
            s.sync();

            QTimer::singleShot(800, this, [this, postTag, postNotes]() {
                QDialog *dlg = new QDialog(this);
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->setWindowTitle(QStringLiteral("Updated to %1").arg(postTag));
                dlg->setMinimumSize(520, 380);

                auto *layout = new QVBoxLayout(dlg);
                layout->setSpacing(12);
                layout->setContentsMargins(16, 16, 16, 12);

                auto *heading = new QLabel(
                    QStringLiteral("<b>Aetherion has been updated to %1</b>").arg(postTag), dlg);
                heading->setTextFormat(Qt::RichText);
                layout->addWidget(heading);

                auto *notes = new QTextEdit(dlg);
                notes->setReadOnly(true);
                notes->setPlainText(postNotes.isEmpty()
                    ? QStringLiteral("No release notes provided.")
                    : postNotes);
                notes->setFrameShape(QFrame::StyledPanel);
                layout->addWidget(notes, /*stretch=*/1);

                auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok, dlg);
                connect(btnBox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
                layout->addWidget(btnBox);

                dlg->show();
            });
        }
    }
}

void MainWindow::checkForUpdatesManual()
{
    if (m_updater)
        m_updater->checkForUpdates(/*silent=*/false);
}

MainWindow::~MainWindow()
{
    // Uninstall the global event filter before destruction to prevent
    // dangling filter reference if QApplication outlives this window.
    qApp->removeEventFilter(this);

    // All child widgets (including simulation widgets) are automatically cleaned up
    // This ensures a graceful shutdown of all simulations tied to this main instance
}

// ---------------------------------------------------------------------------
// eventFilter — application-level intercept for global shortcuts.
// Handles Cmd+Q (quit) and F11 (fullscreen toggle) even when the SFML canvas's
// native NSView holds OS-level keyboard focus and bypasses Qt's normal chain.
// ---------------------------------------------------------------------------
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);

        // Cmd+Q → quit (mirrors the application menu action)
        if (ke->matches(QKeySequence::Quit)) {
            QApplication::quit();
            return true;
        }

        // F11 → toggle fullscreen (backup path; QShortcut handles primary)
        if (ke->key() == Qt::Key_F11 && ke->modifiers() == Qt::NoModifier) {
            if (isFullScreen()) showNormal(); else showFullScreen();
            QTimer::singleShot(50, this, [this] { refocusSimCanvas(); });
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

// ---------------------------------------------------------------------------
// changeEvent — restore SFML canvas focus whenever the window state changes
// (e.g. entering/leaving fullscreen via the macOS green traffic-light button).
// ---------------------------------------------------------------------------
void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
        QTimer::singleShot(50, this, [this] { refocusSimCanvas(); });
}

// ---------------------------------------------------------------------------
// refocusSimCanvas — gives keyboard focus to the active simulation tab so
// that SFML key events are delivered after any window-state transition.
// ---------------------------------------------------------------------------
void MainWindow::refocusSimCanvas()
{
    if (!simTabs || !simTabs->isVisible()) return;
    QWidget *w = simTabs->currentWidget();
    if (w) w->setFocus(Qt::OtherFocusReason);
}

void MainWindow::loadSettings()
{
    QSettings s("Aetherion", "AetherionSuite");
    memoryDisplayEnabled = s.value("ui/memoryDisplay", true).toBool();
}

void MainWindow::saveSettings()
{
    QSettings s("Aetherion", "AetherionSuite");

    if (memoryDisplayCheck)
        s.setValue("ui/memoryDisplay", memoryDisplayCheck->isChecked());
    if (showTooltipsCheck)
        s.setValue("ui/showTooltips", showTooltipsCheck->isChecked());
    if (exportFolderEdit)
        s.setValue("export/folder", exportFolderEdit->text());
    if (exportTypeCombo)
        s.setValue("export/type", exportTypeCombo->currentText());

    // Apply memory display toggle live
    if (memoryDisplayCheck) {
        memoryDisplayEnabled = memoryDisplayCheck->isChecked();
        if (memoryLabel) memoryLabel->setVisible(memoryDisplayEnabled);
        if (memoryTimer) {
            if (memoryDisplayEnabled) { memoryTimer->start(2000); updateMemoryDisplay(); }
            else                        memoryTimer->stop();
        }
    }

    statusLabel->setText("Settings saved");
}

void MainWindow::updateMemoryDisplay()
{
    if (!memoryLabel || !memoryDisplayEnabled) return;
    const qint64 kb = processResidentKB();
    if (kb < 0) {
        memoryLabel->setText("Mem: N/A");
        return;
    }
    if (kb >= 1024 * 1024)
        memoryLabel->setText(QString("Mem: %1 GB").arg(double(kb) / (1024.0 * 1024.0), 0, 'f', 2));
    else if (kb >= 1024)
        memoryLabel->setText(QString("Mem: %1 MB").arg(double(kb) / 1024.0, 0, 'f', 1));
    else
        memoryLabel->setText(QString("Mem: %1 KB").arg(kb));
}

void MainWindow::setupUI()
{
    // F11 toggles full-screen. ApplicationShortcut fires regardless of which
    // native child view (SFML canvas) currently holds keyboard focus.
    auto *fsShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    fsShortcut->setContext(Qt::ApplicationShortcut);
    connect(fsShortcut, &QShortcut::activated, this, [this] {
        if (isFullScreen()) showNormal();
        else showFullScreen();
        // Restore keyboard focus to the active simulation canvas after the
        // window-state change (which resets Qt's focus on macOS).
        QTimer::singleShot(50, this, [this] { refocusSimCanvas(); });
    });

    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // ============ LEFT SIDEBAR ============
    QWidget *sidebar = new QWidget();
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(280);
    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(20, 20, 20, 20);
    sidebarLayout->setSpacing(0);

    // ── Logo + title block ────────────────────────────────────────────────────
    {
        // Resolve icon.png from the exe directory (POST_BUILD copied) or project root.
        auto findLogoPixmap = []() -> QPixmap {
            // Prefer the pre-baked transparent version; fall back to the plain icon.
            const QString appDir = QApplication::applicationDirPath();
            QStringList candidates = {
                appDir + "/icon_transparent.png",
                appDir + "/../icon_transparent.png",
#ifdef Q_OS_MACOS
                // Inside a .app bundle: Contents/MacOS/../Resources
                appDir + "/../Resources/icon_transparent.png",
#endif
                appDir + "/../../icon_transparent.png",
                QString("icon_transparent.png"),
                QString("../icon_transparent.png"),
                // plain icon fallback (rendered with matching bg colour)
                appDir + "/icon.png",
                appDir + "/../icon.png",
#ifdef Q_OS_MACOS
                appDir + "/../Resources/icon.png",
#endif
                QString("icon.png"), QString("../icon.png")
            };
            for (const QString &p : candidates) {
                if (!QFile::exists(p)) continue;
                QPixmap pm(p);
                if (!pm.isNull()) return pm;
            }
            return {};
        };

        QWidget *logoRow = new QWidget();
        logoRow->setAttribute(Qt::WA_NoSystemBackground);
        logoRow->setAttribute(Qt::WA_TransparentForMouseEvents);
        QHBoxLayout *logoHL = new QHBoxLayout(logoRow);
        logoHL->setContentsMargins(0, 0, 0, 6);
        logoHL->setSpacing(11);

        const QPixmap logo = findLogoPixmap();
        if (!logo.isNull()) {
            QLabel *logoLabel = new QLabel();
            logoLabel->setAttribute(Qt::WA_NoSystemBackground);
            logoLabel->setPixmap(logo.scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            logoLabel->setFixedSize(44, 44);
            logoHL->addWidget(logoLabel);
        }

        QVBoxLayout *titleVBox = new QVBoxLayout();
        titleVBox->setContentsMargins(0, 0, 0, 0);
        titleVBox->setSpacing(2);

        QLabel *titleLabel = new QLabel("Aetherion");
        QFont titleFont;
        titleFont.setPointSize(16);
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        titleVBox->addWidget(titleLabel);

        QLabel *subtitleLabel = new QLabel("ASTROPHYSICS SUITE");
        QFont subtitleFont;
        subtitleFont.setPointSize(7);
        subtitleLabel->setFont(subtitleFont);
        subtitleLabel->setObjectName("subtitle");
        titleVBox->addWidget(subtitleLabel);

        logoHL->addLayout(titleVBox);
        logoHL->addStretch();
        sidebarLayout->addWidget(logoRow);
    }

    sidebarLayout->addSpacing(20);
    
    QLabel *workspaceLabel = new QLabel("WORKSPACE");
    workspaceLabel->setObjectName("sectionHeader");
    sidebarLayout->addWidget(workspaceLabel);
    
    workspaceList = new QListWidget();
    workspaceList->setMaximumHeight(140);
    workspaceList->setSelectionMode(QAbstractItemView::SingleSelection);
    workspaceList->addItem("Overview");
    workspaceList->addItem("Simulation");
    workspaceList->addItem("Data Analysis");
    workspaceList->addItem("Object Library");
    workspaceList->setCurrentRow(0);
    sidebarLayout->addWidget(workspaceList);
    
    sidebarLayout->addSpacing(20);
    
    QLabel *toolsLabel = new QLabel("TOOLS");
    toolsLabel->setObjectName("sectionHeader");
    sidebarLayout->addWidget(toolsLabel);
    
    toolsList = new QListWidget();
    toolsList->setMaximumHeight(80);
    toolsList->addItem("Export");
    toolsList->addItem("Settings");
    sidebarLayout->addWidget(toolsList);
    
    sidebarLayout->addStretch();
    QLabel *versionLabel = new QLabel("v0.4.2 • open source");
    versionLabel->setObjectName("footer");
    versionLabel->setAlignment(Qt::AlignCenter);
    sidebarLayout->addWidget(versionLabel);
    
    mainLayout->addWidget(sidebar);
    
    // ============ MAIN CONTENT AREA ============
    QWidget *contentArea = new QWidget();
    QVBoxLayout *contentAreaLayout = new QVBoxLayout(contentArea);
    contentAreaLayout->setContentsMargins(30, 25, 30, 0);
    contentAreaLayout->setSpacing(0);

    contentStack = new QStackedWidget();
    contentStack->addWidget(createOverviewPage());
    contentStack->addWidget(createSimulationPage());
    contentStack->addWidget(createDataAnalysisPage());
    contentStack->addWidget(createObjectLibraryPage());
    contentStack->addWidget(createExportPage());
    contentStack->addWidget(createSettingsPage());
    contentAreaLayout->addWidget(contentStack, 1);

    statusLabel = new QLabel("ready");
    statusLabel->setObjectName("status");
    statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusLabel->setMinimumHeight(28);

    memoryLabel = new QLabel();
    memoryLabel->setObjectName("memoryLabel");
    memoryLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    memoryLabel->setMinimumHeight(28);
    memoryLabel->setVisible(memoryDisplayEnabled);

    QWidget *statusBar = new QWidget();
    QHBoxLayout *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(0);
    statusLayout->addWidget(statusLabel, 1);
    statusLayout->addWidget(memoryLabel);
    contentAreaLayout->addWidget(statusBar);

    mainLayout->addWidget(contentArea, 1);
    
    connect(workspaceList, &QListWidget::currentRowChanged,
            this, &MainWindow::onWorkspaceSelectionChanged);
    connect(toolsList, &QListWidget::currentRowChanged,
            this, &MainWindow::onToolsSelectionChanged);
}

void MainWindow::applyStyles()
{
    qApp->setStyle(QStyleFactory::create("Fusion"));
    
    QPalette darkPalette;
    
    // Background colors
    darkPalette.setColor(QPalette::Window, QColor(13, 13, 18));
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 35));
    darkPalette.setColor(QPalette::AlternateBase, QColor(20, 20, 30));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(50, 50, 70));
    darkPalette.setColor(QPalette::ToolTipText, QColor(238, 238, 245));
    
    // Text colors
    darkPalette.setColor(QPalette::Text, QColor(237, 237, 247));
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(140, 140, 160));
    darkPalette.setColor(QPalette::ButtonText, QColor(237, 237, 247));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(140, 140, 160));
    
    // Button colors
    darkPalette.setColor(QPalette::Button, QColor(30, 30, 42));
    darkPalette.setColor(QPalette::Highlight, QColor(38, 115, 192));
    darkPalette.setColor(QPalette::HighlightedText, QColor(237, 237, 247));
    
    // Link colors
    darkPalette.setColor(QPalette::Link, QColor(51, 153, 255));
    darkPalette.setColor(QPalette::LinkVisited, QColor(51, 153, 255));
    
    qApp->setPalette(darkPalette);
    
    // Custom stylesheet
    QString styleSheet = R"(
        QMainWindow {
            background-color: #0d0d12;
        }
        
        QWidget {
            background-color: #0d0d12;
            color: #ededed;
        }
        
        QWidget#sidebar {
            background-color: #0a0a0f;
            border-right: 1px solid #40404f;
        }
        
        QLabel#subtitle {
            color: #8a8a9a;
            font-weight: normal;
        }
        
        QLabel#sectionHeader {
            color: #8a8a9a;
            font-size: 9pt;
            font-weight: bold;
            margin-top: 10px;
        }
        
        QLabel#subtext {
            color: #a0a0b0;
            font-size: 11pt;
            margin-bottom: 15px;
        }
        
        QLabel#modeNumber {
            color: #8a8a9a;
            font-size: 9pt;
        }
        
        QLabel#description {
            color: #a0a0b0;
            font-size: 10pt;
            line-height: 1.4;
        }
        
        QLabel#footer {
            color: #5a5a6a;
            font-size: 9pt;
        }
        
        QLabel#status {
            color: #4dd995;
            padding: 0 6px;
        }

        QLabel#memoryLabel {
            color: #617090;
            padding: 0 10px 0 0;
            font-size: 9pt;
        }

        QFrame#modeCard {
            background-color: #0f0f18;
            border: 1px solid #3d4d65;
            border-radius: 6px;
            padding: 15px;
        }
        
        QFrame#modeCard:hover {
            background-color: #151520;
            border: 1px solid #4d6d8f;
        }
        
        QPushButton#launchButton {
            background-color: #2669bb;
            color: #ededed;
            border: none;
            border-radius: 4px;
            padding: 8px;
            font-weight: bold;
            font-size: 11pt;
        }
        
        QPushButton#launchButton:hover {
            background-color: #3d7acc;
        }
        
        QPushButton#launchButton:pressed {
            background-color: #1f5aa0;
        }
        
        QListWidget {
            background-color: #0a0a0f;
            border: none;
            outline: none;
        }
        
        QListWidget::item {
            padding: 6px 10px;
            border-radius: 3px;
        }
        
        QListWidget::item:selected {
            background-color: #2669bb;
        }
        
        QListWidget::item:hover {
            background-color: #1a1a26;
        }
        
        QScrollBar:vertical {
            background-color: #0a0a0f;
            width: 12px;
            border: none;
        }
        
        QScrollBar::handle:vertical {
            background-color: #40404f;
            border-radius: 6px;
            min-height: 20px;
        }
        
        QScrollBar::handle:vertical:hover {
            background-color: #50505f;
        }
        
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            border: none;
            background: none;
        }

        QScrollBar:horizontal {
            background-color: #0a0a0f;
            height: 10px;
            border: none;
        }

        QScrollBar::handle:horizontal {
            background-color: #3a3a4e;
            border-radius: 5px;
            min-width: 20px;
        }

        QScrollBar::handle:horizontal:hover { background-color: #50505f; }

        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            border: none;
            background: none;
        }

        QLineEdit {
            background-color: #0f0f18;
            border: 1px solid #2a2a40;
            border-radius: 4px;
            color: #d8ddf0;
            padding: 4px 8px;
            selection-background-color: #2669bb;
        }

        QLineEdit:focus { border-color: #3d6eb8; }

        QComboBox {
            background-color: #0f0f18;
            border: 1px solid #2a2a40;
            border-radius: 4px;
            color: #d8ddf0;
            padding: 3px 8px;
            selection-background-color: #2669bb;
        }

        QComboBox:focus { border-color: #3d6eb8; }

        QComboBox::drop-down {
            border: none;
            width: 22px;
        }

        QComboBox QAbstractItemView {
            background-color: #141421;
            border: 1px solid #2a2a40;
            color: #d8ddf0;
            selection-background-color: #2669bb;
            outline: none;
        }

        QCheckBox {
            color: #c8cce0;
            spacing: 8px;
        }

        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            border: 1px solid #383858;
            border-radius: 3px;
            background-color: #0f0f18;
        }

        QCheckBox::indicator:checked {
            background-color: #2669bb;
            border-color: #3d70bb;
        }

        QCheckBox::indicator:hover { border-color: #5577aa; }

        QPushButton {
            background-color: #181828;
            color: #c0c8e0;
            border: 1px solid #2e2e46;
            border-radius: 4px;
            padding: 5px 14px;
        }

        QPushButton:hover {
            background-color: #20203a;
            border-color: #4055a0;
            color: #d8e0f8;
        }

        QPushButton:pressed { background-color: #10101e; }

        QPushButton:disabled {
            color: #484860;
            border-color: #222235;
        }

        QSplitter::handle { background-color: #222232; }
        QSplitter::handle:horizontal { width: 1px; }
        QSplitter::handle:vertical   { height: 1px; }
    )";
    
    qApp->setStyleSheet(styleSheet);
}

QWidget* MainWindow::createOverviewPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    QLabel *headerLabel = new QLabel("Overview");
    QFont headerFont;
    headerFont.setPointSize(20);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    layout->addWidget(headerLabel);

    QLabel *subtitleLabel = new QLabel("Start here, browse recent progress, or pick a mode.");
    subtitleLabel->setObjectName("subtext");
    layout->addWidget(subtitleLabel);

    QHBoxLayout *cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(20);

    auto createCard = [&](const QString &mode, const QString &title, const QString &description, const QString &targetExec) {
        QFrame *card = new QFrame();
        card->setObjectName("modeCard");
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(15, 15, 15, 15);

        QLabel *modeLabel = new QLabel(mode);
        modeLabel->setObjectName("modeNumber");
        cardLayout->addWidget(modeLabel);

        QLabel *titleLabel = new QLabel(title);
        QFont titleFont;
        titleFont.setPointSize(12);
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        cardLayout->addWidget(titleLabel);

        QLabel *descLabel = new QLabel(description);
        descLabel->setWordWrap(true);
        descLabel->setObjectName("description");
        cardLayout->addWidget(descLabel);

        cardLayout->addStretch();
        QPushButton *button = new QPushButton("LAUNCH");
        button->setMinimumHeight(40);
        button->setObjectName("launchButton");
        if (targetExec == "blackhole-2D")
            connect(button, &QPushButton::clicked, this, &MainWindow::on2DSimulationClicked);
        else
            connect(button, &QPushButton::clicked, this, &MainWindow::on3DSimulationClicked);

        cardLayout->addWidget(button);
        return card;
    };

    cardsLayout->addWidget(createCard("MODE 01", "2D Research Mode",
        "Advanced calculations, data compilation, scientific analysis, BLR simulation",
        "blackhole-2D"));
    cardsLayout->addWidget(createCard("MODE 02", "3D Visualization Mode",
        "Real-time rendering, visual exploration, gravitational lensing preview",
        "blackhole-3D"));

    layout->addLayout(cardsLayout);

    QLabel *recentHeader = new QLabel("Recently used");
    recentHeader->setObjectName("sectionHeader");
    layout->addWidget(recentHeader);
    QLabel *recentText = new QLabel("Resume progress files from your last session.");
    recentText->setObjectName("subtext");
    layout->addWidget(recentText);

    recentList = new QListWidget();
    recentList->setMinimumHeight(80);
    recentList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(recentList, &QListWidget::itemClicked,
            this, &MainWindow::onRecentFileSelected);
    connect(recentList, &QListWidget::customContextMenuRequested,
            this, &MainWindow::onRecentItemContextMenu);
    layout->addWidget(recentList);

    // Populate from disk now that the widget exists
    populateRecentList();

    layout->addStretch();
    return page;
}

QWidget* MainWindow::createSimulationPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QLabel *headerLabel = new QLabel("Simulation");
    QFont headerFont;
    headerFont.setPointSize(20);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    layout->addWidget(headerLabel);

    QLabel *subtitleLabel = new QLabel("No simulation is loaded yet. Open a tab to begin.");
    subtitleLabel->setObjectName("subtext");
    layout->addWidget(subtitleLabel);

    QHBoxLayout *actionsRow = new QHBoxLayout();
    QPushButton *open2DButton = new QPushButton("Open 2D Program");
    QPushButton *open3DButton = new QPushButton("Open 3D Program");
    actionsRow->addWidget(open2DButton);
    actionsRow->addWidget(open3DButton);
    layout->addLayout(actionsRow);

    QHBoxLayout *presetRow = new QHBoxLayout();
    QComboBox *presetCombo = new QComboBox();
    presetCombo->addItems({"TON 618 (3D)", "Sgr A* (2D)", "Micro BH (2D)", "Rapid Spin (3D)"});
    QPushButton *openPresetButton = new QPushButton("Open Preset Tab");
    presetRow->addWidget(presetCombo, 1);
    presetRow->addWidget(openPresetButton);
    layout->addLayout(presetRow);

    QHBoxLayout *customRow = new QHBoxLayout();
    QPushButton *openCustomButton = new QPushButton("Build Custom Black Hole…");
    openCustomButton->setObjectName("launchButton");
    openCustomButton->setToolTip(
        "Opens a dialog to configure every parameter of a custom black hole "
        "and launch it in either the 2D or 3D simulation.");
    customRow->addStretch();
    customRow->addWidget(openCustomButton);
    layout->addLayout(customRow);

    simTabs = new QTabWidget();
    simTabs->setTabsClosable(true);
    simTabs->setMovable(true);
    simTabs->setStyleSheet(R"(
        QTabWidget::pane { border: none; }
        QTabBar::tab {
            background-color: #1a1a26;
            color: #ededed;
            padding: 8px 15px;
            border: none;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: #2669bb;
        }
        QTabBar::tab:hover {
            background-color: #242430;
        }
    )");

    simEmptyStateLabel = new QLabel("No open simulation tabs. Use controls above to open 2D, 3D, a preset, or custom simulation.");
    simEmptyStateLabel->setObjectName("description");
    simEmptyStateLabel->setAlignment(Qt::AlignCenter);
    simEmptyStateLabel->setMinimumHeight(220);

    layout->addWidget(simEmptyStateLabel);
    layout->addWidget(simTabs);
    updateSimulationEmptyState();

    connect(open2DButton, &QPushButton::clicked, this, [this] {
        openSimulationTab2D("2D Research");
        statusLabel->setText("Opened 2D simulation tab");
    });
    connect(open3DButton, &QPushButton::clicked, this, [this] {
        openSimulationTab3D("3D Visualization");
        statusLabel->setText("Opened 3D simulation tab");
    });
    connect(openPresetButton, &QPushButton::clicked, this, [this, presetCombo] {
        const QString preset = presetCombo->currentText();
        const QString presetName = preset.section(" (", 0, 0);
        if (preset.contains("(3D)")) {
            openSimulationTab3D("Preset: " + presetName);
        } else {
            openSimulationTab2D("Preset: " + presetName);
        }
        statusLabel->setText("Opened preset tab: " + presetName);
    });
    connect(openCustomButton, &QPushButton::clicked, this, [this] {
        CustomBlackHoleDialog dlg(this);
        if (dlg.exec() != QDialog::Accepted) return;

        const QString tabTitle = dlg.tabName();
        if (dlg.is3D()) {
            const CustomBH3DConfig cfg = dlg.to3DConfig();
            openSimulationTab3D(tabTitle, &cfg);
        } else {
            openSimulationTab2D(tabTitle, dlg.toJsonState());
        }
        statusLabel->setText("Opened custom tab: " + tabTitle);
    });
    connect(simTabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        QWidget *w = simTabs->widget(index);
        const QString tabTitle = simTabs->tabText(index);
        const QString simType  = qobject_cast<Simulation3DWidget*>(w) ? "3D" : "2D";

        // ── Ask whether to save before closing ──────────────────────────────
        QDialog dlg(this);
        dlg.setWindowTitle("Save Progress");
        dlg.setFixedWidth(420);

        QVBoxLayout *lay = new QVBoxLayout(&dlg);
        lay->setSpacing(12);
        lay->setContentsMargins(20, 20, 20, 20);

        QLabel *msg = new QLabel(
            "Save progress before closing <b>" + tabTitle.toHtmlEscaped() + "</b>?");
        msg->setWordWrap(true);
        lay->addWidget(msg);

        QLineEdit *nameEdit = new QLineEdit(tabTitle);
        nameEdit->setPlaceholderText("Workspace name");
        nameEdit->selectAll();
        lay->addWidget(nameEdit);

        QHBoxLayout *btnRow = new QHBoxLayout();
        QPushButton *cancelBtn   = new QPushButton("Cancel");
        QPushButton *dontSaveBtn = new QPushButton("Close Without Saving");
        QPushButton *saveBtn     = new QPushButton("Save && Close");
        saveBtn->setObjectName("launchButton");
        btnRow->addWidget(cancelBtn);
        btnRow->addStretch();
        btnRow->addWidget(dontSaveBtn);
        btnRow->addWidget(saveBtn);
        lay->addLayout(btnRow);

        connect(saveBtn,     &QPushButton::clicked, &dlg, &QDialog::accept);
        connect(dontSaveBtn, &QPushButton::clicked, &dlg, [&dlg]{ dlg.done(2); });
        connect(cancelBtn,   &QPushButton::clicked, &dlg, &QDialog::reject);

        const int result = dlg.exec();
        if (result == QDialog::Rejected) return;   // Cancel — leave tab open

        if (result == QDialog::Accepted) {
            QString name = nameEdit->text().trimmed();
            if (name.isEmpty()) name = tabTitle;
            // Capture the current simulation state before the widget is deleted.
            QJsonObject simState;
            if (auto *w2d = qobject_cast<Simulation2DWidget*>(w))
                simState = w2d->getState();
            saveWorkspaceFile(name, simType, simState);
        }

        simTabs->removeTab(index);
        delete w;
        updateSimulationEmptyState();
    });
    return page;
}

void MainWindow::openSimulationTab2D(const QString &tabTitle, const QJsonObject &state)
{
    if (!simTabs) return;
    auto *widget = new Simulation2DWidget(tabTitle, simTabs);
    if (!state.isEmpty())
        widget->setPendingState(state);
    const int index = simTabs->addTab(widget, tabTitle);
    simTabs->setCurrentIndex(index);
    updateSimulationEmptyState();
}

void MainWindow::openSimulationTab3D(const QString &tabTitle,
                                      const CustomBH3DConfig *cfg)
{
    if (!simTabs) return;
    auto *widget = new Simulation3DWidget(simTabs);
    if (cfg)
        widget->setPendingConfig(*cfg);
    const int index = simTabs->addTab(widget, tabTitle);
    simTabs->setCurrentIndex(index);
    updateSimulationEmptyState();
}

void MainWindow::updateSimulationEmptyState()
{
    if (!simTabs || !simEmptyStateLabel) {
        return;
    }

    const bool hasTabs = simTabs->count() > 0;
    simTabs->setVisible(hasTabs);
    simEmptyStateLabel->setVisible(!hasTabs);
}

QWidget* MainWindow::createDataAnalysisPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setContentsMargins(0, 0, 0, 18);
    mainLayout->setSpacing(12);

    QLabel *headerLabel = new QLabel("Data Analysis");
    QFont headerFont;
    headerFont.setPointSize(20);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    mainLayout->addWidget(headerLabel);

    QLabel *subtitleLabel = new QLabel(
        "Load exported simulation data or import any compatible CSV file to visualise results.");
    subtitleLabel->setObjectName("subtext");
    mainLayout->addWidget(subtitleLabel);

    // ── Splitter: left source list | right chart area ──────────────────────
    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    splitter->setChildrenCollapsible(false);
    mainLayout->addWidget(splitter, 1);

    // Left panel
    QWidget *leftPanel = new QWidget();
    leftPanel->setMaximumWidth(240);
    leftPanel->setMinimumWidth(180);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 8, 0);
    leftLayout->setSpacing(6);

    QLabel *sourcesHdr = new QLabel("RECENT EXPORTS");
    sourcesHdr->setObjectName("modeNumber");
    leftLayout->addWidget(sourcesHdr);

    QListWidget *exportList = new QListWidget();
    exportList->setObjectName("recentList");
    leftLayout->addWidget(exportList, 1);

    QPushButton *refreshBtn = new QPushButton("Refresh");
    QPushButton *importBtn  = new QPushButton("Import Folder...");
    importBtn->setObjectName("launchButton");
    leftLayout->addWidget(refreshBtn);
    leftLayout->addWidget(importBtn);

    splitter->addWidget(leftPanel);

    // Right panel
    QWidget *rightPanel = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(8, 0, 0, 0);
    rightLayout->setSpacing(6);

    QLabel *loadedLabel = new QLabel();
    loadedLabel->setObjectName("subtext");
    loadedLabel->hide();
    rightLayout->addWidget(loadedLabel);

    QStackedWidget *rightStack = new QStackedWidget();
    rightLayout->addWidget(rightStack, 1);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    // Page 0: empty state
    QLabel *emptyLabel = new QLabel(
        "Select a recent export or click \"Import Data...\" to load simulation data.");
    emptyLabel->setObjectName("description");
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setWordWrap(true);
    rightStack->addWidget(emptyLabel);   // index 0

    // Page 1: chart tabs
    QTabWidget *chartTabs = new QTabWidget();
    rightStack->addWidget(chartTabs);    // index 1

    // ── Chart factory ───────────────────────────────────────────────────────
    auto makeChart = [](const QString &title, QChartView **viewOut) -> QChart * {
        auto *chart = new QChart();
        chart->setTitle(title);
        chart->setBackgroundBrush(QBrush(QColor(0x12, 0x12, 0x1e)));
        chart->setTitleBrush(QBrush(Qt::white));
        chart->legend()->setLabelColor(Qt::white);
        chart->legend()->setAlignment(Qt::AlignBottom);
        auto *view = new QChartView(chart);
        view->setRenderHint(QPainter::Antialiasing);
        view->setBackgroundBrush(QBrush(QColor(0x12, 0x12, 0x1e)));
        *viewOut = view;
        return chart;
    };

    QChartView *orbitView, *consView, *precView, *deflView;
    QChart *orbitChart = makeChart("Orbit Path",                              &orbitView);
    QChart *consChart  = makeChart("Energy Conservation Drift",               &consView);
    QChart *precChart  = makeChart("Precession per Orbit (degrees)",          &precView);
    QChart *deflChart  = makeChart("Photon Deflection vs. Impact Parameter",  &deflView);

    chartTabs->addTab(orbitView, "Orbit Path");
    chartTabs->addTab(consView,  "Energy Conservation");
    chartTabs->addTab(precView,  "Precession");
    chartTabs->addTab(deflView,  "Photon Deflection");

    // ── Axis styling helper ─────────────────────────────────────────────────
    auto styleAxes = [](QChart *chart) {
        for (auto *axis : chart->axes()) {
            auto *va = qobject_cast<QValueAxis *>(axis);
            if (!va) continue;
            va->setLabelsColor(Qt::white);
            va->setTitleBrush(QBrush(Qt::white));
            va->setLinePen(QPen(QColor(0x60, 0x60, 0x80)));
            va->setGridLinePen(QPen(QColor(0x25, 0x25, 0x38)));
        }
    };

    // ── CSV parser (returns rows of doubles, skips header) ──────────────────
    auto parseCSV = [](const QString &path) {
        QVector<QVector<double>> rows;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return rows;
        QTextStream in(&f);
        bool firstLine = true;
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (firstLine) { firstLine = false; continue; }
            if (line.isEmpty()) continue;
            const QStringList parts = line.split(',');
            QVector<double> row;
            for (const QString &p : parts) {
                bool ok;
                double v = p.trimmed().toDouble(&ok);
                if (ok) row.append(v);
            }
            if (!row.isEmpty()) rows.append(row);
        }
        return rows;
    };

    // ── FITS binary table header parser ──────────────────────────────────────
    // Skips the mandatory 1-block primary HDU, reads NAXIS2 from the BINTABLE
    // header, and returns the byte offset where the data rows begin.
    auto fitsDataOffset = [](const QString &path, int *nRows) -> qint64 {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return -1;
        if (!f.seek(2880)) return -1;   // primary HDU is always exactly 1 block
        int cards = 0;
        *nRows = 0;
        while (true) {
            char card[80];
            if (f.read(card, 80) != 80) return -1;
            ++cards;
            const QString key = QString::fromLatin1(card, 8).trimmed();
            if (key == "NAXIS2") *nRows = QString::fromLatin1(card + 10, 20).trimmed().toInt();
            if (key == "END")    break;
        }
        qint64 hdrBlocks = ((qint64)cards * 80 + 2879) / 2880;
        return 2880 + hdrBlocks * 2880;
    };

    // ── Per-type multi-format parsers (CSV → FITS → binary priority) ─────────
    // All return QVector<QVector<double>> with the same column layout as the CSV.

    // orbit_data: [worldX, worldY, radius, phi]
    auto parseOrbit = [=](const QString &folder) -> QVector<QVector<double>> {
        if (QFileInfo::exists(folder + "/orbit_data.csv"))
            return parseCSV(folder + "/orbit_data.csv");
        if (QFileInfo::exists(folder + "/orbit_data.fits")) {
            int nRows = 0;
            qint64 off = fitsDataOffset(folder + "/orbit_data.fits", &nRows);
            QFile f(folder + "/orbit_data.fits");
            if (off >= 0 && f.open(QIODevice::ReadOnly) && f.seek(off)) {
                QVector<QVector<double>> rows;
                for (int i = 0; i < nRows; ++i) {
                    char buf[32];   // 4 × double (big-endian)
                    if (f.read(buf, 32) != 32) break;
                    auto rd = [&](int o) {
                        uint64_t t; memcpy(&t, buf + o, 8);
                        t = qFromBigEndian(t);
                        double v; memcpy(&v, &t, 8); return v;
                    };
                    rows.append({rd(0), rd(8), rd(16), rd(24)});
                }
                return rows;
            }
        }
        if (QFileInfo::exists(folder + "/orbit_data.bin")) {
            QFile f(folder + "/orbit_data.bin");
            if (f.open(QIODevice::ReadOnly)) {
                uint64_t n = 0;
                f.read(reinterpret_cast<char *>(&n), 8);
                QVector<QVector<double>> rows;
                for (uint64_t i = 0; i < n; ++i) {
                    double x = 0, y = 0;
                    f.read(reinterpret_cast<char *>(&x), 8);
                    f.read(reinterpret_cast<char *>(&y), 8);
                    rows.append({x, y, 0.0, 0.0});
                }
                return rows;
            }
        }
        return {};
    };

    // conservation: [proper_time, energy_drift]
    auto parseConservation = [=](const QString &folder) -> QVector<QVector<double>> {
        if (QFileInfo::exists(folder + "/conservation.csv"))
            return parseCSV(folder + "/conservation.csv");
        if (QFileInfo::exists(folder + "/conservation.fits")) {
            int nRows = 0;
            qint64 off = fitsDataOffset(folder + "/conservation.fits", &nRows);
            QFile f(folder + "/conservation.fits");
            if (off >= 0 && f.open(QIODevice::ReadOnly) && f.seek(off)) {
                QVector<QVector<double>> rows;
                for (int i = 0; i < nRows; ++i) {
                    char buf[16];   // 2 × double (big-endian)
                    if (f.read(buf, 16) != 16) break;
                    auto rd = [&](int o) {
                        uint64_t t; memcpy(&t, buf + o, 8);
                        t = qFromBigEndian(t);
                        double v; memcpy(&v, &t, 8); return v;
                    };
                    rows.append({rd(0), rd(8)});
                }
                return rows;
            }
        }
        if (QFileInfo::exists(folder + "/conservation.bin")) {
            QFile f(folder + "/conservation.bin");
            if (f.open(QIODevice::ReadOnly)) {
                uint64_t n = 0;
                f.read(reinterpret_cast<char *>(&n), 8);
                QVector<QVector<double>> rows;
                for (uint64_t i = 0; i < n; ++i) {
                    double t = 0, d = 0;
                    f.read(reinterpret_cast<char *>(&t), 8);
                    f.read(reinterpret_cast<char *>(&d), 8);
                    rows.append({t, d});
                }
                return rows;
            }
        }
        return {};
    };

    // precession: [orbit_num, periapsis_phi, precession_rad, precession_deg]
    auto parsePrecession = [=](const QString &folder) -> QVector<QVector<double>> {
        if (QFileInfo::exists(folder + "/precession.csv"))
            return parseCSV(folder + "/precession.csv");
        if (QFileInfo::exists(folder + "/precession.fits")) {
            int nRows = 0;
            qint64 off = fitsDataOffset(folder + "/precession.fits", &nRows);
            QFile f(folder + "/precession.fits");
            if (off >= 0 && f.open(QIODevice::ReadOnly) && f.seek(off)) {
                QVector<QVector<double>> rows;
                for (int i = 0; i < nRows; ++i) {
                    char buf[28];   // int32 + 3 × double (big-endian)
                    if (f.read(buf, 28) != 28) break;
                    int32_t orb; memcpy(&orb, buf, 4);
                    orb = qFromBigEndian(orb);
                    auto rd = [&](int o) {
                        uint64_t t; memcpy(&t, buf + o, 8);
                        t = qFromBigEndian(t);
                        double v; memcpy(&v, &t, 8); return v;
                    };
                    rows.append({(double)orb, rd(4), rd(12), rd(20)});
                }
                return rows;
            }
        }
        if (QFileInfo::exists(folder + "/precession.bin")) {
            QFile f(folder + "/precession.bin");
            if (f.open(QIODevice::ReadOnly)) {
                uint64_t n = 0;
                f.read(reinterpret_cast<char *>(&n), 8);
                QVector<QVector<double>> rows;
                constexpr double PI = 3.14159265358979323846;
                for (uint64_t i = 0; i < n; ++i) {
                    double p = 0;
                    f.read(reinterpret_cast<char *>(&p), 8);
                    rows.append({(double)(i + 1), 0.0, p, p * 180.0 / PI});
                }
                return rows;
            }
        }
        return {};
    };

    // deflection: [impact_parameter, deflection_rad, deflection_deg, captured]
    auto parseDeflection = [=](const QString &folder) -> QVector<QVector<double>> {
        if (QFileInfo::exists(folder + "/deflection.csv"))
            return parseCSV(folder + "/deflection.csv");
        if (QFileInfo::exists(folder + "/deflection.fits")) {
            int nRows = 0;
            qint64 off = fitsDataOffset(folder + "/deflection.fits", &nRows);
            QFile f(folder + "/deflection.fits");
            if (off >= 0 && f.open(QIODevice::ReadOnly) && f.seek(off)) {
                QVector<QVector<double>> rows;
                for (int i = 0; i < nRows; ++i) {
                    char buf[28];   // 3 × double + int32 (big-endian)
                    if (f.read(buf, 28) != 28) break;
                    auto rd = [&](int o) {
                        uint64_t t; memcpy(&t, buf + o, 8);
                        t = qFromBigEndian(t);
                        double v; memcpy(&v, &t, 8); return v;
                    };
                    int32_t cap; memcpy(&cap, buf + 24, 4);
                    cap = qFromBigEndian(cap);
                    rows.append({rd(0), rd(8), rd(16), (double)cap});
                }
                return rows;
            }
        }
        if (QFileInfo::exists(folder + "/deflection.bin")) {
            QFile f(folder + "/deflection.bin");
            if (f.open(QIODevice::ReadOnly)) {
                uint64_t n = 0;
                f.read(reinterpret_cast<char *>(&n), 8);
                QVector<QVector<double>> rows;
                for (uint64_t i = 0; i < n; ++i) {
                    double impact = 0, rad = 0, deg = 0;
                    uint8_t cap = 0;
                    f.read(reinterpret_cast<char *>(&impact), 8);
                    f.read(reinterpret_cast<char *>(&rad),    8);
                    f.read(reinterpret_cast<char *>(&deg),    8);
                    f.read(reinterpret_cast<char *>(&cap),    1);
                    rows.append({impact, rad, deg, (double)cap});
                }
                return rows;
            }
        }
        return {};
    };

    // ── Load a project folder (CSV / FITS / binary, auto-detected) ───────────
    // Per-body files (orbit_data, conservation, precession) are written into a
    // body_<idx>[_<label>]/ subdirectory by makeBodyExportDir(). Deflection data
    // sits at the session root.  This helper finds the first body subdirectory so
    // the parsers can look in the right place; if none exists it falls back to the
    // session folder itself (handles manually imported flat layouts).
    auto resolveBodyFolder = [](const QString &sessionFolder) -> QString {
        const auto entries = QDir(sessionFolder)
            .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &e : entries)
            if (e.fileName().startsWith("body_"))
                return e.absoluteFilePath();
        return sessionFolder;
    };

    auto loadFolder = [=](const QString &folder) {
        loadedLabel->setText("Source: " + QDir(folder).dirName());
        loadedLabel->show();

        // Per-body data lives in body_<n>_<label>/ inside the session folder.
        // Deflection data is at the session root — use `folder` for that one.
        const QString bodyFolder = resolveBodyFolder(folder);

        // Orbit path
        orbitChart->removeAllSeries();
        {
            auto rows = parseOrbit(bodyFolder);
            if (!rows.isEmpty()) {
                auto *s = new QScatterSeries();
                s->setName("Orbit Trail");
                s->setMarkerSize(3.0);
                s->setColor(QColor(0x26, 0x99, 0xff));
                s->setBorderColor(Qt::transparent);
                for (const auto &r : rows)
                    if (r.size() >= 2) s->append(r[0], r[1]);
                orbitChart->addSeries(s);
                orbitChart->createDefaultAxes();
                styleAxes(orbitChart);
            }
        }

        // Energy conservation
        consChart->removeAllSeries();
        {
            auto rows = parseConservation(bodyFolder);
            if (!rows.isEmpty()) {
                auto *s = new QLineSeries();
                s->setName("Energy Drift");
                s->setColor(QColor(0xff, 0x77, 0x44));
                for (const auto &r : rows)
                    if (r.size() >= 2) s->append(r[0], r[1]);
                consChart->addSeries(s);
                consChart->createDefaultAxes();
                styleAxes(consChart);
            }
        }

        // Precession
        precChart->removeAllSeries();
        {
            auto rows = parsePrecession(bodyFolder);
            if (!rows.isEmpty()) {
                auto *s = new QLineSeries();
                s->setName("Precession (deg/orbit)");
                s->setColor(QColor(0x66, 0xff, 0x99));
                for (const auto &r : rows)
                    if (r.size() >= 4) s->append(r[0], r[3]);
                precChart->addSeries(s);
                precChart->createDefaultAxes();
                styleAxes(precChart);
            }
        }

        // Deflection
        deflChart->removeAllSeries();
        {
            auto rows = parseDeflection(folder);
            if (!rows.isEmpty()) {
                auto *freeSeries = new QScatterSeries();
                auto *captSeries = new QScatterSeries();
                freeSeries->setName("Deflected");
                captSeries->setName("Captured");
                freeSeries->setMarkerSize(4.0);
                captSeries->setMarkerSize(4.0);
                freeSeries->setColor(QColor(0xaa, 0x66, 0xff));
                captSeries->setColor(QColor(0xff, 0x33, 0x33));
                freeSeries->setBorderColor(Qt::transparent);
                captSeries->setBorderColor(Qt::transparent);
                for (const auto &r : rows) {
                    if (r.size() < 4) continue;
                    (r[3] > 0.5 ? captSeries : freeSeries)->append(r[0], r[2]);
                }
                if (!freeSeries->points().isEmpty()) deflChart->addSeries(freeSeries); else delete freeSeries;
                if (!captSeries->points().isEmpty())  deflChart->addSeries(captSeries); else delete captSeries;
                deflChart->createDefaultAxes();
                styleAxes(deflChart);
            }
        }

        rightStack->setCurrentIndex(1);
    };

    // ── Populate recent exports list ─────────────────────────────────────────
    auto populateExports = [this, exportList]() {
        exportList->clear();
        QDir dir(exportsBaseDir());
        if (!dir.exists()) return;
        const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
        for (const QFileInfo &info : entries) {
            auto *item = new QListWidgetItem(info.fileName());
            item->setData(Qt::UserRole, info.absoluteFilePath());
            exportList->addItem(item);
        }
    };

    // ── Signals ──────────────────────────────────────────────────────────────
    connect(refreshBtn, &QPushButton::clicked, page,
            [populateExports] { populateExports(); });

    connect(exportList, &QListWidget::itemClicked, page,
            [loadFolder](QListWidgetItem *item) {
                const QString path = item->data(Qt::UserRole).toString();
                if (!path.isEmpty()) loadFolder(path);
            });

    connect(importBtn, &QPushButton::clicked, page,
            [=]() {
                // Ensure the directory exists so QFileDialog starts there
                // rather than falling back to CWD when no exports have been made yet.
                const QString startDir = exportsBaseDir();
                QDir().mkpath(startDir);
                const QString folder = QFileDialog::getExistingDirectory(
                    this, "Select Data Folder",
                    startDir,
                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
                if (folder.isEmpty()) return;
                loadFolder(folder);
            });

    populateExports();
    return page;
}

QWidget* MainWindow::createObjectLibraryPage()
{
    // ── Data: every orbital object in the 2D program ─────────────────────────
    struct LibObject {
        QString category;   // header grouping
        QString name;
        QString type;       // tag shown in the list
        QString host;       // host black hole / context
        QString description;
        QString properties; // key physical properties, newline-separated
    };

    static const QVector<LibObject> ALL_OBJECTS = {
        // ── Research Scenarios ───────────────────────────────────────────────
        { "Research Scenarios",
          "ISCO Test — Unstable (5M)",
          "Test Body", "Generic BH",
          "Circular orbit placed just inside the innermost stable circular orbit. Receives a "
          "tiny inward radial kick so instability accumulates over time rather than sitting "
          "exactly on the unstable fixed point. Will spiral into the horizon on a dynamical "
          "timescale.",
          "Orbital radius: 5 M\nInitial eccentricity: ~0 (circular)\nOutcome: captured\n"
          "Classification: Unstable (sub-ISCO)" },

        { "Research Scenarios",
          "ISCO Test — Critical (6M)",
          "Test Body", "Generic BH",
          "Circular orbit at exactly the innermost stable circular orbit for a Schwarzschild "
          "black hole. Receives a tiny outward kick. Marginally stable — small perturbations "
          "grow slowly. Used to benchmark the integrator against the analytic ISCO condition "
          "r = 6M.",
          "Orbital radius: 6 M\nInitial eccentricity: ~0 (circular)\nOutcome: marginal\n"
          "Classification: Critical (ISCO)" },

        { "Research Scenarios",
          "ISCO Test — Stable (7M)",
          "Test Body", "Generic BH",
          "Circular orbit well outside the ISCO. Receives a small outward kick; orbit remains "
          "bound and stable over many periods. Demonstrates the contrast with the 5M and 6M "
          "cases.",
          "Orbital radius: 7 M\nInitial eccentricity: ~0 (circular)\nOutcome: stable\n"
          "Classification: Stable (super-ISCO)" },

        { "Research Scenarios",
          "Photon Sphere Test",
          "Photon", "Generic BH",
          "Six photons launched near the critical impact parameter b_crit = 3√3 M. Three are "
          "slightly inside (captured) and three slightly outside (escape). Visualises the razor "
          "edge of the photon sphere at r = 3M and measures deflection angles and orbit counts "
          "before absorption or escape.",
          "Critical impact parameter b_crit = 3√3 M ≈ 5.196 M\nPhoton sphere radius: 3 M\n"
          "Perturbations tested: ±0.1%, ±1%, ±5%" },

        { "Research Scenarios",
          "Radial Infall",
          "Test Body", "Generic BH",
          "A massive test particle dropped from rest at r = 20M with zero angular momentum. "
          "Follows a purely radial geodesic inward. Coordinate velocity v → 0 as r → r_s from "
          "the outside, while proper velocity diverges at the horizon. Used to demonstrate "
          "gravitational time dilation and coordinate singularity effects.",
          "Drop radius: 20 M\nAngular momentum: 0\nTrajectory: radial geodesic\n"
          "Time dilation factor: diverges at r = 2M" },

        { "Research Scenarios",
          "Tidal Disruption Star",
          "Star", "Generic BH",
          "A stellar-mass test body on an extreme eccentric orbit (e = 0.85) starting at "
          "r = 8M. At pericentre the tidal force 2M/r³ peaks sharply. If pericentre dips "
          "inside the Roche limit the body is flagged as disrupted, mimicking a real tidal "
          "disruption event (TDE). If it crosses the ISCO it plunges directly.",
          "Semi-major axis: 8 M\nEccentricity: 0.85\nPericentre: ~1.2 M\n"
          "Roche limit indicator: active\nOutcome: tidal disruption or plunge" },

        { "Research Scenarios",
          "Pulsar (Inspiraling NS)",
          "Neutron Star", "Generic BH",
          "A 1.4 M☉ neutron star in an orbit decaying via gravitational wave emission (Peters "
          "formula) and magnetic unipolar induction. Starts at a = 20M, e = 0.3. The inspiral "
          "is visually accelerated by GW_VIS_SCALE so the merger happens on screen in minutes "
          "rather than millions of years. Real-time data panel shows GW strain, Shapiro delay, "
          "chirp mass, spin-down luminosity and geodetic precession.",
          "Mass: 1.4 M☉ (neutron star)\nStarting semi-major axis: 20 M\nEccentricity: 0.3\n"
          "Spin period: ~33 ms (Crab-like)\nMagnetic field: 10¹² G\n"
          "Physics: GW decay + magnetic drag + spin-down\nVisuals: rotating jets, field arcs, "
          "light cylinder ring" },

        // ── Sgr A* System ────────────────────────────────────────────────────
        { "Sgr A* — Galactic Centre",
          "S2 Analog",
          "S-cluster Star", "Sgr A* (4.3 × 10⁶ M☉)",
          "Analogue of S2, the most-studied star in the Galactic Centre S-cluster. S2 has an "
          "orbital period of ~16 years and a pericentre just ~120 AU from Sgr A*, making it "
          "the primary probe of GR effects at a galactic centre. The 2020 pericentre passage "
          "confirmed Schwarzschild precession at the predicted level.",
          "Semi-major axis: 20 M\nEccentricity: 0.88\nBody type: Main-sequence star (≈15 M☉)\n"
          "Key effect: Schwarzschild precession, gravitational redshift at pericentre" },

        { "Sgr A* — Galactic Centre",
          "S14-like Close Orbit",
          "S-cluster Star", "Sgr A* (4.3 × 10⁶ M☉)",
          "Inspired by S14 and other tightly bound members of the S-cluster. Extreme pericentre "
          "distances (< 100 AU) make these stars the most relativistic stellar orbits known. "
          "Their short periods (< 10 yr) allow multiple pericentre passages to be observed "
          "within a human career.",
          "Semi-major axis: 10 M\nEccentricity: 0.95\nBody type: Star\n"
          "Key effect: strong-field Schwarzschild precession, measurable time dilation" },

        { "Sgr A* — Galactic Centre",
          "IRS 16 Cluster Star",
          "S-cluster Star", "Sgr A* (4.3 × 10⁶ M☉)",
          "Member of the IRS 16 complex, a concentration of massive young stars within ~0.04 pc "
          "of Sgr A*. Their presence close to the black hole is puzzling — the gravitational "
          "tidal forces should inhibit in-situ star formation (the 'paradox of youth').",
          "Semi-major axis: 40 M\nEccentricity: 0.30\nBody type: Massive OB star\n"
          "Key effect: paradox of youth, disk-star interaction models" },

        { "Sgr A* — Galactic Centre",
          "Circumnuclear Gas Clump",
          "Gas Cloud", "Sgr A* (4.3 × 10⁶ M☉)",
          "Fragment of the Circumnuclear Disk (CND), a torus of molecular gas at ~1.5–7 pc. "
          "Dense clumps can orbit the SMBH and episodically feed the accretion flow when they "
          "are perturbed inward, triggering flaring activity. G2 (observed 2013) was a real "
          "gas cloud that survived its pericentre.",
          "Semi-major axis: 60 M\nEccentricity: 0.15\nBody type: Gas Cloud\n"
          "Key effect: accretion feeding, tidal stretching at pericentre" },

        { "Sgr A* — Galactic Centre",
          "S-cluster Member",
          "S-cluster Star", "Sgr A* (4.3 × 10⁶ M☉)",
          "Generic member of the dense stellar population within the inner 0.04 pc of Sgr A*. "
          "Over 100 S-stars are catalogued. Their orbits are randomised in inclination, "
          "consistent with formation in a disrupted cluster or binary.",
          "Semi-major axis: 30 M\nEccentricity: 0.50\nBody type: Star" },

        // ── TON 618 System ───────────────────────────────────────────────────
        { "TON 618 — Ultramassive Quasar",
          "Inner Accretion Clump",
          "Gas Cloud", "TON 618 (6.6 × 10¹⁰ M☉)",
          "Dense blob of accreting plasma deep in the broad-line region (BLR) of TON 618. "
          "At ≈8M from the event horizon it is exposed to extreme UV and X-ray radiation from "
          "the accretion disk corona. Such clumps reprocess ionising radiation and produce the "
          "broad emission lines used to weigh the SMBH via reverberation mapping.",
          "Semi-major axis: 8 M\nEccentricity: 0.15\nBody type: Gas Cloud (BLR)\n"
          "Key effect: reverberation mapping target, radiation pressure dynamics" },

        { "TON 618 — Ultramassive Quasar",
          "Tidal-stripped Star",
          "Star", "TON 618 (6.6 × 10¹⁰ M☉)",
          "A star that has already lost its outer envelope to tidal forces during a previous "
          "close passage. Now on a highly eccentric orbit with a stripped helium or white-dwarf "
          "core. Each pericentre strips more mass, creating a stream of debris that feeds the "
          "accretion disk.",
          "Semi-major axis: 15 M\nEccentricity: 0.55\nBody type: Tidally stripped star\n"
          "Key effect: repeated partial TDE, mass-transfer stream" },

        { "TON 618 — Ultramassive Quasar",
          "Hot Gas Filament",
          "Gas Cloud", "TON 618 (6.6 × 10¹⁰ M☉)",
          "Elongated filament of hot shocked gas from a previous tidal disruption event. The "
          "stream is stretched by differential Keplerian shear and gradually becomes part of "
          "the accretion disk through viscous circularisation.",
          "Semi-major axis: 25 M\nEccentricity: 0.30\nBody type: Gas Cloud (TDE debris)\n"
          "Key effect: disk-feeding, Keplerian shear, circularisation timescale" },

        { "TON 618 — Ultramassive Quasar",
          "Plunging Star",
          "Star", "TON 618 (6.6 × 10¹⁰ M☉)",
          "A star on a nearly-radial orbit that will cross the ISCO and plunge before "
          "completing another orbit. For an ultramassive BH like TON 618 the tidal disruption "
          "radius lies inside the event horizon for solar-type stars — the star is swallowed "
          "whole, producing no bright flare.",
          "Semi-major axis: 12 M\nEccentricity: 0.80\nBody type: Star\n"
          "Key effect: direct capture (no TDE flare for solar-type star)" },

        { "TON 618 — Ultramassive Quasar",
          "Outer Stellar Orbit",
          "Star", "TON 618 (6.6 × 10¹⁰ M☉)",
          "Distant stellar orbit in the gravitational sphere of influence of the SMBH. At "
          "this radius the SMBH dominates stellar dynamics over the surrounding galaxy bulge. "
          "Orbital precession is detectable and apsidal precession rates encode the BH spin "
          "via frame-dragging at higher order.",
          "Semi-major axis: 50 M\nEccentricity: 0.20\nBody type: Star" },

        { "TON 618 — Ultramassive Quasar",
          "Satellite Stellar Cluster",
          "Stellar Cluster", "TON 618 (6.6 × 10¹⁰ M☉)",
          "A dense globular or nuclear star cluster inspiraling towards the SMBH via dynamical "
          "friction. As it sinks it deposits stars at the galactic centre, building the nuclear "
          "stellar cluster. Its own stellar mass black holes sink fastest, seeding an inner "
          "cusp.",
          "Semi-major axis: 80 M\nEccentricity: 0.10\nBody type: Stellar Cluster\n"
          "Key effect: dynamical friction inspiral, cluster dissolution" },

        // ── 3C 273 System ────────────────────────────────────────────────────
        { "3C 273 — First Identified Quasar",
          "Inner Jet-base Cloud",
          "Gas Cloud", "3C 273 (8.9 × 10⁸ M☉)",
          "Dense plasma blob at the base of 3C 273's relativistic jet. The jet extends ~60 kpc "
          "in projection and carries kinetic luminosity comparable to the quasar's bolometric "
          "output. Blobs like this are ejected during accretion flares and can appear "
          "superluminal as they approach the observer at a small angle.",
          "Semi-major axis: 10 M\nEccentricity: 0.20\nBody type: Gas Cloud (jet base)\n"
          "Key effect: superluminal motion, jet ejection" },

        { "3C 273 — First Identified Quasar",
          "Broad-line Region Cloud",
          "Gas Cloud", "3C 273 (8.9 × 10⁸ M☉)",
          "Gas cloud in the broad-line region, orbiting at ~0.1 pc in a flattened, disc-like "
          "configuration. Photoionised by the accretion disc, it re-emits broad Balmer and "
          "Mg II lines whose widths encode the cloud's orbital velocity (~5000 km/s), which "
          "combined with the light-travel-time lag gives the BH mass via reverberation.",
          "Semi-major axis: 20 M\nEccentricity: 0.40\nBody type: Gas Cloud (BLR)\n"
          "Key effect: reverberation mapping, broad-line emission" },

        { "3C 273 — First Identified Quasar",
          "Stripped Dwarf Remnant",
          "Dwarf Galaxy", "3C 273 (8.9 × 10⁸ M☉)",
          "Core of a dwarf galaxy that has been tidally stripped by 3C 273's host galaxy and "
          "is now on a decaying orbit around the central SMBH. The stripped nucleus may harbour "
          "its own intermediate-mass black hole, creating a dual-BH system on a sub-parsec "
          "scale.",
          "Semi-major axis: 70 M\nEccentricity: 0.25\nBody type: Dwarf Galaxy remnant\n"
          "Key effect: galaxy stripping, potential IMBH binary" },

        { "3C 273 — First Identified Quasar",
          "Close Stellar Orbit",
          "Star", "3C 273 (8.9 × 10⁸ M☉)",
          "Star on a bound orbit within the BLR, subject to both gravitational and radiation "
          "pressure from the luminous accretion disc. For luminous quasars near Eddington, "
          "radiation pressure can significantly perturb stellar orbits, making them useful "
          "probes of the accretion disc geometry.",
          "Semi-major axis: 35 M\nEccentricity: 0.60\nBody type: Star" },

        // ── J0529-4351 System ────────────────────────────────────────────────
        { "J0529-4351 — Most Luminous Quasar",
          "Fast Accretion Blob",
          "Gas Cloud", "J0529-4351 (1.7 × 10¹⁰ M☉)",
          "High-velocity blob of accreting gas spiralling in from the inner accretion disc. "
          "J0529-4351 accretes at ~400 M☉/yr — near or above the Eddington limit — driving "
          "powerful disc winds that can reach ~0.3c. Such outflows carry more kinetic power "
          "than most AGN jets.",
          "Semi-major axis: 9 M\nEccentricity: 0.10\nBody type: Gas Cloud\n"
          "Key effect: super-Eddington accretion, disc wind" },

        { "J0529-4351 — Most Luminous Quasar",
          "UV-bright Clump",
          "Gas Cloud", "J0529-4351 (1.7 × 10¹⁰ M☉)",
          "Clump of highly photoionised gas that contributes to the extreme UV luminosity of "
          "J0529-4351. Its redshift places it at z ≈ 3.96, so its emitted optical light is "
          "observed in the near-infrared. The luminosity implies a black hole growing rapidly, "
          "possibly via super-Eddington accretion.",
          "Semi-major axis: 18 M\nEccentricity: 0.35\nBody type: Gas Cloud (BLR)\n"
          "Key effect: UV photoionisation, extreme luminosity" },

        { "J0529-4351 — Most Luminous Quasar",
          "Tidally Disrupting Star",
          "Star", "J0529-4351 (1.7 × 10¹⁰ M☉)",
          "Star undergoing active tidal disruption, with its leading edge already inside the "
          "Roche limit. For this mass (1.7 × 10¹⁰ M☉) the Roche radius for a solar-type star "
          "is near the event horizon, making whole-star swallowing likely. More massive stars "
          "and white dwarfs have smaller radii and can be disrupted at larger separations.",
          "Semi-major axis: 14 M\nEccentricity: 0.70\nBody type: Star (disrupting)\n"
          "Key effect: tidal disruption event (TDE), mass stream" },

        { "J0529-4351 — Most Luminous Quasar",
          "Outer Gas Stream",
          "Gas Cloud", "J0529-4351 (1.7 × 10¹⁰ M☉)",
          "Outer accretion stream feeding the disc from a disrupted companion or infalling "
          "cloud. At this radius gas is still on nearly Keplerian orbits; viscous angular "
          "momentum transport will cause it to drift inward over many orbital periods.",
          "Semi-major axis: 45 M\nEccentricity: 0.20\nBody type: Gas Cloud" },

        { "J0529-4351 — Most Luminous Quasar",
          "Infalling Stellar Cluster",
          "Stellar Cluster", "J0529-4351 (1.7 × 10¹⁰ M☉)",
          "Dense stellar cluster sinking toward J0529's SMBH via dynamical friction. The "
          "extreme accretion environment means stellar winds and disc irradiation may erode the "
          "cluster before it reaches the galactic nucleus.",
          "Semi-major axis: 65 M\nEccentricity: 0.15\nBody type: Stellar Cluster" },

        // ── M87 System ───────────────────────────────────────────────────────
        { "M87 — EHT Image Black Hole",
          "Jet-base Knot",
          "Gas Cloud", "M87 (6.5 × 10⁹ M☉)",
          "Compact emission knot at the base of M87's famous 6-kpc relativistic jet. The EHT "
          "has directly imaged the shadow of M87's SMBH (ring diameter ~40 μas), and VLBI "
          "imaging traces the jet launching zone down to ~10 Schwarzschild radii. Jet knots "
          "can move superluminally at up to ~6c in apparent sky velocity.",
          "Semi-major axis: 10 M\nEccentricity: 0.12\nBody type: Gas Cloud (jet knot)\n"
          "Key effect: relativistic jet, frame-dragging, superluminal motion" },

        { "M87 — EHT Image Black Hole",
          "Inner Stellar Orbit",
          "Star", "M87 (6.5 × 10⁹ M☉)",
          "Star deep in M87's nuclear star cluster, within the SMBH sphere of influence "
          "(≈100 pc). Stellar dynamics here — including the velocity dispersion and the nuclear "
          "stellar cusp slope — were key inputs to the 6.5 × 10⁹ M☉ mass estimate prior to "
          "the EHT image.",
          "Semi-major axis: 20 M\nEccentricity: 0.45\nBody type: Star" },

        { "M87 — EHT Image Black Hole",
          "Hot Gas Shell Fragment",
          "Gas Cloud", "M87 (6.5 × 10⁹ M☉)",
          "Shell of hot X-ray emitting plasma swept up by the jet. M87 is embedded in the "
          "Virgo cluster intracluster medium (ICM); the jet inflates giant cavities in the "
          "ICM ('radio bubbles') that prevent runaway cooling — a key AGN feedback mechanism.",
          "Semi-major axis: 35 M\nEccentricity: 0.25\nBody type: Gas Cloud (ICM shell)\n"
          "Key effect: AGN feedback, radio bubble inflation" },

        { "M87 — EHT Image Black Hole",
          "Globular Cluster",
          "Stellar Cluster", "M87 (6.5 × 10⁹ M☉)",
          "One of M87's ~15,000 globular clusters on an orbit that has decayed to within the "
          "nuclear region. M87 hosts the largest known globular cluster system of any galaxy. "
          "Clusters that reach the nucleus can be tidally stripped, seeding the nuclear stellar "
          "population.",
          "Semi-major axis: 90 M\nEccentricity: 0.08\nBody type: Stellar Cluster\n"
          "Key effect: dynamical friction, tidal stripping, nuclear cusp building" },

        { "M87 — EHT Image Black Hole",
          "Infalling Dwarf Galaxy",
          "Dwarf Galaxy", "M87 (6.5 × 10⁹ M☉)",
          "Dwarf galaxy on a plunging orbit into M87's nucleus. Stellar-mass tidal stripping "
          "from the host cluster has already reduced it to a compact nucleus. If it harbours "
          "an IMBH it will eventually pair with M87's SMBH and produce a low-frequency "
          "gravitational wave source detectable by LISA.",
          "Semi-major axis: 70 M\nEccentricity: 0.30\nBody type: Dwarf Galaxy\n"
          "Key effect: SMBH-IMBH binary, LISA source candidate" },
    };

    // ── Layout: list on left, detail panel on right ───────────────────────────
    QWidget *page = new QWidget();
    QVBoxLayout *outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(12);

    // Header
    QLabel *headerLabel = new QLabel("Object Library");
    QFont headerFont;
    headerFont.setPointSize(20);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    outerLayout->addWidget(headerLabel);

    QLabel *subtitleLabel = new QLabel(
        "All orbital objects in the 2D simulation — click any entry to see details.");
    subtitleLabel->setObjectName("subtext");
    outerLayout->addWidget(subtitleLabel);

    // Main splitter
    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    splitter->setChildrenCollapsible(false);
    outerLayout->addWidget(splitter, 1);

    // ── Left: filterable list ─────────────────────────────────────────────────
    QWidget *listContainer = new QWidget();
    QVBoxLayout *listLayout = new QVBoxLayout(listContainer);
    listLayout->setContentsMargins(0, 0, 8, 0);
    listLayout->setSpacing(6);

    QLineEdit *filterEdit = new QLineEdit();
    filterEdit->setPlaceholderText("Filter objects…");
    listLayout->addWidget(filterEdit);

    QListWidget *libraryList = new QListWidget();
    libraryList->setSpacing(2);
    listLayout->addWidget(libraryList);

    splitter->addWidget(listContainer);

    // ── Right: detail panel ───────────────────────────────────────────────────
    QScrollArea *detailScroll = new QScrollArea();
    detailScroll->setWidgetResizable(true);
    detailScroll->setFrameShape(QFrame::NoFrame);

    QWidget *detailWidget = new QWidget();
    QVBoxLayout *detailLayout = new QVBoxLayout(detailWidget);
    detailLayout->setContentsMargins(16, 8, 8, 8);
    detailLayout->setSpacing(10);
    detailLayout->setAlignment(Qt::AlignTop);

    QLabel *detailNameLabel = new QLabel("Select an object");
    QFont nameFont;
    nameFont.setPointSize(15);
    nameFont.setBold(true);
    detailNameLabel->setFont(nameFont);
    detailNameLabel->setWordWrap(true);
    detailLayout->addWidget(detailNameLabel);

    QLabel *detailTypeLabel = new QLabel();
    detailTypeLabel->setObjectName("subtext");
    detailLayout->addWidget(detailTypeLabel);

    QFrame *divider = new QFrame();
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);
    detailLayout->addWidget(divider);

    QLabel *detailHostLabel = new QLabel();
    detailHostLabel->setObjectName("description");
    detailLayout->addWidget(detailHostLabel);

    QLabel *detailDescLabel = new QLabel();
    detailDescLabel->setWordWrap(true);
    detailDescLabel->setObjectName("description");
    detailLayout->addWidget(detailDescLabel);

    QFrame *divider2 = new QFrame();
    divider2->setFrameShape(QFrame::HLine);
    divider2->setFrameShadow(QFrame::Sunken);
    detailLayout->addWidget(divider2);

    QLabel *detailPropsLabel = new QLabel();
    detailPropsLabel->setWordWrap(true);
    detailPropsLabel->setObjectName("subtext");
    detailLayout->addWidget(detailPropsLabel);

    detailLayout->addStretch();
    detailScroll->setWidget(detailWidget);
    splitter->addWidget(detailScroll);
    splitter->setSizes({320, 480});

    // ── Populate list with category separators ────────────────────────────────
    auto populateList = [&](const QString &filter) {
        libraryList->clear();
        QString lastCat;
        for (int i = 0; i < ALL_OBJECTS.size(); ++i) {
            const LibObject &obj = ALL_OBJECTS[i];
            bool matches = filter.isEmpty()
                || obj.name.contains(filter, Qt::CaseInsensitive)
                || obj.category.contains(filter, Qt::CaseInsensitive)
                || obj.type.contains(filter, Qt::CaseInsensitive)
                || obj.host.contains(filter, Qt::CaseInsensitive);
            if (!matches) continue;

            // Category separator
            if (obj.category != lastCat) {
                lastCat = obj.category;
                QListWidgetItem *sep = new QListWidgetItem("  " + obj.category);
                sep->setFlags(Qt::NoItemFlags);
                QFont sepFont;
                sepFont.setBold(true);
                sepFont.setPointSize(9);
                sep->setFont(sepFont);
                sep->setForeground(QColor(120, 180, 255));
                sep->setData(Qt::UserRole, -1); // not an object
                libraryList->addItem(sep);
            }

            QListWidgetItem *item = new QListWidgetItem(
                "    " + obj.name + "  [" + obj.type + "]");
            item->setData(Qt::UserRole, i);
            libraryList->addItem(item);
        }
    };

    populateList(QString());

    // ── Connections ───────────────────────────────────────────────────────────
    QObject::connect(filterEdit, &QLineEdit::textChanged,
                     [populateList](const QString &text) { populateList(text); });

    QObject::connect(libraryList, &QListWidget::currentItemChanged,
        [=](QListWidgetItem *item, QListWidgetItem *) {
            if (!item) return;
            int idx = item->data(Qt::UserRole).toInt();
            if (idx < 0 || idx >= ALL_OBJECTS.size()) return;
            const LibObject &obj = ALL_OBJECTS[idx];
            detailNameLabel->setText(obj.name);
            detailTypeLabel->setText("Type: " + obj.type);
            detailHostLabel->setText("Host / Context:  " + obj.host);
            detailDescLabel->setText(obj.description);
            detailPropsLabel->setText(obj.properties);
        });

    return page;
}

QWidget* MainWindow::createExportPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    QLabel *headerLabel = new QLabel("Export");
    QFont headerFont;
    headerFont.setPointSize(20);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    layout->addWidget(headerLabel);

    QLabel *subtitleLabel = new QLabel("Choose export folder and file type for CSV/FITS output.");
    subtitleLabel->setObjectName("subtext");
    layout->addWidget(subtitleLabel);

    QLineEdit *exportFolderEdit = new QLineEdit("saves/");
    exportFolderEdit->setPlaceholderText("Export folder");
    layout->addWidget(exportFolderEdit);

    QComboBox *exportTypeCombo = new QComboBox();
    exportTypeCombo->addItems({"CSV", "FITS", "Binary"});
    layout->addWidget(exportTypeCombo);

    QLabel *noteLabel = new QLabel("All exported data will be stored under the selected folder by default.");
    noteLabel->setObjectName("description");
    layout->addWidget(noteLabel);

    QPushButton *exportApplyButton = new QPushButton("Apply export settings");
    layout->addWidget(exportApplyButton);
    connect(exportApplyButton, &QPushButton::clicked, this, [this, exportFolderEdit, exportTypeCombo] {
        QString path = exportFolderEdit->text().trimmed();
        if (path.isEmpty()) {
            QMessageBox::warning(this, "Invalid Path", "Export folder path cannot be empty.");
            return;
        }
        QDir dir(path);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                QMessageBox::warning(this, "Invalid Path",
                    "Could not create export folder:\n" + path);
                return;
            }
        }
        QFileInfo fi(path);
        if (!fi.isWritable()) {
            QMessageBox::warning(this, "Invalid Path",
                "Export folder is not writable:\n" + path);
            return;
        }
        statusLabel->setText("Export set to " + path + " (" + exportTypeCombo->currentText() + ")");
    });

    layout->addStretch();
    return page;
}

QWidget* MainWindow::createSettingsPage()
{
    QSettings s("Aetherion", "AetherionSuite");

    // ── Paths for keybind config files ──────────────────────────────────────
    const QString cfg2DPath = aetherionDataDir() + "/blackhole2d_keybinds.cfg";
    const QString cfg3DPath = aetherionDataDir() + "/blackhole3d_keybinds.cfg";

    // Helper: read one value from a .cfg file
    auto readCfgKey = [](const QString &path, const QString &action,
                         const QString &def) -> QString {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return def;
        while (!f.atEnd()) {
            const QString line = f.readLine().trimmed();
            if (line.startsWith('#') || !line.contains('=')) continue;
            const int eq = line.indexOf('=');
            if (line.left(eq).trimmed() == action)
                return line.mid(eq + 1).trimmed().toUpper();
        }
        return def;
    };

    // ── Outer page widget ────────────────────────────────────────────────────
    QWidget *page         = new QWidget();
    QVBoxLayout *pageVBox = new QVBoxLayout(page);
    pageVBox->setContentsMargins(0, 0, 0, 12);
    pageVBox->setSpacing(12);

    // Header
    QLabel *headerLabel = new QLabel("Settings");
    QFont hf; hf.setPointSize(20); hf.setBold(true);
    headerLabel->setFont(hf);
    pageVBox->addWidget(headerLabel);

    QLabel *subtitleLabel = new QLabel("Configure preferences, display options, and input keybindings.");
    subtitleLabel->setObjectName("subtext");
    pageVBox->addWidget(subtitleLabel);

    // ── Left nav + right stacked content ────────────────────────────────────
    QWidget *body       = new QWidget();
    QHBoxLayout *bodyHL = new QHBoxLayout(body);
    bodyHL->setContentsMargins(0, 0, 0, 0);
    bodyHL->setSpacing(0);

    // Left navigation list
    QListWidget *navList = new QListWidget();
    navList->setObjectName("settingsNav");
    navList->setFixedWidth(155);
    navList->setFrameShape(QFrame::NoFrame);
    navList->addItem("  General");
    navList->addItem("  2D Keybinds");
    navList->addItem("  3D Keybinds");
    navList->setCurrentRow(0);
    navList->setStyleSheet(R"(
        QListWidget#settingsNav {
            background-color: #090910;
            border-right: 1px solid #2a2a3a;
        }
        QListWidget#settingsNav::item {
            height: 36px;
            padding-left: 8px;
            color: #909098;
            font-size: 11pt;
            border-left: 3px solid transparent;
        }
        QListWidget#settingsNav::item:selected {
            background-color: #101020;
            color: #d0d8f8;
            border-left: 3px solid #4477cc;
        }
        QListWidget#settingsNav::item:hover:!selected {
            background-color: #0e0e1c;
            color: #b0b8d8;
        }
    )");

    // Right stacked widget
    QStackedWidget *rightStack = new QStackedWidget();

    bodyHL->addWidget(navList);
    bodyHL->addWidget(rightStack, 1);

    pageVBox->addWidget(body, 1);

    connect(navList, &QListWidget::currentRowChanged,
            rightStack, &QStackedWidget::setCurrentIndex);

    // ── Helper: make a group-header label ───────────────────────────────────
    auto makeGroupHdr = [](const QString &text) {
        QLabel *lbl = new QLabel(text);
        lbl->setStyleSheet(
            "color: #4d78cc;"
            "font-size: 9pt;"
            "font-weight: bold;"
            "letter-spacing: 1px;"
            "padding-top: 6px;"
            "padding-bottom: 2px;"
            "border-bottom: 1px solid #2a2a44;");
        return lbl;
    };

    // ── Helper: make a settings card container ───────────────────────────────
    auto makeCard = [](QWidget *parent = nullptr) {
        QFrame *card = new QFrame(parent);
        card->setObjectName("settingsCard");
        card->setStyleSheet(
            "QFrame#settingsCard {"
            "  background-color: #0e0e1a;"
            "  border: 1px solid #252535;"
            "  border-radius: 6px;"
            "}"
        );
        return card;
    };

    // ────────────────────────────────────────────────────────────────────────
    // PAGE 0 — GENERAL
    // ────────────────────────────────────────────────────────────────────────
    {
        QScrollArea *scroll = new QScrollArea();
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setStyleSheet("background-color: transparent;");

        QWidget *content = new QWidget();
        QVBoxLayout *lay = new QVBoxLayout(content);
        lay->setContentsMargins(18, 14, 18, 14);
        lay->setSpacing(14);

        // ── Interface card ──────────────────────────────────────────────────
        QFrame *ifCard = makeCard();
        QVBoxLayout *ifLay = new QVBoxLayout(ifCard);
        ifLay->setContentsMargins(16, 12, 16, 16);
        ifLay->setSpacing(6);
        ifLay->addWidget(makeGroupHdr("INTERFACE"));

        showTooltipsCheck = new QCheckBox("Show tooltips on hover");
        showTooltipsCheck->setChecked(s.value("ui/showTooltips", true).toBool());
        ifLay->addWidget(showTooltipsCheck);

        memoryDisplayCheck = new QCheckBox("Show memory usage in status bar");
        memoryDisplayCheck->setChecked(s.value("ui/memoryDisplay", true).toBool());
        ifLay->addWidget(memoryDisplayCheck);
        lay->addWidget(ifCard);

        // ── Export defaults card ────────────────────────────────────────────
        QFrame *expCard = makeCard();
        QVBoxLayout *expLay = new QVBoxLayout(expCard);
        expLay->setContentsMargins(16, 12, 16, 16);
        expLay->setSpacing(8);
        expLay->addWidget(makeGroupHdr("EXPORT DEFAULTS"));

        auto makeFormRow = [](const QString &labelText, QWidget *control) {
            QWidget *row = new QWidget();
            QHBoxLayout *hl = new QHBoxLayout(row);
            hl->setContentsMargins(0,0,0,0);
            hl->setSpacing(10);
            QLabel *lbl = new QLabel(labelText);
            lbl->setMinimumWidth(130);
            hl->addWidget(lbl);
            hl->addWidget(control, 1);
            return row;
        };

        exportFolderEdit = new QLineEdit(s.value("export/folder", "saves/").toString());
        expLay->addWidget(makeFormRow("Default folder:", exportFolderEdit));

        exportTypeCombo = new QComboBox();
        exportTypeCombo->addItems({"CSV", "FITS", "Binary"});
        {
            int idx = exportTypeCombo->findText(s.value("export/type", "CSV").toString());
            if (idx >= 0) exportTypeCombo->setCurrentIndex(idx);
        }
        expLay->addWidget(makeFormRow("Default format:", exportTypeCombo));
        lay->addWidget(expCard);

        // ── Updates card ────────────────────────────────────────────────────
        QFrame *updCard = makeCard();
        QVBoxLayout *updLay = new QVBoxLayout(updCard);
        updLay->setContentsMargins(16, 12, 16, 16);
        updLay->setSpacing(8);
        updLay->addWidget(makeGroupHdr("UPDATES"));

        // Current version label
        QLabel *verLabel = new QLabel(
            QString("Current version: <b>%1</b>").arg(Updater::currentVersion()));
        verLabel->setTextFormat(Qt::RichText);
        updLay->addWidget(verLabel);

        // Auto-check checkbox
        QCheckBox *autoCheckBox = new QCheckBox("Automatically check for updates on startup");
        autoCheckBox->setChecked(s.value("updates/autoCheck", true).toBool());
        connect(autoCheckBox, &QCheckBox::toggled, this, [](bool checked) {
            QSettings ss("Aetherion", "AetherionSuite");
            ss.setValue("updates/autoCheck", checked);
        });
        updLay->addWidget(autoCheckBox);

        // Manual check button
        QPushButton *checkNowBtn = new QPushButton("Check for Updates…");
        checkNowBtn->setFixedWidth(180);
        connect(checkNowBtn, &QPushButton::clicked,
                this, &MainWindow::checkForUpdatesManual);
        updLay->addWidget(checkNowBtn);

        lay->addWidget(updCard);

        lay->addStretch();
        scroll->setWidget(content);
        rightStack->addWidget(scroll);
    }

    // ────────────────────────────────────────────────────────────────────────
    // PAGES 1 & 2 — KEYBIND PAGES (shared helper)
    // ────────────────────────────────────────────────────────────────────────

    // Conflict-checker: mark any button red if two buttons share the same key
    // within the same section.
    auto makeConflictChecker = [](std::shared_ptr<QVector<KeyBindButton*>> btns) {
        return [btns]() {
            QMap<QString, int> counts;
            for (auto *b : *btns) counts[b->keyName()]++;
            for (auto *b : *btns) b->setConflicting(counts[b->keyName()] > 1);
        };
    };

    // Helper: add a single keybind row (label + KeyBindButton) to a QGridLayout.
    auto addKBRow = [](QGridLayout *grid, int row, const QString &label,
                       KeyBindButton *btn) {
        QLabel *lbl = new QLabel(label);
        lbl->setStyleSheet("color: #c0c8dc; font-size: 10pt;");
        grid->addWidget(lbl, row, 0);
        grid->addWidget(btn, row, 1, Qt::AlignRight);
    };

    // Helper: add a subsection header spanning both columns.
    auto addGroupRow = [&makeGroupHdr](QGridLayout *grid, int row,
                                       const QString &text) {
        QLabel *hdr = makeGroupHdr(text);
        grid->addWidget(hdr, row, 0, 1, 2);
    };

    // Helper: build one scroll-area keybind page from a list of sections.
    // Each section: { title, [ {action, label, defaultKey} ] }
    struct KBEntry { QString action, label, defaultKey; KeyBindButton *btn = nullptr; };
    using KBSection = std::pair<QString, QVector<KBEntry>>;

    auto buildKBPage = [&](const QString &cfgPath,
                            QVector<KBSection> sections,
                            QString &cfgPathOut,
                            std::shared_ptr<QVector<KeyBindButton*>> btnList)
        -> QWidget*
    {
        QScrollArea *scroll = new QScrollArea();
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setStyleSheet("background-color: transparent;");

        QWidget *content = new QWidget();
        QVBoxLayout *lay = new QVBoxLayout(content);
        lay->setContentsMargins(18, 10, 18, 14);
        lay->setSpacing(10);

        QLabel *hint = new QLabel(
            "Click a key button then press the key you want to assign.  "
            "Escape cancels.  Conflicting keys are shown in \xE2\x9C\x95 red.");
        hint->setWordWrap(true);
        hint->setStyleSheet("color: #707088; font-size: 9pt; margin-bottom: 4px;");
        lay->addWidget(hint);

        auto checker = makeConflictChecker(btnList);

        for (auto &section : sections) {
            QFrame *card = new QFrame();
            card->setObjectName("settingsCard");
            card->setStyleSheet(
                "QFrame#settingsCard {"
                "  background-color: #0e0e1a;"
                "  border: 1px solid #252535;"
                "  border-radius: 6px;"
                "}");

            QGridLayout *grid = new QGridLayout(card);
            grid->setContentsMargins(16, 10, 16, 14);
            grid->setHorizontalSpacing(12);
            grid->setVerticalSpacing(6);
            grid->setColumnStretch(0, 1);
            grid->setColumnMinimumWidth(1, 110);

            int row = 0;
            addGroupRow(grid, row++, section.first);

            for (auto &e : section.second) {
                auto *btn = new KeyBindButton(
                    readCfgKey(cfgPath, e.action, e.defaultKey.toUpper()));
                e.btn = btn;
                btnList->append(btn);
                addKBRow(grid, row++, e.label, btn);

                connect(btn, &KeyBindButton::keyChanged, page, [checker](const QString&) {
                    checker();
                });
            }
            lay->addWidget(card);
        }

        lay->addStretch();
        scroll->setWidget(content);
        cfgPathOut = cfgPath;
        return scroll;
    };

    // ────────────────────────────────────────────────────────────────────────
    // PAGE 1 — 2D KEYBINDS
    // ────────────────────────────────────────────────────────────────────────
    auto btns2D = std::make_shared<QVector<KeyBindButton*>>();
    QString usedCfg2D;

    QVector<KBSection> sections2D = {
        { "SIMULATION", {
            { "pause",          "Pause / Resume",        "SPACE" },
            { "toggle_preset",  "Toggle Preset Mode",    "T"     },
            { "next_preset",    "Next Preset",           "]"     },
            { "prev_preset",    "Previous Preset",       "["     },
            { "reset",          "Reset Simulation",      "R"     },
        }},
        { "ORBITAL CONTROL", {
            { "zoom_in",        "Zoom In",               "UP"    },
            { "zoom_out",       "Zoom Out",              "DOWN"  },
            { "orbit_in",       "Orbit Closer",          "LEFT"  },
            { "orbit_out",      "Orbit Farther",         "RIGHT" },
            { "ecc_decrease",   "Decrease Eccentricity", "E"     },
            { "ecc_increase",   "Increase Eccentricity", "Q"     },
        }},
        { "VISUALIZATION", {
            { "toggle_rays",    "Toggle Ray Paths",      "S"     },
            { "toggle_photon",  "Toggle Photon Sphere",  "P"     },
            { "toggle_galaxy",  "Toggle Galaxy System",  "G"     },
            { "toggle_influence","Toggle Influence Zones","I"    },
            { "speed_up",       "Increase Time Scale",   "="     },
            { "speed_down",     "Decrease Time Scale",   "-"     },
            { "temp_down",      "Decrease Gas Temp",     ","     },
            { "temp_up",        "Increase Gas Temp",     "."     },
        }},
        { "RESEARCH TOOLS", {
            { "toggle_data",    "Data Panel",            "D"     },
            { "toggle_dilation","Time Dilation Map",     "F"     },
            { "toggle_caustics","Caustic Field",         "C"     },
            { "toggle_error",   "Numerical Error",       "N"     },
            { "cycle_body",     "Cycle Selected Body",   "H"     },
            { "toggle_controls","Toggle Controls Panel", "/"     },
            { "export_data",    "Export Data",           "X"     },
        }},
        { "TEST SCENARIOS", {
            { "test_isco",      "ISCO Test (5/6/7 M)",   "1"    },
            { "test_photon",    "Photon Sphere Test",    "2"     },
            { "test_infall",    "Radial Infall Test",    "3"     },
            { "test_tidal",     "Tidal Disruption Test", "4"     },
        }},
    };

    rightStack->addWidget(buildKBPage(cfg2DPath, sections2D, usedCfg2D, btns2D));

    // ────────────────────────────────────────────────────────────────────────
    // PAGE 2 — 3D KEYBINDS
    // ────────────────────────────────────────────────────────────────────────
    auto btns3D = std::make_shared<QVector<KeyBindButton*>>();
    QString usedCfg3D;

    QVector<KBSection> sections3D = {
        { "VIEW CONTROL", {
            { "toggle_freelook", "Freelook / Orbit",     "F"      },
            { "release_mouse",   "Release Mouse",        "ESCAPE" },
        }},
        { "ENVIRONMENT", {
            { "toggle_jets",        "Toggle Jets",       "J"      },
            { "toggle_blr",         "Toggle BLR Cloud",  "G"      },
            { "toggle_orb_body",    "Toggle Orb. Body",  "O"      },
            { "toggle_host_galaxy", "Toggle Host Galaxy","NUM1"   },
            { "toggle_lab",         "Toggle LAB",        "NUM2"   },
            { "toggle_cgm",         "Toggle CGM",        "NUM3"   },
        }},
        { "VISUAL EFFECTS", {
            { "toggle_doppler",    "Toggle Doppler",     "V"      },
            { "toggle_blueshift",  "Toggle Blueshift",   "U"      },
            { "toggle_render",     "Cinematic Mode",     "P"      },
        }},
        { "SIMULATION", {
            { "cycle_anim_speed",  "Cycle Anim Speed",   "Y"      },
            { "toggle_hud",        "Toggle HUD",         "H"      },
            { "next_profile",      "Next Preset Profile","N"      },
        }},
    };

    rightStack->addWidget(buildKBPage(cfg3DPath, sections3D, usedCfg3D, btns3D));

    // ── Save + Reset bar ─────────────────────────────────────────────────────
    QWidget *btnBar   = new QWidget();
    QHBoxLayout *btnHL = new QHBoxLayout(btnBar);
    btnHL->setContentsMargins(0, 4, 0, 0);
    btnHL->setSpacing(10);

    QPushButton *resetBtn = new QPushButton("Reset Keybinds");
    resetBtn->setStyleSheet(
        "QPushButton { background-color:#161622; color:#a0a8c0;"
        "  border:1px solid #333348; border-radius:4px; padding:6px 14px; }"
        "QPushButton:hover { background-color:#1e1e34; color:#c0c8e0; }");
    btnHL->addWidget(resetBtn);
    btnHL->addStretch();

    QPushButton *saveBtn = new QPushButton("Save Settings");
    saveBtn->setStyleSheet(
        "QPushButton { background-color:#2669bb; color:#ededed;"
        "  border:none; border-radius:4px; padding:6px 18px; font-weight:bold; }"
        "QPushButton:hover  { background-color:#3d7acc; }"
        "QPushButton:pressed{ background-color:#1f5aa0; }");
    btnHL->addWidget(saveBtn);

    pageVBox->addWidget(btnBar);

    // ── Save logic ───────────────────────────────────────────────────────────
    auto writeCfg = [this](const QString &path,
                             const QString  &header,
                             const QVector<KBSection> &sections,
                             const QVector<KeyBindButton*> &allBtns) {
        // Collect action→button in order
        int btnIdx = 0;
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            return false;
        QTextStream out(&f);
        out << header << "\n\n";
        for (const auto &section : sections) {
            out << "# " << section.first << "\n";
            for (const auto &e : section.second) {
                QString val = allBtns[btnIdx++]->keyName().trimmed().toUpper();
                if (val.isEmpty()) val = e.defaultKey.toUpper();
                out << e.action << "=" << val << "\n";
            }
            out << "\n";
        }
        return true;
    };

    connect(saveBtn, &QPushButton::clicked, page,
        [this, sections2D, sections3D, cfg2DPath, cfg3DPath,
         btns2D, btns3D, writeCfg]() mutable {

            saveSettings();   // persist QSettings prefs

            bool ok2D = writeCfg(cfg2DPath,
                "# Aetherion 2D keybinds\n# Format: action=KEY\n# Reopen the 2D tab to apply.",
                sections2D, *btns2D);
            // Append export_format so the 2D sim knows which format to use
            if (ok2D && exportTypeCombo) {
                QFile f2(cfg2DPath);
                if (f2.open(QIODevice::Append | QIODevice::Text)) {
                    QTextStream ts(&f2);
                    ts << "export_format=" << exportTypeCombo->currentText() << "\n";
                }
            }
            bool ok3D = writeCfg(cfg3DPath,
                "# Aetherion 3D keybinds\n# Format: action=KEY\n# Reopen the 3D tab to apply.",
                sections3D, *btns3D);

            if (ok2D && ok3D)
                statusLabel->setText("Settings saved — reopen simulation tabs to apply keybinds");
            else if (!ok2D)
                statusLabel->setText("Warning: could not write 2D keybind file");
            else
                statusLabel->setText("Warning: could not write 3D keybind file");
        });

    connect(resetBtn, &QPushButton::clicked, page,
        [this, cfg2DPath, cfg3DPath, sections2D, sections3D,
         btns2D, btns3D]() mutable {

            // Reset all 2D buttons to their defaults
            {
                int i = 0;
                for (const auto &sec : sections2D)
                    for (const auto &e : sec.second)
                        (*btns2D)[i++]->setKeyName(e.defaultKey.toUpper());
            }
            // Recheck 2D conflicts after reset
            {
                QMap<QString,int> cnt;
                for (auto *b : *btns2D) cnt[b->keyName()]++;
                for (auto *b : *btns2D) b->setConflicting(cnt[b->keyName()] > 1);
            }
            // Reset all 3D buttons to their defaults
            {
                int i = 0;
                for (const auto &sec : sections3D)
                    for (const auto &e : sec.second)
                        (*btns3D)[i++]->setKeyName(e.defaultKey.toUpper());
            }
            {
                QMap<QString,int> cnt;
                for (auto *b : *btns3D) cnt[b->keyName()]++;
                for (auto *b : *btns3D) b->setConflicting(cnt[b->keyName()] > 1);
            }
            QFile::remove(cfg2DPath);
            QFile::remove(cfg3DPath);
            statusLabel->setText("Keybinds reset to defaults — reopen tabs to apply");
        });

    return page;
}

void MainWindow::on2DSimulationClicked()
{
    // Switch to Simulation workspace and open a 2D tab on demand.
    workspaceList->setCurrentRow(1);
    contentStack->setCurrentIndex(1);
    openSimulationTab2D("2D Research");
    statusLabel->setText("Opened 2D simulation tab");
}

void MainWindow::on3DSimulationClicked()
{
    // Switch to Simulation workspace and open a 3D tab on demand.
    workspaceList->setCurrentRow(1);
    contentStack->setCurrentIndex(1);
    openSimulationTab3D("3D Visualization");
    statusLabel->setText("Opened 3D simulation tab");
}

void MainWindow::onWorkspaceSelectionChanged(int row)
{
    if (row >= 0 && row <= 3) {
        contentStack->setCurrentIndex(row);
        toolsList->clearSelection();
    }
}

void MainWindow::onToolsSelectionChanged(int row)
{
    if (row >= 0) {
        contentStack->setCurrentIndex(4 + row);
        workspaceList->clearSelection();
    }
}

void MainWindow::onRecentFileSelected(QListWidgetItem *item)
{
    if (!item || !(item->flags() & Qt::ItemIsSelectable)) return;

    const QString name     = item->data(Qt::UserRole + 1).toString();
    const QString type     = item->data(Qt::UserRole + 2).toString();
    const QJsonObject sim  = item->data(Qt::UserRole + 3).toJsonObject();

    workspaceList->setCurrentRow(1);
    contentStack->setCurrentIndex(1);
    if (type == "3D")
        openSimulationTab3D(name);
    else
        openSimulationTab2D(name, sim);

    statusLabel->setText("Opened workspace: " + name);
}

// ── Workspace persistence ─────────────────────────────────────────────────────

QString MainWindow::workspacesDir() const
{
    return aetherionDataDir() + "/workspaces";
}

QString MainWindow::exportsBaseDir() const
{
    return aetherionDataDir() + "/exports";
}

void MainWindow::saveWorkspaceFile(const QString &name, const QString &type,
                                   const QJsonObject &simState)
{
    const QString dir = workspacesDir();
    QDir().mkpath(dir);

    // Timestamp-based filename guarantees uniqueness
    const QString ts       = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    const QString filePath = dir + "/workspace_" + ts + ".aetherion";

    QJsonObject obj;
    obj["version"] = 2;
    obj["name"]    = name;
    obj["type"]    = type;
    obj["savedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    if (!simState.isEmpty())
        obj["sim"]  = simState;

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusLabel->setText("Error: could not write workspace file");
        return;
    }
    f.write(QJsonDocument(obj).toJson());
    f.close();

    populateRecentList();
    statusLabel->setText("Workspace saved: " + name);
}

void MainWindow::populateRecentList()
{
    if (!recentList) return;
    recentList->clear();

    QDir dir(workspacesDir());
    const QStringList files =
        dir.entryList({"*.aetherion"}, QDir::Files, QDir::Time);

    if (files.isEmpty()) {
        auto *placeholder = new QListWidgetItem("No recent workspaces");
        placeholder->setFlags(Qt::NoItemFlags);
        recentList->addItem(placeholder);
        return;
    }

    for (const QString &filename : files) {
        const QString path = dir.filePath(filename);
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (doc.isNull() || !doc.isObject()) continue;

        const QJsonObject obj  = doc.object();
        const QString name     = obj["name"].toString("Unnamed");
        const QString type     = obj["type"].toString("?");
        const QString savedAt  = obj["savedAt"].toString();
        const QJsonObject sim  = obj["sim"].toObject();  // may be empty for v1 files

        QString dateStr;
        if (!savedAt.isEmpty()) {
            const QDateTime dt = QDateTime::fromString(savedAt, Qt::ISODate);
            dateStr = dt.toString("MMM d, yyyy  h:mm ap");
        }

        const QString display = name + "   [" + type + "]"
                                + (dateStr.isEmpty() ? QString() : "   " + dateStr);

        auto *item = new QListWidgetItem(display);
        item->setData(Qt::UserRole,     path);
        item->setData(Qt::UserRole + 1, name);
        item->setData(Qt::UserRole + 2, type);
        item->setData(Qt::UserRole + 3, sim);
        recentList->addItem(item);
    }
}

void MainWindow::onRecentItemContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = recentList->itemAt(pos);
    if (!item || !(item->flags() & Qt::ItemIsSelectable)) return;

    const QString filePath = item->data(Qt::UserRole).toString();
    const QString name     = item->data(Qt::UserRole + 1).toString();
    const QString type     = item->data(Qt::UserRole + 2).toString();

    QMenu menu(this);
    QAction *openAction   = menu.addAction("Open");
    menu.addSeparator();
    QAction *renameAction = menu.addAction("Rename...");
    QAction *deleteAction = menu.addAction("Delete");

    QAction *chosen = menu.exec(recentList->mapToGlobal(pos));

    if (chosen == openAction) {
        const QJsonObject sim = item->data(Qt::UserRole + 3).toJsonObject();
        workspaceList->setCurrentRow(1);
        contentStack->setCurrentIndex(1);
        if (type == "3D") openSimulationTab3D(name);
        else              openSimulationTab2D(name, sim);
        statusLabel->setText("Opened workspace: " + name);

    } else if (chosen == renameAction) {
        bool ok;
        const QString newName = QInputDialog::getText(
            this, "Rename Workspace", "New name:",
            QLineEdit::Normal, name, &ok).trimmed();

        if (ok && !newName.isEmpty()) {
            QFile f(filePath);
            if (f.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
                f.close();
                if (!doc.isNull() && doc.isObject()) {
                    QJsonObject obj = doc.object();
                    obj["name"] = newName;
                    if (f.open(QIODevice::WriteOnly | QIODevice::Text |
                               QIODevice::Truncate)) {
                        f.write(QJsonDocument(obj).toJson());
                        f.close();
                    }
                }
            }
            populateRecentList();
            statusLabel->setText("Renamed to: " + newName);
        }

    } else if (chosen == deleteAction) {
        const int result = QMessageBox::question(
            this, "Delete Workspace",
            "Delete \"" + name + "\"? This cannot be undone.",
            QMessageBox::Yes | QMessageBox::No);
        if (result == QMessageBox::Yes) {
            QFile::remove(filePath);
            populateRecentList();
            statusLabel->setText("Deleted workspace: " + name);
        }
    }
}
