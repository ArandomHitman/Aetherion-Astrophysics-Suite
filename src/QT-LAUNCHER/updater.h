#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

class QNetworkReply;
class QProgressDialog;
class QWidget;
class QJsonArray;

// ─── Version string ──────────────────────────────────────────────────────────
// AETHERION_VERSION is injected by CMake via target_compile_definitions.
// Fallback for direct includes (e.g. unit tests).
#ifndef AETHERION_VERSION
#  define AETHERION_VERSION "0.1.0"
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Updater
//
// Queries the GitHub Releases API for this repository, compares the latest
// release tag against the compiled-in AETHERION_VERSION, and — if a newer
// version exists — prompts the user to download it.
//
// On confirmation the asset for the current platform is downloaded with a
// progress dialog.  Once the download is complete a small helper shell script
// is written to /tmp, Aetherion quits, the script waits for the process to
// fully exit, installs the new binary / .app bundle in-place, and relaunches.
//
// Usage (in MainWindow):
//   m_updater = new Updater(this);
//   // silent = true → only prompt when an update is found (for startup check)
//   QTimer::singleShot(3000, m_updater, [this]{ m_updater->checkForUpdates(true); });
//   // silent = false → always show a result dialog (for "Check for Updates…" button)
//   connect(checkBtn, &QPushButton::clicked, this, [this]{
//       m_updater->checkForUpdates(false);
//   });
// ─────────────────────────────────────────────────────────────────────────────
class Updater : public QObject
{
    Q_OBJECT

public:
    explicit Updater(QWidget *parent = nullptr);

    // Kick off a background check against the GitHub API.
    // silent = true  → no dialog when already up to date (for startup check).
    // silent = false → always show a result (for manual "Check for Updates").
    void checkForUpdates(bool silent = true);

    // Returns the compiled-in version string (e.g. "0.1.0").
    static QString currentVersion();

private slots:
    void onCheckFinished();
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();

private:
    // Returns true when `remote` (e.g. "v1.2.3") is strictly newer than
    // `local` (e.g. "0.9.1") after stripping any leading 'v'.
    static bool isNewer(const QString &remote, const QString &local);

    // Iterates `assets` and returns the browser_download_url of the first
    // asset whose name contains a platform-specific keyword.
    // Sets *outFilename to the asset's file name if non-null.
    static QString platformAssetUrl(const QJsonArray &assets,
                                    QString *outFilename = nullptr);

    // Shows the "update available" dialog; if accepted, starts the download.
    void promptDownload(const QString &tag,
                        const QString &releaseNotes,
                        const QString &assetUrl,
                        const QString &assetFilename);

    // Initiates the asset download and shows the progress dialog.
    void startDownload(const QString &url, const QString &filename);

    // Writes a helper script, quits this process, and lets the script
    // replace the binary / bundle and relaunch.
    void applyAndRelaunch(const QString &filePath);

    QWidget               *m_parentWidget;
    QNetworkAccessManager *m_nam;
    QNetworkReply         *m_checkReply = nullptr;
    QNetworkReply         *m_dlReply    = nullptr;
    QProgressDialog       *m_progress   = nullptr;
    QString                m_dlPath;
    QString                m_pendingTag;    // tag of the release being installed
    QString                m_pendingNotes;  // release body saved for post-relaunch dialog
    bool                   m_silent     = true;
};
