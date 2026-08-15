#pragma once

#include <QString>
#include <QVector>
#include <optional>

/// Local VPN profiles under AppLocalDataLocation/profiles/ (imported .ovpn / Xray).
class LocalProfileStore {
public:
    enum class Type {
        OpenVpn,
        Xray,
    };

    struct Profile {
        QString id;
        QString name;
        Type type = Type::OpenVpn;
        QString createdUtc;
        QString updatedUtc;
    };

    static QString typeToString(Type t);
    static Type typeFromString(const QString& s);

    static QString profilesRoot();

    static QVector<Profile> list();
    static std::optional<Profile> find(const QString& id);

    /// Copy .ovpn into store. displayName empty → use file basename.
    static std::optional<Profile> importOvpn(const QString& sourceFilePath, const QString& displayName, QString* errorOut);

    /// Import vless:// link or Xray JSON text/file as type=xray (writes config.json).
    static std::optional<Profile> importXrayUri(const QString& vlessUri, const QString& displayName, QString* errorOut);
    static std::optional<Profile> importXrayJsonFile(const QString& sourceFilePath, const QString& displayName,
        QString* errorOut);
    static std::optional<Profile> importXrayJsonText(const QString& jsonText, const QString& displayName,
        QString* errorOut);
    /// Import or overwrite an existing Xray profile with the same display name.
    static std::optional<Profile> upsertXrayJsonText(const QString& jsonText, const QString& displayName,
        QString* errorOut);

    static bool rename(const QString& id, const QString& newName, QString* errorOut);
    static bool remove(const QString& id, QString* errorOut);

    static QString readOvpnText(const QString& id, QString* errorOut);
    static QString readXrayConfigText(const QString& id, QString* errorOut);
    static QString xrayConfigPath(const QString& id);
    static QString profileDir(const QString& id);
};
