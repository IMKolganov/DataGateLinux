// Minimal child process: OpenVPN 3 core + TUN (CAP_NET_ADMIN on this binary, not the GUI).
// Protocol on stdout (one line per message):
//   DG3E <hex(name)> <hex(info)>\n   — core event (name/info UTF-8)
//   DG3L <hex(line)>\n               — log line (UTF-8)
// Errors from connectBlocking go to stderr; exit 0 on normal disconnect, 1 on connect error, 2 on usage/file.

#include "DatagateOvpn3Client.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string toHex(std::string_view in)
{
    static const char* d = "0123456789abcdef";
    std::string out;
    out.reserve(in.size() * 2);
    for (const unsigned char c : in) {
        out += d[c >> 4];
        out += d[c & 15];
    }
    return out;
}

void emitE(const std::string& n, const std::string& i)
{
    std::cout << "DG3E " << toHex(n) << ' ' << toHex(i) << '\n' << std::flush;
}

void emitL(const std::string& line)
{
    std::cout << "DG3L " << toHex(line) << '\n' << std::flush;
}

std::string readFileUtf8(const char* path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: DataGateOvpn3Helper <profile.ovpn> [guiVersion]\n";
        return 2;
    }
    const std::string profile = readFileUtf8(argv[1]);
    if (profile.empty()) {
        std::cerr << "DataGateOvpn3Helper: empty or unreadable profile: " << argv[1] << '\n';
        return 2;
    }
    const std::string gui = (argc >= 3 && argv[2][0] != '\0') ? std::string(argv[2]) : std::string("datagate_linux 0");

    Datagate::DatagateOvpn3Client client;
    client.setCallbacks(
        [](const std::string& n, const std::string& i) { emitE(n, i); },
        [](const std::string& line) { emitL(line); });
    const std::string err = client.connectBlocking(profile, gui);
    if (!err.empty()) {
        std::cerr << err << '\n';
        return 1;
    }
    return 0;
}
