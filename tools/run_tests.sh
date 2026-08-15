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

# ----- 4) DataGateXray -test (optional) -----
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
