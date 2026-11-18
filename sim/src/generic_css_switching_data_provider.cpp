#include <generic_css_switching_data_provider.hpp>
#include <sim_data_provider_factory.hpp>
#include <ItcLogger/Logger.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

namespace Nos3
{
    REGISTER_DATA_PROVIDER(Generic_cssSwitchingDataProvider, "GENERIC_CSS_SWITCHING_PROVIDER");
    extern ItcLogger::Logger* sim_logger;

    Generic_cssSwitchingDataProvider::Generic_cssSwitchingDataProvider(
        const boost::property_tree::ptree& config)
        : SimIDataProvider(config),
          _mode(SIM),
          _control_port(config.get<unsigned short>(
              "simulator.hardware-model.data-provider.control-port", 5001))
    {
        std::string initial = config.get<std::string>(
            "simulator.hardware-model.data-provider.mode", "SIM");
        if (initial == "HIL" || initial == "hil") _mode.store(HIL);
        else if (initial != "SIM" && initial != "sim")
            throw std::runtime_error("Generic CSS switch: mode must be SIM or HIL");

        // These constructors read the existing XML settings for their respective
        // ports, hostname, spacecraft ID and stale timeout.
        _sim_provider.reset(SimDataProviderFactory::Instance().Create(
            "GENERIC_CSS_42_PROVIDER", config));
        if (!_sim_provider) throw std::runtime_error("Cannot create CSS 42 provider");
        _hil_provider.reset(SimDataProviderFactory::Instance().Create(
            "GENERIC_CSS_HIL_PROVIDER", config));
        if (!_hil_provider) throw std::runtime_error("Cannot create CSS HIL provider");

        _control_thread = std::thread(&Generic_cssSwitchingDataProvider::control_loop, this);
        sim_logger->info("CSS switching provider started in %s mode",
                         _mode.load() == HIL ? "HIL" : "SIM");
    }

    Generic_cssSwitchingDataProvider::~Generic_cssSwitchingDataProvider()
    {
        _stop.store(true);
        if (_control_thread.joinable()) _control_thread.join();
    }

    boost::shared_ptr<SimIDataPoint>
    Generic_cssSwitchingDataProvider::get_data_point(void) const
    {
        if (_mode.load() == HIL) return _hil_provider->get_data_point();
        return _sim_provider->get_data_point();
    }

    void Generic_cssSwitchingDataProvider::control_loop()
    {
        const int server = ::socket(AF_INET, SOCK_STREAM, 0);
        if (server < 0) {
            sim_logger->error("CSS switch: socket failed: %s", std::strerror(errno));
            return;
        }
        int reuse = 1;
        ::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(_control_port);
        if (::bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
            ::listen(server, 4) < 0) {
            sim_logger->error("CSS switch: bind/listen on %u failed: %s",
                              _control_port, std::strerror(errno));
            ::close(server);
            return;
        }
        sim_logger->info("CSS switch: listening on TCP %u", _control_port);
        while (!_stop.load()) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(server, &fds);
            timeval timeout{};
            timeout.tv_usec = 200000;
            const int ready = ::select(server + 1, &fds, nullptr, nullptr, &timeout);
            if (ready < 0) {
                if (errno == EINTR) continue;
                sim_logger->error("CSS switch: select failed: %s", std::strerror(errno));
                break;
            }
            if (ready == 0) continue;
            const int client = ::accept(server, nullptr, nullptr);
            if (client < 0) continue;
            // Prevent a stalled client from blocking thread shutdown forever.
            timeval client_timeout{};
            client_timeout.tv_sec = 1;
            ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                         &client_timeout, sizeof(client_timeout));
            std::string cmd;
            char ch;
            while (cmd.size() < 32 && ::recv(client, &ch, 1, 0) == 1) {
                if (ch == '\n') break;
                if (ch != '\r') cmd += ch;
            }
            std::string reply;
            if (cmd == "SIM" || cmd == "sim") {
                _mode.store(SIM);
                reply = "OK SIM\n";
                sim_logger->info("CSS switch: switched to SIM");
            } else if (cmd == "HIL" || cmd == "hil") {
                _mode.store(HIL);
                reply = "OK HIL\n";
                sim_logger->info("CSS switch: switched to HIL");
            } else if (cmd == "STATUS" || cmd == "status") {
                reply = _mode.load() == HIL ? "OK HIL\n" : "OK SIM\n";
            } else {
                reply = "ERR expected SIM, HIL, or STATUS\n";
            }
            ::send(client, reply.c_str(), reply.size(), MSG_NOSIGNAL);
            ::close(client);
        }
        ::close(server);
    }
}