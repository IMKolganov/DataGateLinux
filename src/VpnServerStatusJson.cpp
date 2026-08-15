#include "DatagateUtils.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QPair>
#include <QString>
#include <QVector>

#include <algorithm>
#include <optional>

Q_LOGGING_CATEGORY(lcVpnStatusJson, "datagate.vpn_status_json")

namespace {

int readServerType(const QJsonObject& serverObj)
{
    const QJsonValue v = serverObj.contains(QStringLiteral("serverType"))
        ? serverObj.value(QStringLiteral("serverType"))
        : serverObj.value(QStringLiteral("ServerType"));
    if (v.isString()) {
        const QString s = v.toString();
        if (s.compare(QStringLiteral("Xray"), Qt::CaseInsensitive) == 0
            || s.compare(QStringLiteral("1"), Qt::CaseInsensitive) == 0) {
            return 1;
        }
        return 0;
    }
    return v.toInt(0);
}

BestServer bestServerFromStatusItem(const QJsonObject& item)
{
    const QJsonObject serverObj = item.value(QStringLiteral("openVpnServerResponses")).toObject()
        .value(QStringLiteral("openVpnServer")).toObject();
    BestServer b;
    b.serverId = serverObj.value(QStringLiteral("id")).toInt(-1);
    b.name = serverObj.value(QStringLiteral("serverName")).toString().trimmed();
    if (b.name.isEmpty()) {
        b.name = QStringLiteral("Server #%1").arg(b.serverId);
    }
    b.apiUrl = serverObj.value(QStringLiteral("apiUrl")).toString();
    b.isOnline = serverObj.value(QStringLiteral("isOnline")).toBool(false);
    b.serverType = readServerType(serverObj);
    b.countConnectedClients = item.value(QStringLiteral("countConnectedClients")).toInt(0);
    if (b.countConnectedClients < 0) {
        b.countConnectedClients = 0;
    }
    {
        const QJsonValue v1 = serverObj.value(QStringLiteral("isAccessibleForUserQuotaPlan"));
        const QJsonValue v2 = serverObj.value(QStringLiteral("IsAccessibleForUserQuotaPlan"));
        if (v1.isUndefined() && v2.isUndefined()) {
            b.isAccessibleForUserQuotaPlan = true;
        } else if (!v1.isUndefined()) {
            b.isAccessibleForUserQuotaPlan = v1.toBool(true);
        } else {
            b.isAccessibleForUserQuotaPlan = v2.toBool(true);
        }
    }
    return b;
}

bool isWssOpenVpnCandidate(const QJsonObject& serverObj, const BestServer& b)
{
    if (b.isXray()) {
        return false;
    }
    if (!serverObj.value(QStringLiteral("isEnableWss")).toBool(false)) {
        return false;
    }
    return b.serverId >= 0 && !b.apiUrl.isEmpty();
}

bool isManualConnectCandidate(const QJsonObject& serverObj, const BestServer& b)
{
    if (b.serverId < 0) {
        return false;
    }
    if (b.isXray()) {
        return true;
    }
    return isWssOpenVpnCandidate(serverObj, b);
}

std::optional<QVector<BestServer>> parseWssServersFromStatusJson(const QByteArray& jsonBody)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBody, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcVpnStatusJson) << "parseWssServersFromStatusJson: invalid JSON" << err.errorString();
        return std::nullopt;
    }
    const QJsonObject root = doc.object();
    if (!root.value(QStringLiteral("success")).toBool(false)) {
        qCWarning(lcVpnStatusJson) << "parseWssServersFromStatusJson: success=false"
                                   << root.value(QStringLiteral("message")).toString();
        return std::nullopt;
    }
    const QJsonArray arr = root.value(QStringLiteral("data")).toObject()
                               .value(QStringLiteral("openVpnServerWithStatuses"))
                               .toArray();

    QVector<BestServer> ranked;
    ranked.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        const QJsonObject item = v.toObject();
        const QJsonObject serverObj = item.value(QStringLiteral("openVpnServerResponses")).toObject()
                                          .value(QStringLiteral("openVpnServer"))
                                          .toObject();
        const BestServer b = bestServerFromStatusItem(item);
        if (!isWssOpenVpnCandidate(serverObj, b)) {
            continue;
        }
        ranked.push_back(b);
    }
    if (ranked.isEmpty()) {
        return std::nullopt;
    }
    return ranked;
}

std::optional<QVector<BestServer>> parseManualConnectServersFromStatusJson(const QByteArray& jsonBody)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBody, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }
    const QJsonObject root = doc.object();
    if (!root.value(QStringLiteral("success")).toBool(false)) {
        return std::nullopt;
    }
    const QJsonArray arr = root.value(QStringLiteral("data")).toObject()
                               .value(QStringLiteral("openVpnServerWithStatuses"))
                               .toArray();

    QVector<BestServer> ranked;
    ranked.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        const QJsonObject item = v.toObject();
        const QJsonObject serverObj = item.value(QStringLiteral("openVpnServerResponses")).toObject()
                                          .value(QStringLiteral("openVpnServer"))
                                          .toObject();
        const BestServer b = bestServerFromStatusItem(item);
        if (!isManualConnectCandidate(serverObj, b)) {
            continue;
        }
        ranked.push_back(b);
    }
    if (ranked.isEmpty()) {
        return std::nullopt;
    }
    return ranked;
}

} // namespace

namespace DatagateUtils {

std::optional<BestServer> pickBestServerWinStyle(const QByteArray& jsonBody)
{
    const auto rankedOpt = parseWssServersFromStatusJson(jsonBody);
    if (!rankedOpt.has_value()) {
        return std::nullopt;
    }
    QVector<BestServer> ranked;
    ranked.reserve(rankedOpt->size());
    for (const BestServer& b : *rankedOpt) {
        if (b.isAccessibleForUserQuotaPlan) {
            ranked.push_back(b);
        }
    }
    if (ranked.isEmpty()) {
        qCWarning(lcVpnStatusJson) << "pickBestServerWinStyle: no WSS servers allowed by user quota plan";
        return std::nullopt;
    }

    std::sort(ranked.begin(), ranked.end(), [](const BestServer& a, const BestServer& b) {
        if (a.isOnline != b.isOnline) {
            return a.isOnline > b.isOnline;
        }
        return a.countConnectedClients < b.countConnectedClients;
    });

    const int last = loadLastSelectedServerId();
    int index = 0;
    if (ranked.size() > 1 && last >= 0) {
        for (int i = 0; i < ranked.size(); ++i) {
            if (ranked[i].serverId == last) {
                index = (i + 1) % ranked.size();
                break;
            }
        }
    }
    const BestServer chosen = ranked[index];
    saveLastSelectedServerId(chosen.serverId);
    return chosen;
}

QVector<QPair<int, QString>> listWssServersFromStatusJson(const QByteArray& jsonBody)
{
    const auto rankedOpt = parseWssServersFromStatusJson(jsonBody);
    if (!rankedOpt.has_value()) {
        return {};
    }
    QVector<QPair<int, QString>> out;
    out.reserve(rankedOpt->size());
    for (const BestServer& b : *rankedOpt) {
        out.push_back(qMakePair(b.serverId, b.name));
    }
    std::sort(out.begin(), out.end(), [](const QPair<int, QString>& a, const QPair<int, QString>& b) {
        return a.second.localeAwareCompare(b.second) < 0;
    });
    return out;
}

QVector<BestServer> listManualConnectServersFromStatusJson(const QByteArray& jsonBody)
{
    const auto rankedOpt = parseManualConnectServersFromStatusJson(jsonBody);
    if (!rankedOpt.has_value()) {
        return {};
    }
    QVector<BestServer> out = *rankedOpt;
    std::sort(out.begin(), out.end(), [](const BestServer& a, const BestServer& b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return out;
}

std::optional<BestServer> pickServerByIdFromStatusJson(const QByteArray& jsonBody, int serverId)
{
    const auto rankedOpt = parseManualConnectServersFromStatusJson(jsonBody);
    if (!rankedOpt.has_value()) {
        return std::nullopt;
    }
    for (const BestServer& b : *rankedOpt) {
        if (b.serverId == serverId) {
            saveLastSelectedServerId(b.serverId);
            return b;
        }
    }
    return std::nullopt;
}

} // namespace DatagateUtils
