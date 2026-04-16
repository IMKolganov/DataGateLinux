#include "StatisticsPanel.h"

#include "DatagateTr.h"
#include "DatagateUtils.h"

#include <QVector>

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
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

/// Line chart (polyline + area) for traffic per client; no Qt Charts dependency.
class TrafficLineChartWidget final : public QWidget {
public:
    explicit TrafficLineChartWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(200);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    }

    void setSeries(const QVector<QPair<QString, double>>& items, double maxMb)
    {
        m_items = items;
        m_max = maxMb > 0 ? maxMb : 1.0;
        update();
    }

    void clearSeries()
    {
        m_items.clear();
        m_max = 1.0;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRect r = rect();
        p.fillRect(r, palette().color(QPalette::Base));

        p.setPen(palette().color(QPalette::Text));
        p.drawText(r.adjusted(8, 4, -8, -8), Qt::AlignTop | Qt::AlignLeft,
            Datagate::tr("Traffic (MB) — per client"));

        if (m_items.isEmpty()) {
            p.setPen(palette().color(QPalette::PlaceholderText));
            p.drawText(r, Qt::AlignCenter, Datagate::tr("No data"));
            return;
        }

        const int marginL = 44;
        const int marginR = 16;
        const int marginT = 28;
        const int marginB = 36;
        const QRect plot = r.adjusted(marginL, marginT, -marginR, -marginB);
        if (plot.width() < 4 || plot.height() < 4) {
            return;
        }

        QColor gridCol = palette().color(QPalette::Mid);
        gridCol.setAlpha(80);
        p.setPen(gridCol);
        for (int i = 0; i <= 4; ++i) {
            const int y = plot.top() + (plot.height() * i) / 4;
            p.drawLine(plot.left(), y, plot.right(), y);
        }
        p.setPen(gridCol);
        for (int i = 0; i <= 4; ++i) {
            const int x = plot.left() + (plot.width() * i) / 4;
            p.drawLine(x, plot.top(), x, plot.bottom());
        }

        const double y0 = plot.bottom();
        const int n = m_items.size();
        QVector<QPointF> pts;
        pts.reserve(n);
        for (int i = 0; i < n; ++i) {
            const double t = n > 1 ? static_cast<double>(i) / static_cast<double>(n - 1) : 0.5;
            const double x = plot.left() + t * plot.width();
            const double v = m_items[i].second;
            const double y = plot.bottom() - (static_cast<double>(plot.height()) * (v / m_max));
            pts.push_back(QPointF(x, y));
        }

        QPainterPath fillPath;
        fillPath.moveTo(QPointF(pts.first().x(), y0));
        for (const QPointF& pt : pts) {
            fillPath.lineTo(pt);
        }
        fillPath.lineTo(QPointF(pts.last().x(), y0));
        fillPath.closeSubpath();
        QColor fill = palette().color(QPalette::Highlight);
        fill.setAlpha(55);
        p.fillPath(fillPath, fill);

        QPen linePen(palette().color(QPalette::Highlight));
        linePen.setWidth(2);
        p.setPen(linePen);
        for (int i = 1; i < pts.size(); ++i) {
            p.drawLine(pts[i - 1], pts[i]);
        }
        p.setBrush(palette().color(QPalette::Highlight));
        for (const QPointF& pt : pts) {
            p.drawEllipse(pt, 4.0, 4.0);
        }

        p.setPen(palette().color(QPalette::Text));
        const QFont f = p.font();
        p.setFont(QFont(f.family(), f.pointSize() > 0 ? f.pointSize() - 1 : 8));
        p.drawText(QRect(marginL - 8, 0, marginL, marginT + plot.height() - 8), Qt::AlignRight | Qt::AlignVCenter,
            QStringLiteral("%1").arg(m_max, 0, 'f', 1));
        p.drawText(QRect(marginL - 8, plot.bottom() - 8, marginL, 24), Qt::AlignRight | Qt::AlignTop,
            QStringLiteral("0"));

        p.setPen(palette().color(QPalette::PlaceholderText));
        for (int i = 0; i < n; ++i) {
            const QString txt = m_items[i].first;
            const QString shortTxt = txt.size() > 5 ? txt.left(5) + QStringLiteral("…") : txt;
            const double t = n > 1 ? static_cast<double>(i) / static_cast<double>(n - 1) : 0.5;
            const int x = static_cast<int>(plot.left() + t * plot.width());
            p.drawText(QRect(x - 28, plot.bottom() + 4, 56, 28), Qt::AlignHCenter | Qt::AlignTop, shortTxt);
        }
    }

private:
    QVector<QPair<QString, double>> m_items;
    double m_max = 1.0;
};

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
        const QString txt = Datagate::tr("%1 — %2 MB").arg(m_title).arg(m_mb, 0, 'f', 2);
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
        Datagate::tr("Per-server traffic from the API (GET open-vpn-statistics/get/{id})."), this);
    hint->setObjectName(QStringLiteral("muted"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* row1 = new QHBoxLayout();
    row1->addWidget(new QLabel(Datagate::tr("Server:"), this));
    m_serverCombo = new QComboBox(this);
    m_serverCombo->setMinimumWidth(220);
    row1->addWidget(m_serverCombo);
    row1->addStretch();
    root->addLayout(row1);

    m_onlyMine = new QCheckBox(Datagate::tr("Only my traffic (JWT externalId)"), this);
    m_onlyMine->setChecked(false);
    root->addWidget(m_onlyMine);

    auto* loadBtn = new QPushButton(Datagate::tr("Load statistics"), this);
    loadBtn->setProperty("primary", true);
    root->addWidget(loadBtn);

    m_status = new QLabel(QStringLiteral(""), this);
    m_status->setObjectName(QStringLiteral("muted"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    m_trafficChart = new TrafficLineChartWidget(this);
    root->addWidget(m_trafficChart);

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
            m_serverCombo->addItem(p.second.isEmpty() ? Datagate::tr("Server %1").arg(p.first) : p.second,
                p.first);
        }
    }
}

void StatisticsPanel::loadStatistics(const QString& apiBaseUrl, const QString& bearerAccessToken)
{
    m_apiBase = apiBaseUrl.trimmed();
    m_bearer = bearerAccessToken.trimmed();
    if (!m_bearer.isEmpty() && m_status) {
        m_status->setText(Datagate::tr("Ready. Choose a server and tap Load statistics."));
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
    if (m_trafficChart) {
        if (items.isEmpty()) {
            m_trafficChart->clearSeries();
        } else {
            m_trafficChart->setSeries(items, maxMb);
        }
    }
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
        finishWithError(Datagate::tr("Sign in and ensure Api:BaseUrl is set."));
        return;
    }
    const int sid = m_serverCombo ? m_serverCombo->currentData().toInt() : 0;
    if (sid <= 0) {
        finishWithError(Datagate::tr("Choose a VPN server (refresh list on Access if empty)."));
        return;
    }

    finishWithError(Datagate::tr("Loading…"));
    QUrl url(joinApi(m_apiBase, QStringLiteral("api/open-vpn-statistics/get/%1").arg(sid)));
    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_bearer).toUtf8());

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finishWithError(Datagate::tr("Error: %1").arg(reply->errorString()));
            return;
        }
        const QByteArray body = reply->readAll();
        QJsonParseError jerr{};
        const QJsonDocument doc = QJsonDocument::fromJson(body, &jerr);
        if (jerr.error != QJsonParseError::NoError || !doc.isObject()) {
            finishWithError(Datagate::tr("Invalid JSON."));
            return;
        }
        const QJsonObject rootObj = doc.object();
        if (!rootObj.value(QStringLiteral("success")).toBool(false)) {
            finishWithError(
                Datagate::tr("API: %1").arg(rootObj.value(QStringLiteral("message")).toString()));
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
                ? (ext.isEmpty() ? Datagate::tr("(client)") : ext)
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
                    ? Datagate::tr("No clientTraffics rows in the response.")
                    : Datagate::tr("No rows for your externalId in this response."));
            renderRows({}, 1);
            return;
        }

        if (m_status) {
            m_status->setText(Datagate::tr("Loaded %1 row(s).").arg(items.size()));
        }
        renderRows(items, maxMb);
    });
}
