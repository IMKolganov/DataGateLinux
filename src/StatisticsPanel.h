#pragma once

#include <QWidget>

#include <QVector>
#include <utility>

class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QVBoxLayout;
class QNetworkAccessManager;

/// Loads per-server OpenVPN statistics from the backend and shows horizontal traffic bars (QPainter).
class StatisticsPanel final : public QWidget {
    Q_OBJECT
public:
    explicit StatisticsPanel(QWidget* parent = nullptr);

    void setServers(const QVector<QPair<int, QString>>& idAndName);

public slots:
    void loadStatistics(const QString& apiBaseUrl, const QString& bearerAccessToken);

private:
    void onLoadClicked();
    void finishWithError(const QString& text);
    void renderRows(const QVector<QPair<QString, double>>& items, double maxMb);

    QVector<QPair<int, QString>> m_servers;
    QString m_apiBase;
    QString m_bearer;

    QComboBox* m_serverCombo = nullptr;
    QCheckBox* m_onlyMine = nullptr;
    QLabel* m_status = nullptr;
    QVBoxLayout* m_rowsLayout = nullptr;
    QNetworkAccessManager* m_nam = nullptr;
};
