#include "DatagateOvpn3Client.h"
#include "LinuxCapNetAdmin.h"

#include <utility>

#include <openvpn/io/io.hpp>
#include <client/ovpncli.hpp>

#if defined(__linux__)
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#endif

namespace Datagate {

namespace {

using namespace openvpn::ClientAPI;

#if defined(__linux__)

bool parseDefaultRouteV4(std::string& gatewayOut, std::string& deviceOut)
{
    FILE* f = popen("ip -4 route show default 2>/dev/null", "r");
    if (!f) {
        return false;
    }
    char buf[512];
    std::string line;
    if (fgets(buf, sizeof(buf), f)) {
        line = buf;
    }
    pclose(f);
    const auto viaPos = line.find(" via ");
    const auto devPos = line.find(" dev ");
    if (viaPos == std::string::npos || devPos == std::string::npos || devPos < viaPos) {
        return false;
    }
    gatewayOut = line.substr(viaPos + 5, devPos - (viaPos + 5));
    while (!gatewayOut.empty() && gatewayOut.back() == ' ') {
        gatewayOut.pop_back();
    }
    std::string rest = line.substr(devPos + 5);
    const auto sp = rest.find(' ');
    deviceOut = (sp == std::string::npos) ? rest : rest.substr(0, sp);
    while (!deviceOut.empty() && (deviceOut.back() == '\n' || deviceOut.back() == '\r')) {
        deviceOut.pop_back();
    }
    return !gatewayOut.empty() && !deviceOut.empty();
}

bool runIp(const std::vector<std::string>& args)
{
    std::vector<std::string> storage = args;
    std::vector<char*> argv;
    argv.reserve(storage.size() + 2);
    static char ipBin[] = "ip";
    argv.push_back(ipBin);
    for (std::string& s : storage) {
        argv.push_back(s.data());
    }
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        execvp("ip", argv.data());
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

#endif // __linux__

class Ovpn3ClientImpl final : public OpenVPNClient {
public:
    DatagateOvpn3Client::EventFn onEvent;
    DatagateOvpn3Client::LogFn onLog;

#if defined(__linux__)
    std::string m_remoteIp;
    std::string m_physGw;
    std::string m_physDev;
    bool m_hostRouteOn = false;
#endif

    Ovpn3ClientImpl() = default;

    bool pause_on_connection_timeout() override { return false; }

#if defined(__linux__)
    void clearHostRoute()
    {
        if (!m_hostRouteOn && m_remoteIp.empty()) {
            return;
        }
        (void)datagate_linux_try_raise_effective_cap_net_admin();
        if (!m_remoteIp.empty() && !m_physGw.empty() && !m_physDev.empty()) {
            runIp({"route", "del", m_remoteIp + "/32", "via", m_physGw, "dev", m_physDev});
        }
        if (m_hostRouteOn && onLog) {
            onLog("host-route: cleared");
        }
        m_hostRouteOn = false;
    }

    bool installHostRoute(const std::string& remote)
    {
        (void)datagate_linux_try_raise_effective_cap_net_admin();
        if (m_physGw.empty() || m_physDev.empty()) {
            parseDefaultRouteV4(m_physGw, m_physDev);
        }
        if (remote.empty() || m_physGw.empty() || m_physDev.empty()) {
            return false;
        }
        m_remoteIp = remote;
        const bool ok = runIp(
            {"route", "replace", m_remoteIp + "/32", "via", m_physGw, "dev", m_physDev});
        m_hostRouteOn = ok;
        return ok;
    }
#endif

    void event(const Event& ev) override
    {
#if defined(__linux__)
        if (ev.name == "DISCONNECTED") {
            clearHostRoute();
        }
#endif
        if (onEvent) {
            onEvent(ev.name, ev.info);
        }
    }

    void acc_event(const AppCustomControlMessageEvent& ev) override
    {
        (void)ev;
    }

    void log(const LogInfo& li) override
    {
        if (onLog) {
            onLog(li.text);
        }
    }

    void external_pki_cert_request(ExternalPKICertRequest& r) override
    {
        r.error = true;
        r.errorText = "External PKI not enabled";
    }

    void external_pki_sign_request(ExternalPKISignRequest& r) override
    {
        r.error = true;
        r.errorText = "External PKI not enabled";
    }

    bool socket_protect(openvpn_io::detail::socket_type socket, std::string remote, bool ipv6) override
    {
#if defined(__linux__)
        // Do NOT use SO_BINDTODEVICE here — with OpenVPN 3 + TCP it correlates with
        // NETWORK_EOF ~1s after CONNECTED even when redirect-gateway is ignored.
        // openvpn3-linux default protect method is host-route only.
        (void)socket;
        (void)ipv6;
        (void)datagate_linux_try_raise_effective_cap_net_admin();
        parseDefaultRouteV4(m_physGw, m_physDev);
        const bool routed = installHostRoute(remote);
        if (onLog) {
            onLog(std::string("socket_protect: remote=") + remote + " host-route="
                + (routed ? "ok" : "FAIL") + " via=" + m_physGw + " dev=" + m_physDev);
        }
        return true; // never abort connect on protect failure
#else
        (void)socket;
        (void)remote;
        (void)ipv6;
        return true;
#endif
    }
};

} // namespace

class DatagateOvpn3Client::Impl {
public:
    Ovpn3ClientImpl client;
};

DatagateOvpn3Client::DatagateOvpn3Client()
    : m_impl(std::make_unique<Impl>())
{
}

DatagateOvpn3Client::~DatagateOvpn3Client() = default;

void DatagateOvpn3Client::setCallbacks(EventFn onEvent, LogFn onLog)
{
    m_impl->client.onEvent = std::move(onEvent);
    m_impl->client.onLog = std::move(onLog);
}

std::string DatagateOvpn3Client::connectBlocking(const std::string& profileUtf8, const std::string& guiVersion)
{
#if defined(__linux__)
    datagate_linux_try_raise_effective_cap_net_admin();
#endif
    Ovpn3ClientImpl& c = m_impl->client;

    Config cfg;
    cfg.content = profileUtf8;
    cfg.guiVersion = guiVersion;

    const EvalConfig ec = c.eval_config(cfg);
    if (ec.error) {
        return ec.message;
    }

    ProvideCreds creds;
    const Status credStatus = c.provide_creds(creds);
    if (credStatus.error) {
        return credStatus.message;
    }

    const Status st = c.connect();
#if defined(__linux__)
    c.clearHostRoute();
#endif
    if (st.error) {
        return st.message;
    }
    return {};
}

void DatagateOvpn3Client::requestStop()
{
    m_impl->client.stop();
}

} // namespace Datagate
