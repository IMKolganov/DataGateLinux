#pragma once

#include <QWidget>

#include <QVector>
#include <utility>

class QComboBox;
class QPushButton;
class QLabel;
class QVBoxLayout;
class QButtonGroup;
class QNetworkAccessManager;

/// User traffic time series (GET api/open-vpn-clients/overview/series) — parity with DataGateAndroid StatsScreen.
class StatisticsPanel final : public QWidget {
    Q_OBJECT
public:
    explicit StatisticsPanel(QWidget* parent = nullptr);

    /// No-op (kept for MainWindow compatibility); statistics are per-user via JWT externalId.
    void setServers(const QVector<QPair<int, QString>>&) {}

    void retranslateUi();

public slots:
    void loadStatistics(const QString& apiBaseUrl, const QString& bearerAccessToken);

private slots:
    void reload();

private:
    void finishWithError(const QString& text);
    void applyOverviewJson(const QByteArray& body);

    QString m_apiBase;
    QString m_bearer;

    QLabel* m_hintLabel = nullptr;
    QPushButton* m_range7Btn = nullptr;
    QPushButton* m_range30Btn = nullptr;
    QButtonGroup* m_rangeGroup = nullptr;
    int m_presetDays = 7;

    QComboBox* m_groupingCombo = nullptr;
    QComboBox* m_metricCombo = nullptr;

    QPushButton* m_refreshBtn = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QWidget* m_trafficChart = nullptr;

    QNetworkAccessManager* m_nam = nullptr;
    QByteArray m_lastOverviewBody;
};
