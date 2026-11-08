#!/usr/bin/env bash
# Unit/smoke tests for DataGateLinux (no gtest). All temps under $HOME (root /tmp may be full).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export TMPDIR="${TMPDIR:-$HOME/tmp/datagate-tests}"
mkdir -p "$TMPDIR"
WORKDIR="$TMPDIR/run_$$"
mkdir -p "$WORKDIR"
cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

fail=0
pass=0
assert_ok() {
  local name="$1"
  shift
  if "$@"; then
    echo "PASS  $name"
    pass=$((pass + 1))
  else
    echo "FAIL  $name"
    fail=$((fail + 1))
  fi
}

STUB="$WORKDIR/datagate_tr_stub.cpp"
cat >"$STUB" <<'EOF'
#include <QString>
namespace Datagate { QString tr(const char* s){ return QString::fromUtf8(s);} }
EOF

LAST_SEL_STUB="$WORKDIR/last_selected_stub.cpp"
cat >"$LAST_SEL_STUB" <<'EOF'
namespace DatagateUtils {
void saveLastSelectedServerId(int) {}
int loadLastSelectedServerId() { return -1; }
}
EOF

CXXFLAGS=(-std=c++20 -fPIC $(pkg-config --cflags Qt6Core) -I"$ROOT/src")
LIBS=($(pkg-config --libs Qt6Core))

# ----- 1) XrayConfigBuilder -----
BUILDER_SRC="$WORKDIR/test_xray_builder.cpp"
cat >"$BUILDER_SRC" <<'EOF'
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "XrayConfigBuilder.h"
static bool hasTun(const QString& j){
  auto o=QJsonDocument::fromJson(j.toUtf8()).object();
  for(const auto& v: o.value("inbounds").toArray()){
    if(v.toObject().value("protocol").toString()=="tun") return true;
  }
  return false;
}
static int socksPort(const QString& j){
  auto o=QJsonDocument::fromJson(j.toUtf8()).object();
  for(const auto& v: o.value("inbounds").toArray()){
    auto in=v.toObject();
    if(in.value("protocol").toString()=="socks") return in.value("port").toInt();
  }
  return -1;
}
#define CHECK(cond) do{ if(!(cond)){ qWarning("CHECK fail %s:%d", __FILE__, __LINE__); return 1; } }while(0)
int main(int argc,char**argv){
  QCoreApplication a(argc,argv);
  QString err;
  auto j1=XrayConfigBuilder::clientJsonFromVlessUri(
    "vless://11111111-1111-1111-1111-111111111111@example.com?encryption=none&security=tls&sni=a",
    10808,true,&err);
  CHECK(!j1.isEmpty() && j1.contains("\"port\": 443"));
  auto j2=XrayConfigBuilder::clientConfigFromOpaqueText(
    "Use: {{vless_uri}}\nvless://11111111-1111-1111-1111-111111111111@h.example:8443?type=tcp#n\n",
    10808,true,&err);
  CHECK(!j2.isEmpty() && j2.contains("8443"));
  auto j3=XrayConfigBuilder::clientJsonFromVlessUri(
    "vless://11111111-1111-1111-1111-111111111111@h.example:443?type=xhttp&path=/x&host=h.example&security=tls&sni=h.example",
    10808,false,&err);
  CHECK(!j3.isEmpty() && j3.contains("xhttpSettings") && !hasTun(j3));
  auto j4=XrayConfigBuilder::clientJsonFromVlessUri(
    "vless://11111111-1111-1111-1111-111111111111@h.example:443?encryption=none",
    10808,true,&err);
  CHECK(!j4.isEmpty() && hasTun(j4) && socksPort(j4)==10808);
  auto j5=XrayConfigBuilder::normalizeImportedJson(
    R"({"outbounds":[{"tag":"proxy","protocol":"freedom"}],"inbounds":[{"protocol":"socks","port":1}]})",
    10809,true,&err);
  CHECK(!j5.isEmpty() && hasTun(j5) && socksPort(j5)==10809);
  auto j6=XrayConfigBuilder::normalizeImportedJson(
    R"({"protocol":"vless","settings":{"vnext":[{"address":"h","port":443,"users":[{"id":"11111111-1111-1111-1111-111111111111","encryption":"none"}]}]}})",
    10810,false,&err);
  CHECK(!j6.isEmpty() && !hasTun(j6) && socksPort(j6)==10810 && j6.contains("\"tag\": \"proxy\""));
  auto uri=XrayConfigBuilder::extractVlessUriFromText("prefix vless://u@h:1?e=none trailing");
  CHECK(uri.startsWith("vless://"));
  auto empty=XrayConfigBuilder::clientConfigFromOpaqueText("not a config", 1, false, &err);
  CHECK(empty.isEmpty() && !err.isEmpty());

  // Dashboard profile JSON: vless + dnsServers must survive; DNS /32 before private→direct.
  const QString profile = QStringLiteral(
    R"({"vless":"vless://11111111-1111-1111-1111-111111111111@h.example:443?encryption=none",)"
    R"("dnsServers":["172.20.0.1"],"dnsIdentityEnabled":true,"friendlyName":"n"})");
  CHECK(XrayConfigBuilder::extractDnsServersFromProfileText(profile) == QStringList{QStringLiteral("172.20.0.1")});
  CHECK(XrayConfigBuilder::extractDnsIdentityEnabled(profile));
  auto built = XrayConfigBuilder::buildClientConfigFromOpaqueText(profile, 10808, true, &err);
  CHECK(!built.json.isEmpty());
  CHECK(built.dnsServers == QStringList{QStringLiteral("172.20.0.1")});
  CHECK(built.dnsIdentityEnabled);
  CHECK(XrayConfigBuilder::dnsServersFromClientJson(built.json) == QStringList{QStringLiteral("172.20.0.1")});
  {
    const auto root = QJsonDocument::fromJson(built.json.toUtf8()).object();
    CHECK(!root.contains(QStringLiteral("dns"))); // no DoH/FakeDNS block
    CHECK(!root.contains(QStringLiteral("fakeDns")));
    bool sawDirect=false;
    for (const auto& v : root.value(QStringLiteral("outbounds")).toArray()) {
      if (v.toObject().value(QStringLiteral("tag")).toString() == QLatin1String("direct")) sawDirect=true;
    }
    CHECK(sawDirect);
    const auto rules = root.value(QStringLiteral("routing")).toObject().value(QStringLiteral("rules")).toArray();
    CHECK(rules.size() >= 3);
    const auto r0 = rules.at(0).toObject();
    CHECK(r0.value(QStringLiteral("outboundTag")).toString() == QLatin1String("proxy"));
    const auto ips0 = r0.value(QStringLiteral("ip")).toArray();
    CHECK(ips0.size()==1 && ips0.at(0).toString().startsWith(QStringLiteral("172.20.0.1")));
    const auto r1 = rules.at(1).toObject();
    CHECK(r1.value(QStringLiteral("outboundTag")).toString() == QLatin1String("direct"));
    CHECK(r1.value(QStringLiteral("ip")).toArray().contains(QStringLiteral("172.16.0.0/12")));
    const auto rLast = rules.at(rules.size()-1).toObject();
    CHECK(rLast.value(QStringLiteral("outboundTag")).toString() == QLatin1String("proxy"));
    CHECK(rLast.value(QStringLiteral("network")).toString().contains(QStringLiteral("udp")));
  }
  // Plain vless:// without dnsServers — still parses; no Pi-hole guess.
  auto plain = XrayConfigBuilder::buildClientConfigFromOpaqueText(
    "vless://11111111-1111-1111-1111-111111111111@h.example:443?encryption=none", 10808, false, &err);
  CHECK(!plain.json.isEmpty() && plain.dnsServers.isEmpty() && !plain.dnsIdentityEnabled);
  return 0;
}
EOF
g++ "${CXXFLAGS[@]}" "$BUILDER_SRC" "$ROOT/src/XrayConfigBuilder.cpp" "$STUB" -o "$WORKDIR/test_xray_builder" "${LIBS[@]}"
assert_ok "xray_config_builder" "$WORKDIR/test_xray_builder"

# ----- 2) VpnServerStatusJson (Xray in manual list, not in WSS-only) -----
STATUS_SRC="$WORKDIR/test_vpn_status.cpp"
cat >"$STATUS_SRC" <<'EOF'
#include <QCoreApplication>
#include <QDebug>
#include "DatagateUtils.h"
#define CHECK(cond) do{ if(!(cond)){ qWarning("CHECK fail line %d", __LINE__); return 1; } }while(0)
static const char* kJson = R"JSON(
{"success":true,"data":{"openVpnServerWithStatuses":[
  {"countConnectedClients":1,"openVpnServerResponses":{"openVpnServer":{
    "id":69,"serverName":"Helsinki WSS","serverType":0,"isOnline":true,"isEnableWss":true,
    "apiUrl":"https://ovpn.example","isAccessibleForUserQuotaPlan":true}}},
  {"countConnectedClients":0,"openVpnServerResponses":{"openVpnServer":{
    "id":1,"serverName":"Cyprus noWSS","serverType":0,"isOnline":true,"isEnableWss":false,
    "apiUrl":"https://cy.example","isAccessibleForUserQuotaPlan":true}}},
  {"countConnectedClients":2,"openVpnServerResponses":{"openVpnServer":{
    "id":76,"serverName":"Norway xray","serverType":1,"isOnline":true,"isEnableWss":false,
    "apiUrl":"https://xs.example","isAccessibleForUserQuotaPlan":true}}},
  {"countConnectedClients":9,"openVpnServerResponses":{"openVpnServer":{
    "id":99,"serverName":"Xray string type","ServerType":"Xray","isOnline":true,"isEnableWss":false,
    "apiUrl":"https://xs2.example","isAccessibleForUserQuotaPlan":true}}}
]}}
)JSON";
int main(int argc,char**argv){
  QCoreApplication a(argc,argv);
  const QByteArray body(kJson);
  const auto wss = DatagateUtils::listWssServersFromStatusJson(body);
  CHECK(wss.size()==1);
  CHECK(wss[0].first==69);
  const auto manual = DatagateUtils::listManualConnectServersFromStatusJson(body);
  CHECK(manual.size()==3); // 69 + 76 + 99 (not Cyprus)
  bool saw76=false, saw99=false, saw69=false;
  for(const auto& s: manual){
    if(s.serverId==76){ CHECK(s.isXray()); saw76=true; }
    if(s.serverId==99){ CHECK(s.isXray()); saw99=true; }
    if(s.serverId==69){ CHECK(!s.isXray()); saw69=true; }
    if(s.serverId==1) return 2;
  }
  CHECK(saw76 && saw99 && saw69);
  const auto picked = DatagateUtils::pickServerByIdFromStatusJson(body, 76);
  CHECK(picked.has_value() && picked->isXray() && picked->serverId==76);
  const auto missing = DatagateUtils::pickServerByIdFromStatusJson(body, 1);
  CHECK(!missing.has_value()); // Cyprus not connectable via Home manual
  const auto best = DatagateUtils::pickBestServerWinStyle(body);
  CHECK(best.has_value() && best->serverId==69 && !best->isXray());
  return 0;
}
EOF
g++ "${CXXFLAGS[@]}" "$STATUS_SRC" "$ROOT/src/VpnServerStatusJson.cpp" "$LAST_SEL_STUB" \
  -o "$WORKDIR/test_vpn_status" "${LIBS[@]}"
assert_ok "vpn_server_status_json" "$WORKDIR/test_vpn_status"

# ----- 3) LocalProfileStore upsert/type -----
PROFILE_SRC="$WORKDIR/test_profiles.cpp"
PROFILE_STUBS="$WORKDIR/profile_link_stubs.cpp"
cat >"$PROFILE_STUBS" <<'EOF'
#include "AppConfig.h"
#include "DatagateUtils.h"
#include <QLoggingCategory>
Q_LOGGING_CATEGORY(lcUi, "datagate.ui")
QString AppConfig::s_apiBaseUrl;
QString AppConfig::s_googleClientId;
int AppConfig::s_googleRedirectPort = 0;
bool AppConfig::s_openVpnIgnoreRedirectGateway = false;
bool AppConfig::s_udpBridgeFramed = true;
bool AppConfig::s_openVpnUseSystemBinary = false;
QString AppConfig::s_updateGithubOwner;
QString AppConfig::s_updateGithubRepo;
bool AppConfig::s_updateCheckOnStartup = false;
QString AppConfig::s_xrayExecutablePath;
quint16 AppConfig::s_xraySocksPort = 10808;
bool AppConfig::s_xrayUseTun = true;
QString AppConfig::xrayExecutablePath() { return s_xrayExecutablePath; }
quint16 AppConfig::xraySocksPort() { return s_xraySocksPort; }
bool AppConfig::xrayUseTun() { return s_xrayUseTun; }
namespace DatagateUtils {
ParsedRemote parseFirstRemoteFromOvpn(const QString&) { return {}; }
}
EOF
cat >"$PROFILE_SRC" <<'EOF'
#include <QCoreApplication>
#include <QDebug>
#include "AppConfig.h"
#include "LocalProfileStore.h"
#include "XrayConfigBuilder.h"
#define CHECK(cond) do{ if(!(cond)){ qWarning("CHECK fail line %d", __LINE__); return 1; } }while(0)
int main(int argc,char**argv){
  QCoreApplication::setOrganizationName("DataGateTest");
  QCoreApplication::setApplicationName("DataGateLinuxTest");
  QCoreApplication a(argc,argv);
  const QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
  CHECK(!dataHome.isEmpty());
  QString err;
  const QString uri="vless://11111111-1111-1111-1111-111111111111@h.example:443?encryption=none&security=none";
  auto p1=LocalProfileStore::importXrayUri(uri, "Norway xray (Xray)", &err);
  CHECK(p1.has_value());
  CHECK(p1->type==LocalProfileStore::Type::Xray);
  QString buildErr;
  const QString jsonBuilt=XrayConfigBuilder::clientJsonFromVlessUri(uri, AppConfig::xraySocksPort(), AppConfig::xrayUseTun(), &buildErr);
  auto p2=LocalProfileStore::upsertXrayJsonText(jsonBuilt, "Norway xray (Xray)", &err);
  CHECK(p2.has_value());
  CHECK(p2->id==p1->id);
  const auto list=LocalProfileStore::list();
  int xrayCount=0;
  for(const auto& p: list) if(p.type==LocalProfileStore::Type::Xray) ++xrayCount;
  CHECK(xrayCount==1);
  const QString json=LocalProfileStore::readXrayConfigText(p1->id, &err);
  CHECK(!json.isEmpty() && json.contains("socks"));
  CHECK(LocalProfileStore::remove(p1->id, &err));
  CHECK(LocalProfileStore::list().isEmpty());
  return 0;
}
EOF
XDG_DATA_HOME="$WORKDIR/xdg-data"
mkdir -p "$XDG_DATA_HOME"
# AppConfig.h may declare more statics — include only what we need via stub.
# Prefer compiling against AppConfig.h without AppConfig.cpp:
g++ "${CXXFLAGS[@]}" "$PROFILE_SRC" \
  "$ROOT/src/LocalProfileStore.cpp" \
  "$ROOT/src/XrayConfigBuilder.cpp" \
  "$PROFILE_STUBS" \
  "$STUB" \
  -o "$WORKDIR/test_profiles" "${LIBS[@]}"
assert_ok "local_profile_store" env XDG_DATA_HOME="$XDG_DATA_HOME" "$WORKDIR/test_profiles"

# ----- 4) ClientIdentity: headers only for the backend host, rejection detection -----
CI_SRC="$WORKDIR/test_client_identity.cpp"
cat >"$CI_SRC" <<'EOF'
#include "ClientIdentity.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>
#include <cstdio>

// One-shot HTTP server: records the request head, answers with a fixed status/body.
struct FakeHttp {
    QTcpServer server;
    QByteArray lastHead;
    int status = 200;
    QByteArray body = "{}";
    FakeHttp() {
        server.listen(QHostAddress::LocalHost, 0);
        QObject::connect(&server, &QTcpServer::newConnection, [this]() {
            QTcpSocket* s = server.nextPendingConnection();
            QObject::connect(s, &QTcpSocket::readyRead, [this, s]() {
                lastHead += s->readAll();
                if (!lastHead.contains("\r\n\r\n")) return;
                const QByteArray reason = status == 200 ? "OK" : "Bad Request";
                s->write("HTTP/1.1 " + QByteArray::number(status) + " " + reason + "\r\nContent-Type: application/json\r\n"
                         "Connection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                s->disconnectFromHost();
            });
        });
    }
    QUrl url(const QString& path) const {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(server.serverPort()).arg(path));
    }
};

static QByteArray get(QNetworkAccessManager& nam, const QUrl& url) {
    QNetworkReply* r = nam.get(QNetworkRequest(url));
    QEventLoop loop;
    QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    const QByteArray b = r->readAll();
    r->deleteLater();
    return b;
}

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "check failed: %s (line %d)\n", #c, __LINE__); ++fails; } } while (0)

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(QStringLiteral("1.1.0"));
    FakeHttp backend, other;
    ClientIdentity::setBackendBaseUrl(backend.url(QStringLiteral("/")));
    DatagateNetworkAccessManager nam;

    get(nam, backend.url(QStringLiteral("/api/v3/open-vpn-servers/get-all-with-status")));
    CHECK(backend.lastHead.contains("X-Client-Name: linux\r\n"));
    CHECK(backend.lastHead.contains("X-Client-Version: 1.1.0\r\n"));

    get(nam, other.url(QStringLiteral("/repos/x/y/releases/latest")));
    CHECK(!other.lastHead.isEmpty());
    CHECK(!other.lastHead.contains("X-Client-Name"));
    CHECK(!other.lastHead.contains("X-Client-Version"));

    QStringList rejected;
    QObject::connect(ClientIdentity::Events::instance(), &ClientIdentity::Events::rejected,
        [&](const QString& code) { rejected << code; });
    backend.lastHead.clear();
    backend.status = 400;
    backend.body = R"({"success":false,"data":{"code":"client_version_invalid","nameHeader":"X-Client-Name"}})";
    const QByteArray seen = get(nam, backend.url(QStringLiteral("/api/auth/refresh")));
    CHECK(seen.contains("client_version_invalid")); // caller still reads the body
    CHECK(rejected == QStringList{QStringLiteral("client_version_invalid")});

    CHECK(ClientIdentity::rejectionCode(R"({"data":{"code":"invalid_token"}})").isEmpty());
    CHECK(ClientIdentity::rejectionCode("not json").isEmpty());
    CHECK(ClientIdentity::rejectionCode(R"({"Data":{"Code":"client_name_unknown"}})") == QStringLiteral("client_name_unknown"));

    const QUrl hub = ClientIdentity::appendToHubUrl(QUrl(QStringLiteral("https://api.example.test/api/hubs/frontend?access_token=x")), QStringLiteral("1.1.0 (7)"));
    const QUrlQuery hq(hub);
    CHECK(hub.toString(QUrl::FullyEncoded).startsWith(QStringLiteral("https://api.example.test/api/hubs/frontend?access_token=x&clientName=linux&clientVersion=")));
    CHECK(hq.queryItemValue(QStringLiteral("clientVersion"), QUrl::FullyDecoded) == QStringLiteral("1.1.0 (7)"));

    CHECK(ClientIdentity::sanitizeVersion(QStringLiteral(" 1.1.0\n")) == QStringLiteral("1.1.0"));
    CHECK(ClientIdentity::sanitizeVersion(QStringLiteral("1.0-é")) == QStringLiteral("1.0-"));
    CHECK(ClientIdentity::sanitizeVersion(QString(100, QLatin1Char('9'))).size() == ClientIdentity::kMaxVersionLength);
    CHECK(ClientIdentity::sanitizeVersion(QString()) == QStringLiteral("0"));
    CHECK(!ClientIdentity::isBackendUrl(QUrl(QStringLiteral("https://node.example.test/api/proxy")), QUrl(QStringLiteral("https://api.example.test"))));
    CHECK(ClientIdentity::isBackendUrl(QUrl(QStringLiteral("https://API.example.test:443/api/x")), QUrl(QStringLiteral("https://api.example.test/"))));
    return fails == 0 ? 0 : 1;
}
EOF
MOC=""
for c in /usr/lib/qt6/libexec/moc /usr/lib/x86_64-linux-gnu/qt6/libexec/moc "$(command -v moc-qt6 || true)"; do
  if [[ -n "$c" && -x "$c" ]]; then MOC="$c"; break; fi
done
if [[ -n "$MOC" ]]; then
  "$MOC" "$ROOT/src/ClientIdentity.h" -o "$WORKDIR/moc_ClientIdentity.cpp"
  g++ -std=c++20 -fPIC $(pkg-config --cflags Qt6Core Qt6Network) -I"$ROOT/src" \
    "$CI_SRC" "$ROOT/src/ClientIdentity.cpp" "$ROOT/src/AppLogging.cpp" "$WORKDIR/moc_ClientIdentity.cpp" \
    -o "$WORKDIR/test_client_identity" $(pkg-config --libs Qt6Core Qt6Network)
  assert_ok "client_identity" "$WORKDIR/test_client_identity"
else
  echo "SKIP  client_identity (moc not found)"
fi

# ----- 5) Real IPv4 routing in a private network namespace -----
assert_ok "xray_routes" bash "$ROOT/tools/test_xray_routes.sh"

# ----- 6) DataGateXray -test (optional) -----
XRAY="$ROOT/cmake-build-debug/DataGateXray"
if [[ -x "$XRAY" ]]; then
  CFG="$WORKDIR/xray-test.json"
  python3 - <<PY
import json
cfg={
  "log":{"loglevel":"warning"},
  "inbounds":[{"tag":"socks-in","listen":"127.0.0.1","port":18081,"protocol":"socks","settings":{"udp":True}}],
  "outbounds":[{"tag":"proxy","protocol":"freedom"}],
  "routing":{"domainStrategy":"AsIs","rules":[{"type":"field","outboundTag":"proxy","network":"tcp,udp"}]}
}
open("$CFG","w").write(json.dumps(cfg))
PY
  assert_ok "datagatexray_run_test" "$XRAY" run -test -c "$CFG"
else
  echo "SKIP  datagatexray_run_test (binary missing)"
fi

echo
echo "Results: $pass passed, $fail failed"
[[ "$fail" -eq 0 ]]
