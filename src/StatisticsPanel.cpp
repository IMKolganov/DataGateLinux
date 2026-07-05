#include "StatisticsPanel.h"

#include "DatagateTr.h"
#include "DatagateUtils.h"

#include <QVector>

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
#include <QTimeZone>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QDate>
#include <QLocale>

namespace {

/// Y values are either byte counts (traffic) or unitless (active clients).
class StatsLineChartWidget final : public QWidget {
public:
    explicit StatsLineChartWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(240);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    }

    void setSeries(const QVector<QPair<QString, double>>& items, double maxY, bool yAxisIsBytes, const QString& yTitle)
    {
        m_items = items;
        m_max = maxY > 0 ? maxY : 1.0;
        m_yBytes = yAxisIsBytes;
        m_yTitle = yTitle;
        update();
    }

    void clearSeries()
    {
        m_items.clear();
        m_max = 1.0;
        m_yTitle.clear();
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
        const QString head = m_yTitle.isEmpty() ? Datagate::tr("Overview") : m_yTitle;
        p.drawText(r.adjusted(8, 4, -8, -8), Qt::AlignTop | Qt::AlignLeft, head);

        if (m_items.isEmpty()) {
            p.setPen(palette().color(QPalette::PlaceholderText));
            p.drawText(r, Qt::AlignCenter, Datagate::tr("No data"));
            return;
        }

        const int marginL = 72;
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
        const QString topLbl = m_yBytes ? DatagateUtils::formatDataSizeBytes(static_cast<qint64>(m_max))
                                        : QString::number(static_cast<qint64>(m_max));
        p.drawText(QRect(marginL - 8, 0, marginL, marginT + plot.height() - 8), Qt::AlignRight | Qt::AlignVCenter,
            topLbl);
        p.drawText(QRect(marginL - 8, plot.bottom() - 8, marginL, 24), Qt::AlignRight | Qt::AlignTop,
            m_yBytes ? DatagateUtils::formatDataSizeBytes(0) : QStringLiteral("0"));

        p.setPen(palette().color(QPalette::PlaceholderText));
        for (int i = 0; i < n; ++i) {
            const QString txt = m_items[i].first;
            const QString shortTxt = txt.size() > 8 ? txt.left(8) + QStringLiteral("…") : txt;
            const double t = n > 1 ? static_cast<double>(i) / static_cast<double>(n - 1) : 0.5;
            const int x = static_cast<int>(plot.left() + t * plot.width());
            p.drawText(QRect(x - 36, plot.bottom() + 4, 72, 28), Qt::AlignHCenter | Qt::AlignTop, shortTxt);
        }
    }

private:
    QVector<QPair<QString, double>> m_items;
    double m_max = 1.0;
    bool m_yBytes = true;
    QString m_yTitle;
};

StatsLineChartWidget* trafficChart(QWidget* w)
{
    return w ? static_cast<StatsLineChartWidget*>(w) : nullptr;
}

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

constexpr int kRangeMonthBase = 10000;
constexpr int kRangeYearBase = 20000;

/// Same range as Android [StatsViewModel.rangeForLastCalendarDays]: local midnight boundaries → UTC ISO.
QPair<QDateTime, QDateTime> rangeLastCalendarDaysUtc(int days)
{
    const QTimeZone tz = QTimeZone::systemTimeZone();
    const QDate today = QDate::currentDate();
    const QDateTime todayStart = QDateTime(today, QTime(0, 0, 0), tz);
    const QDateTime fromLocal = todayStart.addDays(-days);
    const QDateTime tomorrowStart = todayStart.addDays(1);
    const QDateTime toLocal = tomorrowStart.addMSecs(-1);
    return {fromLocal.toUTC(), toLocal.toUTC()};
}

QPair<QDateTime, QDateTime> rangeLastCalendarMonthsUtc(int months)
{
    const QTimeZone tz = QTimeZone::systemTimeZone();
    const QDate today = QDate::currentDate();
    const QDateTime todayStart = QDateTime(today, QTime(0, 0, 0), tz);
    const QDateTime fromLocal = todayStart.addMonths(-months);
    const QDateTime tomorrowStart = todayStart.addDays(1);
    const QDateTime toLocal = tomorrowStart.addMSecs(-1);
    return {fromLocal.toUTC(), toLocal.toUTC()};
}

QPair<QDateTime, QDateTime> rangeLastCalendarYearsUtc(int years)
{
    const QTimeZone tz = QTimeZone::systemTimeZone();
    const QDate today = QDate::currentDate();
    const QDateTime todayStart = QDateTime(today, QTime(0, 0, 0), tz);
    const QDateTime fromLocal = todayStart.addYears(-years);
    const QDateTime tomorrowStart = todayStart.addDays(1);
    const QDateTime toLocal = tomorrowStart.addMSecs(-1);
    return {fromLocal.toUTC(), toLocal.toUTC()};
}

QPair<QDateTime, QDateTime> rangeForPresetId(int presetId)
{
    if (presetId >= kRangeYearBase) {
        return rangeLastCalendarYearsUtc(presetId - kRangeYearBase);
    }
    if (presetId >= kRangeMonthBase) {
        return rangeLastCalendarMonthsUtc(presetId - kRangeMonthBase);
    }
    return rangeLastCalendarDaysUtc(presetId > 0 ? presetId : 7);
}

void fillRangeCombo(QComboBox* combo, int selectPresetId)
{
    if (!combo) {
        return;
    }
    combo->blockSignals(true);
    combo->clear();
    combo->addItem(Datagate::tr("Last 7 days"), 7);
    combo->addItem(Datagate::tr("Last 30 days"), 30);
    combo->addItem(Datagate::tr("Last 2 months"), kRangeMonthBase + 2);
    combo->addItem(Datagate::tr("Last 3 months"), kRangeMonthBase + 3);
    combo->addItem(Datagate::tr("Last 6 months"), kRangeMonthBase + 6);
    combo->addItem(Datagate::tr("Last year"), kRangeYearBase + 1);
    combo->addItem(Datagate::tr("Last 2 years"), kRangeYearBase + 2);
    int idx = 0;
    for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemData(i).toInt() == selectPresetId) {
            idx = i;
            break;
        }
    }
    combo->setCurrentIndex(idx);
    combo->blockSignals(false);
}

QString shortLabelForTs(const QString& ts)
{
    QDateTime dt = QDateTime::fromString(ts.trimmed(), Qt::ISODateWithMs);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(ts.trimmed(), Qt::ISODate);
    }
    if (!dt.isValid()) {
        return ts.left(10);
    }
    return QLocale().toString(dt.toLocalTime(), QStringLiteral("dd.MM"));
}

} // namespace

StatisticsPanel::StatisticsPanel(QWidget* parent)
    : QWidget(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);

    m_hintLabel = new QLabel(
        Datagate::tr("Your OpenVPN traffic for the selected period (same series as the Android app)."), this);
    m_hintLabel->setObjectName(QStringLiteral("muted"));
    m_hintLabel->setWordWrap(true);
    root->addWidget(m_hintLabel);

    auto* presetRow = new QHBoxLayout();
    m_rangeCombo = new QComboBox(this);
    m_rangeCombo->setMinimumWidth(220);
    presetRow->addWidget(m_rangeCombo, 1);
    root->addLayout(presetRow);

    auto* filterRow = new QHBoxLayout();
    m_groupingCombo = new QComboBox(this);
    m_groupingCombo->setMinimumWidth(160);
    m_metricCombo = new QComboBox(this);
    m_metricCombo->setMinimumWidth(180);
    filterRow->addWidget(m_groupingCombo, 1);
    filterRow->addWidget(m_metricCombo, 1);
    root->addLayout(filterRow);

    m_refreshBtn = new QPushButton(Datagate::tr("Refresh"), this);
    m_refreshBtn->setProperty("primary", true);
    root->addWidget(m_refreshBtn);

    m_status = new QLabel(QString(), this);
    m_status->setObjectName(QStringLiteral("muted"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName(QStringLiteral("muted"));
    m_summaryLabel->setWordWrap(true);
    root->addWidget(m_summaryLabel);

    m_trafficChart = new StatsLineChartWidget(this);
    root->addWidget(m_trafficChart, 1);

    connect(m_rangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { reload(); });
    connect(m_groupingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { reload(); });
    connect(m_metricCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!m_lastOverviewBody.isEmpty()) {
            applyOverviewJson(m_lastOverviewBody);
        }
    });
    connect(m_refreshBtn, &QPushButton::clicked, this, &StatisticsPanel::reload);

    retranslateUi();
}

void StatisticsPanel::retranslateUi()
{
    if (m_hintLabel) {
        m_hintLabel->setText(
            Datagate::tr("Your OpenVPN traffic for the selected period (same series as the Android app)."));
    }
    if (m_rangeCombo) {
        int prevPreset = 7;
        if (m_rangeCombo->count() > 0) {
            bool ok = false;
            const int v = m_rangeCombo->currentData().toInt(&ok);
            if (ok) {
                prevPreset = v;
            }
        }
        fillRangeCombo(m_rangeCombo, prevPreset);
    }
    if (m_refreshBtn) {
        m_refreshBtn->setText(Datagate::tr("Refresh"));
    }
    const int gix = m_groupingCombo ? m_groupingCombo->currentIndex() : 0;
    const int mix = m_metricCombo ? m_metricCombo->currentIndex() : 0;
    if (m_groupingCombo) {
        m_groupingCombo->blockSignals(true);
        m_groupingCombo->clear();
        m_groupingCombo->addItem(Datagate::tr("Grouping: Auto"), 0);
        m_groupingCombo->addItem(Datagate::tr("Grouping: Hours"), 1);
        m_groupingCombo->addItem(Datagate::tr("Grouping: Months"), 3);
        m_groupingCombo->addItem(Datagate::tr("Grouping: Years"), 4);
        m_groupingCombo->setCurrentIndex(qBound(0, gix, m_groupingCombo->count() - 1));
        m_groupingCombo->blockSignals(false);
    }
    if (m_metricCombo) {
        m_metricCombo->blockSignals(true);
        m_metricCombo->clear();
        m_metricCombo->addItem(Datagate::tr("Traffic: total"), 0);
        m_metricCombo->addItem(Datagate::tr("Traffic: in"), 1);
        m_metricCombo->addItem(Datagate::tr("Traffic: out"), 2);
        m_metricCombo->addItem(Datagate::tr("Active clients"), 3);
        m_metricCombo->setCurrentIndex(qBound(0, mix, m_metricCombo->count() - 1));
        m_metricCombo->blockSignals(false);
    }
    if (StatsLineChartWidget* c = trafficChart(m_trafficChart)) {
        c->update();
    }
}

void StatisticsPanel::loadStatistics(const QString& apiBaseUrl, const QString& bearerAccessToken)
{
    m_apiBase = apiBaseUrl.trimmed();
    m_bearer = bearerAccessToken.trimmed();
    if (m_groupingCombo && m_groupingCombo->count() == 0) {
        retranslateUi();
    }
    if (!m_bearer.isEmpty() && m_status) {
        m_status->setText(Datagate::tr("Ready."));
        reload();
    } else if (m_status) {
        m_status->setText(Datagate::tr("Sign in to load statistics."));
        if (StatsLineChartWidget* c = trafficChart(m_trafficChart)) {
            c->clearSeries();
        }
        if (m_summaryLabel) {
            m_summaryLabel->clear();
        }
    }
}

void StatisticsPanel::finishWithError(const QString& text)
{
    if (m_status) {
        m_status->setText(text);
    }
    if (text == Datagate::tr("Loading…")) {
        return;
    }
    m_lastOverviewBody.clear();
    if (m_summaryLabel) {
        m_summaryLabel->clear();
    }
    if (StatsLineChartWidget* c = trafficChart(m_trafficChart)) {
        c->clearSeries();
    }
}

void StatisticsPanel::applyOverviewJson(const QByteArray& body)
{
    QJsonParseError jerr{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &jerr);
    if (jerr.error != QJsonParseError::NoError || !doc.isObject()) {
        finishWithError(Datagate::tr("Invalid JSON."));
        return;
    }
    const QJsonObject rootObj = doc.object();
    if (!rootObj.value(QStringLiteral("success")).toBool(false)) {
        finishWithError(Datagate::tr("API: %1").arg(rootObj.value(QStringLiteral("message")).toString()));
        return;
    }
    m_lastOverviewBody = body;
    QJsonObject data = rootObj.value(QStringLiteral("data")).toObject();
    QJsonArray rows = data.value(QStringLiteral("overviewSeriesRows")).toArray();
    if (rows.isEmpty()) {
        rows = data.value(QStringLiteral("OverviewSeriesRows")).toArray();
    }

    QJsonObject summaryObj = data.value(QStringLiteral("summary")).toObject();
    if (summaryObj.isEmpty()) {
        summaryObj = data.value(QStringLiteral("Summary")).toObject();
    }
    const qint64 sumIn = static_cast<qint64>(summaryObj.value(QStringLiteral("totalTrafficInBytes")).toDouble(0));
    const qint64 sumOut = static_cast<qint64>(summaryObj.value(QStringLiteral("totalTrafficOutBytes")).toDouble(0));
    const int peak = summaryObj.value(QStringLiteral("peakActiveClients")).toInt(0);

    if (m_summaryLabel) {
        m_summaryLabel->setText(
            Datagate::tr("Period total — In: %1 · Out: %2 · Peak active clients: %3")
                .arg(DatagateUtils::formatDataSizeBytes(sumIn))
                .arg(DatagateUtils::formatDataSizeBytes(sumOut))
                .arg(peak));
    }

    const int metricId = m_metricCombo ? m_metricCombo->currentData().toInt() : 0;
    const bool yBytes = metricId != 3;

    QVector<QPair<QString, double>> items;
    double maxY = 1.0;
    for (const QJsonValue& v : rows) {
        const QJsonObject o = v.toObject();
        const QString ts = o.value(QStringLiteral("ts")).toString();
        double y = 0;
        switch (metricId) {
        case 1:
            y = static_cast<double>(o.value(QStringLiteral("trafficInBytes")).toDouble());
            break;
        case 2:
            y = static_cast<double>(o.value(QStringLiteral("trafficOutBytes")).toDouble());
            break;
        case 3:
            y = static_cast<double>(o.value(QStringLiteral("activeClients")).toInt());
            break;
        default:
            y = static_cast<double>(o.value(QStringLiteral("trafficTotalBytes")).toDouble());
            break;
        }
        items.push_back(qMakePair(shortLabelForTs(ts), y));
        if (y > maxY) {
            maxY = y;
        }
    }

    QString yTitle;
    switch (metricId) {
    case 1:
        yTitle = Datagate::tr("Traffic in");
        break;
    case 2:
        yTitle = Datagate::tr("Traffic out");
        break;
    case 3:
        yTitle = Datagate::tr("Active clients");
        break;
    default:
        yTitle = Datagate::tr("Traffic total");
        break;
    }

    if (items.isEmpty()) {
        finishWithError(Datagate::tr("No series points in the response."));
        if (StatsLineChartWidget* c = trafficChart(m_trafficChart)) {
            c->clearSeries();
        }
        return;
    }

    if (m_status) {
        m_status->setText(Datagate::tr("Loaded %1 point(s).").arg(items.size()));
    }
    if (StatsLineChartWidget* c = trafficChart(m_trafficChart)) {
        c->setSeries(items, maxY, yBytes, yTitle);
    }
}

void StatisticsPanel::reload()
{
    if (m_apiBase.isEmpty() || m_bearer.isEmpty()) {
        finishWithError(Datagate::tr("Sign in and ensure Api:BaseUrl is set."));
        return;
    }
    const QString ext = DatagateUtils::externalIdFromJwt(m_bearer).trimmed();
    if (ext.isEmpty()) {
        finishWithError(
            Datagate::tr("Statistics need an OpenVPN external ID in your token (sub / externalId)."));
        if (m_summaryLabel) {
            m_summaryLabel->clear();
        }
        if (StatsLineChartWidget* c = trafficChart(m_trafficChart)) {
            c->clearSeries();
        }
        return;
    }

    const int presetId = m_rangeCombo ? m_rangeCombo->currentData().toInt() : 7;
    const auto range = rangeForPresetId(presetId);
    const int grouping = m_groupingCombo ? m_groupingCombo->currentData().toInt() : 0;

    QUrl url(joinApi(m_apiBase, QStringLiteral("api/open-vpn-clients/overview/series")));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("From"), range.first.toString(Qt::ISODateWithMs));
    q.addQueryItem(QStringLiteral("To"), range.second.toString(Qt::ISODateWithMs));
    q.addQueryItem(QStringLiteral("Grouping"), QString::number(grouping));
    q.addQueryItem(QStringLiteral("ExternalId"), ext);
    url.setQuery(q);

    finishWithError(Datagate::tr("Loading…"));
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
        applyOverviewJson(reply->readAll());
    });
}
