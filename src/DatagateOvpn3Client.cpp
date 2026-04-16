#include "DatagateOvpn3Client.h"
#include "DatagateUtils.h"

#include <utility>

#include <openvpn/io/io.hpp>
#include <client/ovpncli.hpp>

namespace Datagate {

namespace {

using namespace openvpn::ClientAPI;

class Ovpn3ClientImpl final : public OpenVPNClient {
public:
    DatagateOvpn3Client::EventFn onEvent;
    DatagateOvpn3Client::LogFn onLog;

    Ovpn3ClientImpl() = default;

    bool pause_on_connection_timeout() override { return false; }

    void event(const Event& ev) override
    {
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

    bool socket_protect(openvpn_io::detail::socket_type /*socket*/, std::string /*remote*/, bool /*ipv6*/) override
    {
        return true;
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
    DatagateUtils::linuxTryRaiseEffectiveCapNetAdmin();
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
