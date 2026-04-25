#include "updater.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QCoreApplication>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QTextStream>
#include <QTimer>
#include <QSettings>

// ─── Repository constants ────────────────────────────────────────────────────
static constexpr const char *kOwner = "0xLiam0920";
static constexpr const char *kRepo  = "Aetherion-Astrophysics-Suite";

static QString apiLatestUrl()
{
    return QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
               .arg(kOwner, kRepo);
}

static QString releasesPageUrl(const QString &tag = {})
{
    if (tag.isEmpty())
        return QStringLiteral("https://github.com/%1/%2/releases").arg(kOwner, kRepo);
    return QStringLiteral("https://github.com/%1/%2/releases/tag/%3").arg(kOwner, kRepo, tag);
}

// ─── POSIX shell quoting ─────────────────────────────────────────────────────
// Wraps a string in single quotes, escaping any embedded single quotes.
static QString sq(const QString &s)
{
    return u'\'' + QString(s).replace(u'\'', QStringLiteral("'\\''")) + u'\'';
}

// ─────────────────────────────────────────────────────────────────────────────

Updater::Updater(QWidget *parent)
    : QObject(parent)
    , m_parentWidget(parent)
    , m_nam(new QNetworkAccessManager(this))
{}

QString Updater::currentVersion()
{
    return QStringLiteral(AETHERION_VERSION);
}

// ─── Public: kick off a version check ────────────────────────────────────────
void Updater::checkForUpdates(bool silent)
{
    m_silent = silent;

    // Abort any in-flight check before starting a new one.
    if (m_checkReply) {
        m_checkReply->abort();
        m_checkReply->deleteLater();
        m_checkReply = nullptr;
    }

    QNetworkRequest req{QUrl(apiLatestUrl())};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("AetherionSuite/" AETHERION_VERSION));
    req.setRawHeader("Accept",              "application/vnd.github+json");
    req.setRawHeader("X-GitHub-Api-Version","2022-11-28");

    m_checkReply = m_nam->get(req);
    connect(m_checkReply, &QNetworkReply::finished,
            this,          &Updater::onCheckFinished);
}

// ─── Version comparison ───────────────────────────────────────────────────────
// Parses "vX.Y.Z" or "X.Y.Z" into {major, minor, patch} and compares.
bool Updater::isNewer(const QString &remote, const QString &local)
{
    auto stripped = [](const QString &v) {
        return v.startsWith('v', Qt::CaseInsensitive) ? v.mid(1) : v;
    };
    auto parts = [](const QString &v) -> std::array<int, 3> {
        const QStringList ps = v.split('.');
        return { ps.value(0).toInt(), ps.value(1).toInt(), ps.value(2).toInt() };
    };
    const auto r = parts(stripped(remote));
    const auto c = parts(stripped(local));
    for (int i = 0; i < 3; ++i) {
        if (r[i] > c[i]) return true;
        if (r[i] < c[i]) return false;
    }
    return false; // equal
}

// ─── Platform asset selection ─────────────────────────────────────────────────
// Scans the GitHub release assets and returns the download URL for the asset
// whose name contains a platform-specific keyword.
QString Updater::platformAssetUrl(const QJsonArray &assets, QString *outFilename)
{
#if defined(Q_OS_MACOS)
    const QStringList keywords = { "macos", "mac", "darwin", "osx" };
#elif defined(Q_OS_LINUX)
    const QStringList keywords = { "linux", "appimage" };
#elif defined(Q_OS_WIN)
    const QStringList keywords = { "windows", "win" };
#else
    const QStringList keywords;
#endif

    for (const QJsonValue &v : assets) {
        const QJsonObject asset = v.toObject();
        const QString     name  = asset["name"].toString();
        const QString     lower = name.toLower();
        for (const QString &kw : keywords) {
            if (lower.contains(kw)) {
                if (outFilename) *outFilename = name;
                return asset["browser_download_url"].toString();
            }
        }
    }
    return {};
}

// ─── Slot: release metadata response received ─────────────────────────────────
void Updater::onCheckFinished()
{
    if (!m_checkReply) return;
    QNetworkReply *reply = m_checkReply;
    m_checkReply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (!m_silent) {
            QMessageBox::warning(m_parentWidget, "Update Check Failed",
                "Could not reach the update server.\n\n" + reply->errorString());
        }
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    const QString     tag   = root["tag_name"].toString().trimmed();
    const QString     notes = root["body"].toString().trimmed();
    const QJsonArray  assets = root["assets"].toArray();

    if (tag.isEmpty()) {
        if (!m_silent)
            QMessageBox::information(m_parentWidget, "Check for Updates",
                "No releases were found for this repository.");
        return;
    }

    if (!isNewer(tag, currentVersion())) {
        if (!m_silent)
            QMessageBox::information(m_parentWidget, "Up to Date",
                QString("You are running the latest version (%1).").arg(currentVersion()));
        return;
    }

    QString filename;
    const QString assetUrl = platformAssetUrl(assets, &filename);
    promptDownload(tag, notes, assetUrl, filename);
}

// ─── Show the "update available" dialog ──────────────────────────────────────
void Updater::promptDownload(const QString &tag, const QString &releaseNotes,
                              const QString &assetUrl, const QString &assetFilename)
{
    QString notes = releaseNotes;
    if (notes.length() > 800)
        notes = notes.left(800) + QStringLiteral("\n…\n(see GitHub for the full changelog)");

    const QString msg =
        QStringLiteral("<b>Aetherion %1 is available!</b><br><br>"
                       "You are currently on version <b>%2</b>.<br><br>%3")
            .arg(tag, currentVersion(),
                 notes.isEmpty() ? QString()
                                 : "<b>Release notes:</b><pre>"
                                   + notes.toHtmlEscaped() + "</pre>");

    QMessageBox dlg(m_parentWidget);
    dlg.setWindowTitle("Update Available");
    dlg.setTextFormat(Qt::RichText);
    dlg.setText(msg);
    dlg.setIcon(QMessageBox::Information);

    QPushButton *updateBtn  = dlg.addButton("Update Now",       QMessageBox::AcceptRole);
    QPushButton *browserBtn = dlg.addButton("Open in Browser",  QMessageBox::HelpRole);
    /* later */               dlg.addButton("Later",            QMessageBox::RejectRole);
    dlg.setDefaultButton(updateBtn);
    dlg.exec();

    if (dlg.clickedButton() == updateBtn) {
        if (assetUrl.isEmpty()) {
            // No platform-specific asset found — send user to the releases page.
            QDesktopServices::openUrl(QUrl(releasesPageUrl(tag)));
            return;
        }
        // Persist tag and notes so applyAndRelaunch() can save them for the
        // post-relaunch changelog dialog.
        m_pendingTag   = tag;
        m_pendingNotes = releaseNotes;
        startDownload(assetUrl,
                      assetFilename.isEmpty() ? QStringLiteral("aetherion_update") : assetFilename);

    } else if (dlg.clickedButton() == browserBtn) {
        QDesktopServices::openUrl(QUrl(releasesPageUrl(tag)));
    }
}

// ─── Download the asset ───────────────────────────────────────────────────────
void Updater::startDownload(const QString &url, const QString &filename)
{
    const QString tempDir = QDir::tempPath() + "/aetherion_update";
    QDir().mkpath(tempDir);
    m_dlPath = tempDir + "/" + filename;

    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("AetherionSuite/" AETHERION_VERSION));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    m_dlReply = m_nam->get(req);
    connect(m_dlReply, &QNetworkReply::downloadProgress,
            this,      &Updater::onDownloadProgress);
    connect(m_dlReply, &QNetworkReply::finished,
            this,      &Updater::onDownloadFinished);

    m_progress = new QProgressDialog("Downloading update…", "Cancel", 0, 0, m_parentWidget);
    m_progress->setWindowTitle("Updating Aetherion");
    m_progress->setWindowModality(Qt::ApplicationModal);
    m_progress->setMinimumDuration(0);
    m_progress->setValue(0);
    m_progress->show();

    connect(m_progress, &QProgressDialog::canceled, this, [this]() {
        if (m_dlReply) m_dlReply->abort();
    });
}

void Updater::onDownloadProgress(qint64 received, qint64 total)
{
    if (!m_progress) return;
    if (total > 0) {
        m_progress->setMaximum(static_cast<int>(total / 1024));
        m_progress->setValue(static_cast<int>(received / 1024));
        m_progress->setLabelText(
            QString("Downloading… %1 / %2 MB")
                .arg(received / 1'048'576.0, 0, 'f', 1)
                .arg(total    / 1'048'576.0, 0, 'f', 1));
    } else {
        m_progress->setMaximum(0); // indeterminate spinner
    }
}

void Updater::onDownloadFinished()
{
    if (!m_dlReply) return;
    QNetworkReply *reply = m_dlReply;
    m_dlReply = nullptr;

    if (m_progress) {
        m_progress->close();
        m_progress->deleteLater();
        m_progress = nullptr;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (reply->error() != QNetworkReply::OperationCanceledError) {
            QMessageBox::warning(m_parentWidget, "Download Failed",
                "The update download failed:\n\n" + reply->errorString());
        }
        return;
    }

    // Write the downloaded bytes to disk.
    QFile f(m_dlPath);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(m_parentWidget, "Update Failed",
            "Could not write the downloaded file to:\n" + m_dlPath);
        return;
    }
    f.write(reply->readAll());
    f.close();

    applyAndRelaunch(m_dlPath);
}

// ─── Apply update and relaunch ────────────────────────────────────────────────
//
// On macOS and Linux a small shell script is written to /tmp.  The script:
//   1. Waits for this process (by PID) to fully exit.
//   2. Extracts the archive and replaces the installed binary / .app bundle.
//   3. Relaunches the new version.
//   4. Cleans up the temp files including itself.
//
// On Windows the downloaded installer is opened directly; the user completes
// the installation manually before relaunching.
// ─────────────────────────────────────────────────────────────────────────────
void Updater::applyAndRelaunch(const QString &filePath)
{
#if defined(Q_OS_WIN)
    QMessageBox::information(m_parentWidget, "Update Downloaded",
        "The update has been downloaded to:\n\n" + filePath +
        "\n\nClose Aetherion and run the installer to complete the update.");
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    return;

#else  // macOS and Linux

    const qint64  pid   = QCoreApplication::applicationPid();
    const QString lower = filePath.toLower();
    const bool isTarGz   = lower.endsWith(".tar.gz") || lower.endsWith(".tgz");
    const bool isAppImage = lower.endsWith(".appimage");
    const bool isZip      = lower.endsWith(".zip");

    // ── Determine install target ─────────────────────────────────────────────
#if defined(Q_OS_MACOS)
    // If running inside an .app bundle the directory layout is:
    //   .../Aetherion.app/Contents/MacOS/blackhole-sim   ← applicationDirPath()
    // We want to replace the whole .app bundle.
    QDir macDir(QApplication::applicationDirPath());
    const bool inBundle =
        macDir.dirName() == QLatin1String("MacOS") &&
        QFileInfo(macDir.absolutePath()).dir().dirName() == QLatin1String("Contents");

    QString installTarget; // full path to the thing being replaced
    if (inBundle) {
        macDir.cdUp(); // …/Contents
        macDir.cdUp(); // …/Aetherion.app
        installTarget = macDir.absolutePath();
    } else {
        installTarget = QApplication::applicationFilePath();
    }
#else  // Linux
    const QString installTarget = QApplication::applicationFilePath();
    constexpr bool inBundle = false;
#endif

    // ── Write the helper script ──────────────────────────────────────────────
    const QString scriptPath  = QDir::tempPath() + "/aetherion_update_apply.sh";
    const QString extractDir  = QDir::tempPath() + "/aetherion_update_extracted";

    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::critical(m_parentWidget, "Update Failed",
            "Could not write the update helper script to:\n" + scriptPath);
        return;
    }

    QTextStream out(&script);
    out << "#!/bin/bash\n"
        << "set -e\n\n"
        << "# Wait for the running Aetherion process (PID " << pid << ") to exit.\n"
        << "while kill -0 " << pid << " 2>/dev/null; do sleep 0.3; done\n\n";

    if (isTarGz) {
        out << "mkdir -p " << sq(extractDir) << "\n"
            << "tar -xzf " << sq(filePath) << " -C " << sq(extractDir) << "\n";

#if defined(Q_OS_MACOS)
        if (inBundle) {
            out << "NEW_APP=$(find " << sq(extractDir) << " -maxdepth 3 -name '*.app' | head -1)\n"
                << "[ -z \"$NEW_APP\" ] && echo 'ERROR: no .app bundle found in archive' && exit 1\n"
                << "rm -rf "    << sq(installTarget) << "\n"
                << "cp -a \"$NEW_APP\" " << sq(installTarget) << "\n"
                << "open -n "  << sq(installTarget) << "\n";
        } else {
            out << "NEW_BIN=$(find " << sq(extractDir) << " -maxdepth 3 -perm +111 -type f | head -1)\n"
                << "[ -z \"$NEW_BIN\" ] && echo 'ERROR: no executable found in archive' && exit 1\n"
                << "cp \"$NEW_BIN\" " << sq(installTarget) << "\n"
                << "chmod +x "   << sq(installTarget) << "\n"
                << sq(installTarget) << " &\n";
        }
#else  // Linux
        out << "NEW_BIN=$(find " << sq(extractDir) << " -maxdepth 3 -executable -type f | head -1)\n"
            << "[ -z \"$NEW_BIN\" ] && echo 'ERROR: no executable found in archive' && exit 1\n"
            << "cp \"$NEW_BIN\" " << sq(installTarget) << "\n"
            << "chmod +x "   << sq(installTarget) << "\n"
            << sq(installTarget) << " &\n";
#endif

    } else if (isAppImage) {
        out << "cp "      << sq(filePath)      << " " << sq(installTarget) << "\n"
            << "chmod +x " << sq(installTarget) << "\n"
            << sq(installTarget) << " &\n";

    } else if (isZip) {
        out << "mkdir -p " << sq(extractDir) << "\n"
            << "unzip -o " << sq(filePath) << " -d " << sq(extractDir) << "\n";

#if defined(Q_OS_MACOS)
        if (inBundle) {
            out << "NEW_APP=$(find " << sq(extractDir) << " -maxdepth 3 -name '*.app' | head -1)\n"
                << "[ -z \"$NEW_APP\" ] && echo 'ERROR: no .app bundle found in archive' && exit 1\n"
                << "rm -rf "    << sq(installTarget) << "\n"
                << "cp -a \"$NEW_APP\" " << sq(installTarget) << "\n"
                << "open -n "  << sq(installTarget) << "\n";
        } else {
            out << "NEW_BIN=$(find " << sq(extractDir) << " -maxdepth 3 -perm +111 -type f | head -1)\n"
                << "cp \"$NEW_BIN\" " << sq(installTarget) << "\n"
                << "chmod +x "   << sq(installTarget) << "\n"
                << sq(installTarget) << " &\n";
        }
#else  // Linux
        out << "NEW_BIN=$(find " << sq(extractDir) << " -maxdepth 3 -executable -type f | head -1)\n"
            << "[ -z \"$NEW_BIN\" ] || NEW_BIN=$(find " << sq(extractDir) << " -maxdepth 3 -type f | head -1)\n"
            << "cp \"$NEW_BIN\" " << sq(installTarget) << "\n"
            << "chmod +x "   << sq(installTarget) << "\n"
            << sq(installTarget) << " &\n";
#endif

    } else {
        // Fallback: just try to open whatever we downloaded.
        out << "open " << sq(filePath)
            << " 2>/dev/null || xdg-open " << sq(filePath) << " 2>/dev/null || true\n";
    }

    out << "\n# Clean up temp files and this script.\n"
        << "rm -rf " << sq(extractDir) << " " << sq(filePath) << " \"$0\"\n";

    script.close();
    QFile::setPermissions(scriptPath,
        QFileDevice::ReadOwner  | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
        QFileDevice::ReadGroup  | QFileDevice::ExeGroup);

    // Inform the user, then hand off to the script.
    QMessageBox::information(m_parentWidget, "Ready to Update",
        "Aetherion will now close and restart with the new version.\n\n"
        "The update will be applied automatically.");

    // Save the release notes so the new process can show a "what's new" dialog.
    if (!m_pendingTag.isEmpty()) {
        QSettings s("Aetherion", "AetherionSuite");
        s.setValue("updates/postUpdateTag",      m_pendingTag);
        s.setValue("updates/postUpdateChangelog", m_pendingNotes);
        s.sync();
    }

    QProcess::startDetached("/bin/bash", { scriptPath });
    QApplication::quit();
#endif  // Q_OS_WIN
}
