#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;

/// GET https://api.github.com/repos/{owner}/{repo}/releases/latest — same idea as DataGateWin GitHubUpdateChecker.
class GitHubReleaseChecker final : public QObject {
    Q_OBJECT
public:
    explicit GitHubReleaseChecker(QNetworkAccessManager* nam, QObject* parent = nullptr);

    void start(const QString& owner, const QString& repo);

signals:
    /// Emitted once. newerAvailable is true when latest release tag parses as greater than QCoreApplication::applicationVersion().
    void finished(bool newerAvailable, const QString& latestTag, const QUrl& releasePageUrl);

private:
    QNetworkAccessManager* m_nam = nullptr;
};
