#pragma once

#include <functional>
#include <memory>
#include <string>

namespace Datagate {

/// OpenVPN 3 core client (same stack as DataGateAndroid).
/// Callbacks may run on the worker thread that calls connectBlocking — use Qt::QueuedConnection to touch UI.
class DatagateOvpn3Client {
public:
    using EventFn = std::function<void(const std::string& name, const std::string& info)>;
    using LogFn = std::function<void(const std::string& line)>;

    DatagateOvpn3Client();
    ~DatagateOvpn3Client();

    void setCallbacks(EventFn onEvent, LogFn onLog);

    /// eval_config + empty creds + connect() — blocks until session ends. Call from a worker thread only.
    /// Returns empty on normal disconnect; otherwise an error message.
    std::string connectBlocking(const std::string& profileUtf8, const std::string& guiVersion);

    void requestStop();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Datagate
