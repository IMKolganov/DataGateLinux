#include "LocalProfileStore.h"
#include "AppConfig.h"
#include "DatagateTr.h"
#include "DatagateUtils.h"
#include "XrayConfigBuilder.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

namespace {

QString indexPath()
{
    return QDir(LocalProfileStore::profilesRoot()).filePath(QStringLiteral("index.json"));
}

bool ensureProfilesRoot(QString* errorOut)
{
    const QString root = LocalProfileStore::profilesRoot();
    if (root.isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("App local data path is unavailable.");
        }
        return false;
    }
    QDir dir(root);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (errorOut) {
            *errorOut = Datagate::tr("Could not create profiles directory.");
        }
        return false;
    }
    return true;
}

QJsonArray loadIndexArray()
{
    QFile f(indexPath());
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    return doc.object().value(QStringLiteral("profiles")).toArray();
}

bool saveIndexArray(const QJsonArray& arr, QString* errorOut)
{
    if (!ensureProfilesRoot(errorOut)) {
        return false;
    }
    QJsonObject root;
    root.insert(QStringLiteral("profiles"), arr);
    QFile f(indexPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut) {
            *errorOut = Datagate::tr("Could not write profiles index.");
        }
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

LocalProfileStore::Profile profileFromJson(const QJsonObject& o)
{
    LocalProfileStore::Profile p;
    p.id = o.value(QStringLiteral("id")).toString();
    p.name = o.value(QStringLiteral("name")).toString();
    p.type = LocalProfileStore::typeFromString(o.value(QStringLiteral("type")).toString());
    p.createdUtc = o.value(QStringLiteral("createdUtc")).toString();
    p.updatedUtc = o.value(QStringLiteral("updatedUtc")).toString();
    return p;
}

QJsonObject profileToJson(const LocalProfileStore::Profile& p)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), p.id);
    o.insert(QStringLiteral("name"), p.name);
    o.insert(QStringLiteral("type"), LocalProfileStore::typeToString(p.type));
    o.insert(QStringLiteral("createdUtc"), p.createdUtc);
    o.insert(QStringLiteral("updatedUtc"), p.updatedUtc);
    return o;
}

bool writeProfileMeta(const LocalProfileStore::Profile& p, QString* errorOut)
{
    const QString dir = LocalProfileStore::profileDir(p.id);
    if (!QDir().mkpath(dir)) {
        if (errorOut) {
            *errorOut = Datagate::tr("Could not create profile folder.");
        }
        return false;
    }
    QFile f(QDir(dir).filePath(QStringLiteral("profile.json")));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut) {
            *errorOut = Datagate::tr("Could not write profile.json.");
        }
        return false;
    }
    f.write(QJsonDocument(profileToJson(p)).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace

QString LocalProfileStore::typeToString(Type t)
{
    switch (t) {
    case Type::Xray:
        return QStringLiteral("xray");
    case Type::OpenVpn:
    default:
        return QStringLiteral("openvpn");
    }
}

LocalProfileStore::Type LocalProfileStore::typeFromString(const QString& s)
{
    if (s.compare(QStringLiteral("xray"), Qt::CaseInsensitive) == 0) {
        return Type::Xray;
    }
    return Type::OpenVpn;
}

QString LocalProfileStore::profilesRoot()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        return {};
    }
    return QDir(base).filePath(QStringLiteral("profiles"));
}

QString LocalProfileStore::profileDir(const QString& id)
{
    return QDir(profilesRoot()).filePath(id);
}

QVector<LocalProfileStore::Profile> LocalProfileStore::list()
{
    QVector<Profile> out;
    const QJsonArray arr = loadIndexArray();
    out.reserve(arr.size());
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) {
            continue;
        }
        Profile p = profileFromJson(v.toObject());
        if (!p.id.isEmpty()) {
            out.push_back(std::move(p));
        }
    }
    return out;
}

std::optional<LocalProfileStore::Profile> LocalProfileStore::find(const QString& id)
{
    if (id.isEmpty()) {
        return std::nullopt;
    }
    for (const Profile& p : list()) {
        if (p.id == id) {
            return p;
        }
    }
    return std::nullopt;
}

std::optional<LocalProfileStore::Profile> LocalProfileStore::importOvpn(const QString& sourceFilePath,
    const QString& displayName,
    QString* errorOut)
{
    QFile in(sourceFilePath);
    if (!in.open(QIODevice::ReadOnly)) {
        if (errorOut) {
            *errorOut = Datagate::tr("Could not read the selected file.");
        }
        return std::nullopt;
    }
    const QByteArray bytes = in.readAll();
    const QString text = QString::fromUtf8(bytes);
    if (text.trimmed().isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("The OpenVPN file is empty.");
        }
        return std::nullopt;
    }
    const ParsedRemote remote = DatagateUtils::parseFirstRemoteFromOvpn(text);
    if (remote.host.isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("No valid remote line found in the .ovpn file.");
        }
        return std::nullopt;
    }

    QString err;
    if (!ensureProfilesRoot(&err)) {
        if (errorOut) {
            *errorOut = err;
        }
        return std::nullopt;
    }

    Profile p;
    p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString name = displayName.trimmed();
    if (name.isEmpty()) {
        name = QFileInfo(sourceFilePath).completeBaseName();
    }
    if (name.isEmpty()) {
        name = Datagate::tr("OpenVPN profile");
    }
    p.name = name;
    p.type = Type::OpenVpn;
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    p.createdUtc = now;
    p.updatedUtc = now;

    if (!writeProfileMeta(p, errorOut)) {
        return std::nullopt;
    }

    QFile out(QDir(profileDir(p.id)).filePath(QStringLiteral("config.ovpn")));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QDir(profileDir(p.id)).removeRecursively();
        if (errorOut) {
            *errorOut = Datagate::tr("Could not write config.ovpn.");
        }
        return std::nullopt;
    }
    out.write(bytes);

    QJsonArray arr = loadIndexArray();
    arr.append(profileToJson(p));
    if (!saveIndexArray(arr, errorOut)) {
        QDir(profileDir(p.id)).removeRecursively();
        return std::nullopt;
    }
    return p;
}

std::optional<LocalProfileStore::Profile> LocalProfileStore::importXrayJsonText(const QString& jsonText,
    const QString& displayName,
    QString* errorOut)
{
    QString err;
    const QString normalized = XrayConfigBuilder::clientConfigFromOpaqueText(
        jsonText, AppConfig::xraySocksPort(), AppConfig::xrayUseTun(), &err);
    if (normalized.isEmpty()) {
        if (errorOut) {
            *errorOut = err;
        }
        return std::nullopt;
    }
    if (!ensureProfilesRoot(&err)) {
        if (errorOut) {
            *errorOut = err;
        }
        return std::nullopt;
    }

    Profile p;
    p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString name = displayName.trimmed();
    if (name.isEmpty()) {
        name = Datagate::tr("Xray profile");
    }
    p.name = name;
    p.type = Type::Xray;
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    p.createdUtc = now;
    p.updatedUtc = now;

    if (!writeProfileMeta(p, errorOut)) {
        return std::nullopt;
    }
    QFile out(QDir(profileDir(p.id)).filePath(QStringLiteral("config.json")));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QDir(profileDir(p.id)).removeRecursively();
        if (errorOut) {
            *errorOut = Datagate::tr("Could not write config.json.");
        }
        return std::nullopt;
    }
    out.write(normalized.toUtf8());

    QJsonArray arr = loadIndexArray();
    arr.append(profileToJson(p));
    if (!saveIndexArray(arr, errorOut)) {
        QDir(profileDir(p.id)).removeRecursively();
        return std::nullopt;
    }
    return p;
}

std::optional<LocalProfileStore::Profile> LocalProfileStore::upsertXrayJsonText(const QString& jsonText,
    const QString& displayName,
    QString* errorOut)
{
    QString err;
    const QString normalized = XrayConfigBuilder::clientConfigFromOpaqueText(
        jsonText, AppConfig::xraySocksPort(), AppConfig::xrayUseTun(), &err);
    if (normalized.isEmpty()) {
        if (errorOut) {
            *errorOut = err;
        }
        return std::nullopt;
    }

    const QString name = displayName.trimmed().isEmpty() ? Datagate::tr("Xray profile") : displayName.trimmed();
    for (const Profile& existing : list()) {
        if (existing.type != Type::Xray || existing.name != name) {
            continue;
        }
        QFile out(xrayConfigPath(existing.id));
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (errorOut) {
                *errorOut = Datagate::tr("Could not write config.json.");
            }
            return std::nullopt;
        }
        out.write(normalized.toUtf8());
        Profile updated = existing;
        updated.updatedUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        if (!writeProfileMeta(updated, errorOut)) {
            return std::nullopt;
        }
        QJsonArray arr = loadIndexArray();
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject o = arr.at(i).toObject();
            if (o.value(QStringLiteral("id")).toString() == updated.id) {
                arr.replace(i, profileToJson(updated));
                break;
            }
        }
        if (!saveIndexArray(arr, errorOut)) {
            return std::nullopt;
        }
        return updated;
    }
    return importXrayJsonText(normalized, name, errorOut);
}

std::optional<LocalProfileStore::Profile> LocalProfileStore::importXrayUri(const QString& vlessUri,
    const QString& displayName,
    QString* errorOut)
{
    QString err;
    const QString json = XrayConfigBuilder::clientConfigFromOpaqueText(
        vlessUri, AppConfig::xraySocksPort(), AppConfig::xrayUseTun(), &err);
    if (json.isEmpty()) {
        if (errorOut) {
            *errorOut = err;
        }
        return std::nullopt;
    }
    QString name = displayName.trimmed();
    if (name.isEmpty()) {
        const QString uri = XrayConfigBuilder::extractVlessUriFromText(vlessUri);
        const QUrl url(uri.isEmpty() ? vlessUri.trimmed() : uri);
        name = QUrl::fromPercentEncoding(url.fragment(QUrl::FullyDecoded).toUtf8());
        if (name.isEmpty()) {
            name = url.host();
        }
    }
    return importXrayJsonText(json, name, errorOut);
}

std::optional<LocalProfileStore::Profile> LocalProfileStore::importXrayJsonFile(const QString& sourceFilePath,
    const QString& displayName,
    QString* errorOut)
{
    QFile in(sourceFilePath);
    if (!in.open(QIODevice::ReadOnly)) {
        if (errorOut) {
            *errorOut = Datagate::tr("Could not read the selected file.");
        }
        return std::nullopt;
    }
    QString name = displayName.trimmed();
    if (name.isEmpty()) {
        name = QFileInfo(sourceFilePath).completeBaseName();
    }
    return importXrayJsonText(QString::fromUtf8(in.readAll()), name, errorOut);
}

bool LocalProfileStore::rename(const QString& id, const QString& newName, QString* errorOut)
{
    const QString name = newName.trimmed();
    if (id.isEmpty() || name.isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Invalid profile name.");
        }
        return false;
    }
    QJsonArray arr = loadIndexArray();
    bool found = false;
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject o = arr.at(i).toObject();
        if (o.value(QStringLiteral("id")).toString() != id) {
            continue;
        }
        o.insert(QStringLiteral("name"), name);
        o.insert(QStringLiteral("updatedUtc"), now);
        arr.replace(i, o);
        Profile p = profileFromJson(o);
        if (!writeProfileMeta(p, errorOut)) {
            return false;
        }
        found = true;
        break;
    }
    if (!found) {
        if (errorOut) {
            *errorOut = Datagate::tr("Profile not found.");
        }
        return false;
    }
    return saveIndexArray(arr, errorOut);
}

bool LocalProfileStore::remove(const QString& id, QString* errorOut)
{
    if (id.isEmpty()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Invalid profile id.");
        }
        return false;
    }
    QJsonArray arr = loadIndexArray();
    QJsonArray next;
    bool found = false;
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("id")).toString() == id) {
            found = true;
            continue;
        }
        next.append(o);
    }
    if (!found) {
        if (errorOut) {
            *errorOut = Datagate::tr("Profile not found.");
        }
        return false;
    }
    if (!saveIndexArray(next, errorOut)) {
        return false;
    }
    QDir(profileDir(id)).removeRecursively();
    return true;
}

QString LocalProfileStore::xrayConfigPath(const QString& id)
{
    return QDir(profileDir(id)).filePath(QStringLiteral("config.json"));
}

QString LocalProfileStore::readXrayConfigText(const QString& id, QString* errorOut)
{
    const auto meta = find(id);
    if (!meta.has_value()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Profile not found.");
        }
        return {};
    }
    if (meta->type != Type::Xray) {
        if (errorOut) {
            *errorOut = Datagate::tr("Profile is not an Xray config.");
        }
        return {};
    }
    QFile f(xrayConfigPath(id));
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut) {
            *errorOut = Datagate::tr("Could not read config.json.");
        }
        return {};
    }
    return QString::fromUtf8(f.readAll());
}

QString LocalProfileStore::readOvpnText(const QString& id, QString* errorOut)
{
    const auto meta = find(id);
    if (!meta.has_value()) {
        if (errorOut) {
            *errorOut = Datagate::tr("Profile not found.");
        }
        return {};
    }
    if (meta->type != Type::OpenVpn) {
        if (errorOut) {
            *errorOut = Datagate::tr("Profile is not an OpenVPN config.");
        }
        return {};
    }
    QFile f(QDir(profileDir(id)).filePath(QStringLiteral("config.ovpn")));
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut) {
            *errorOut = Datagate::tr("Could not read config.ovpn.");
        }
        return {};
    }
    return QString::fromUtf8(f.readAll());
}
