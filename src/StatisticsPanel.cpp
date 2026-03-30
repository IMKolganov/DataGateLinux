#include "StatisticsPanel.h"

#include "DatagateUtils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

QString joinApi(QString base, const QString& path)
{
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    QString p = path;
    if (p.startsWith(QLatin1Char('/'))) {
        p = p.mid(1);
    }
    return base + QLatin1Char('/') + p;
}

class TrafficBarWidget final : public QWidget {
public:
    explicit TrafficBarWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(28);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setData(const QString& title, double mb, double maxMb)
    {
        m_title = title;
        m_mb = mb;
        m_max = maxMb > 0 ? maxMb : 1.0;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRect r = rect();
        p.fillRect(r, palette().color(QPalette::Base));

        const double frac = m_max > 0 ? qBound(0.0, m_mb / m_max, 1.0) : 0.0;
        const int w = static_cast<int>(std::lround(frac * r.width()));
        QRect fill = r;
        fill.setWidth(w);
        QColor fillCol = palette().color(QPalette::Highlight);
        fillCol.setAlpha(200);
        p.fillRect(fill, fillCol);

        p.setPen(palette().color(QPalette::Text));
        const QString txt = QStringLiteral("%1 — %2 MB").arg(m_title).arg(m_mb, 0, 'f', 2);
        p.drawText(r.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, txt);
    }

private:
    QString m_title;
    double m_mb = 0;
    double m_max = 1;
};

} // namespace

StatisticsPanel::StatisticsPanel(QWidget* parent)
    : QWidget(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);

    auto* hint = new QLabel(
        QStringLiteral("Per-server traffic from the API (GET open-vpn-statistics/get/{id})."), this);
    hint->setObjectName(QStringLiteral("muted"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* row1 = new QHBoxLayout();
    row1->addWidget(new QLabel(QStringLiteral("Server:"), this));
    m_serverCombo = new QComboBox(this);
    m_serverCombo->setMinimumWidth(220);
    row1->addWidget(m_serverCombo);
    row1->addStretch();
    root->addLayout(row1);

    m_onlyMine = new QCheckBox(QStringLiteral("Only my traffic (JWT externalId)"), this);
    m_onlyMine->setChecked(false);
    root->addWidget(m_onlyMine);

    auto* loadBtn = new QPushButton(QStringLiteral("Load statistics"), this);
    loadBtn->setProperty("primary", true);
    root->addWidget(loadBtn);

    m_status = new QLabel(QStringLiteral(""), this);
    m_status->setObjectName(QStringLiteral("muted"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* host = new QWidget(scroll);
    m_rowsLayout = new QVBoxLayout(host);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(8);
    m_rowsLayout->addStretch();
    scroll->setWidget(host);
    root->addWidget(scroll, 1);

    connect(loadBtn, &QPushButton::clicked, this, &StatisticsPanel::onLoadClicked);
}

void StatisticsPanel::setServers(const QVector<QPair<int, QString>>& idAndName)
{
    m_servers = idAndName;
    if (!m_serverCombo) {
        return;
    }
    m_serverCombo->clear();
    for (const auto& p : idAndName) {
        if (p.first > 0) {
            m_serverCombo->addItem(p.second.isEmpty() ? QStringLiteral("Server %1").arg(p.first) : p.second,
                p.first);
        }
    }
}

void StatisticsPanel::loadStatistics(const QString& apiBaseUrl, const QString& bearerAccessToken)
{
    m_apiBase = apiBaseUrl.trimmed();
    m_bearer = bearerAccessToken.trimmed();
    if (!m_bearer.isEmpty() && m_status) {
        m_status->setText(QStringLiteral("Ready. Choose a server and tap Load statistics."));
    }
}

void StatisticsPanel::finishWithError(const QString& text)
{
    if (m_status) {
        m_status->setText(text);
    }
}

void StatisticsPanel::renderRows(const QVector<QPair<QString, double>>& items, double maxMb)
{
    if (!m_rowsLayout) {
        return;
    }
    while (QLayoutItem* it = m_rowsLayout->takeAt(0)) {
        if (it->widget()) {
            it->widget()->deleteLater();
        }
        delete it;
    }
    if (items.isEmpty()) {
        m_rowsLayout->addStretch();
        return;
    }
    QWidget* parentW = m_rowsLayout->parentWidget();
    for (const auto& row : items) {
        auto* bar = new TrafficBarWidget(parentW);
        bar->setData(row.first, row.second, maxMb);
        m_rowsLayout->addWidget(bar);
    }
    m_rowsLayout->addStretch();
}

void StatisticsPanel::onLoadClicked()
{
    if (m_apiBase.isEmpty() || m_bearer.isEmpty()) {
        finishWithError(QStringLiteral("Sign in and ensure Api:BaseUrl is set."));
        return;
    }
    const int sid = m_serverCombo ? m_serverCombo->currentData().toInt() : 0;
    if (sid <= 0) {
        finishWithError(QStringLiteral("Choose a VPN server (refresh list on Access if empty)."));
        return;
    }

    finishWithError(QStringLiteral("Loading…"));
    QUrl url(joinApi(m_apiBase, QStringLiteral("api/open-vpn-statistics/get/%1").arg(sid)));
    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_bearer).toUtf8());

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finishWithError(QStringLiteral("Error: %1").arg(reply->errorString()));
            return;
        }
        const QByteArray body = reply->readAll();
        QJsonParseError jerr{};
        const QJsonDocument doc = QJsonDocument::fromJson(body, &jerr);
        if (jerr.error != QJsonParseError::NoError || !doc.isObject()) {
            finishWithError(QStringLiteral("Invalid JSON."));
            return;
        }
        const QJsonObject rootObj = doc.object();
        if (!rootObj.value(QStringLiteral("success")).toBool(false)) {
            finishWithError(
                QStringLiteral("API: %1").arg(rootObj.value(QStringLiteral("message")).toString()));
            return;
        }
        QJsonObject data = rootObj.value(QStringLiteral("data")).toObject();
        QJsonArray arr = data.value(QStringLiteral("clientTraffics")).toArray();
        if (arr.isEmpty()) {
            arr = rootObj.value(QStringLiteral("clientTraffics")).toArray();
        }

        QString filterExt;
        if (m_onlyMine && m_onlyMine->isChecked()) {
            filterExt = DatagateUtils::externalIdFromJwt(m_bearer);
        }

        QVector<QPair<QString, double>> items;
        double maxMb = 1.0;
        for (const QJsonValue& v : arr) {
            const QJsonObject o = v.toObject();
            const QString ext = o.value(QStringLiteral("externalId")).toString();
            if (!filterExt.isEmpty() && ext != filterExt) {
                continue;
            }
            const QString cn = o.value(QStringLiteral("commonName")).toString();
            const QString label = cn.isEmpty()
                ? (ext.isEmpty() ? QStringLiteral("(client)") : ext)
                : cn;
            double mb = o.value(QStringLiteral("totalMbTraffic")).toDouble();
            if (mb <= 0) {
                mb = o.value(QStringLiteral("totalMbTraffic")).toString().toDouble();
            }
            items.push_back(qMakePair(label, mb));
            if (mb > maxMb) {
                maxMb = mb;
            }
        }

        if (items.isEmpty()) {
            finishWithError(filterExt.isEmpty()
                    ? QStringLiteral("No clientTraffics rows in the response.")
                    : QStringLiteral("No rows for your externalId in this response."));
            renderRows({}, 1);
            return;
        }

        if (m_status) {
            m_status->setText(QStringLiteral("Loaded %1 row(s).").arg(items.size()));
        }
        renderRows(items, maxMb);
    });
}
