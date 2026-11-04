#include <generic_css_hil_data_provider.hpp>
#include <ItcLogger/Logger.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

namespace Nos3
{
    REGISTER_DATA_PROVIDER(Generic_cssHilDataProvider, "GENERIC_CSS_HIL_PROVIDER");
    extern ItcLogger::Logger *sim_logger;

    Generic_cssHilDataProvider::Generic_cssHilDataProvider(
        const boost::property_tree::ptree& config)
        : SimIDataProvider(config),
          _port(config.get<unsigned short>("simulator.hardware-model.data-provider.tcp-port", 5000)),
          _stale_timeout(config.get<unsigned int>(
              "simulator.hardware-model.data-provider.stale-timeout-ms", 3000))
    {
        sim_logger->info("Generic_cssHilDataProvider: Initialized; TCP port %u", _port);
        _listener = std::thread(&Generic_cssHilDataProvider::listen_for_samples, this);
    }

    Generic_cssHilDataProvider::~Generic_cssHilDataProvider()
    {
        _stop = true;
        if (_listener.joinable()) _listener.join();
    }

    bool Generic_cssHilDataProvider::parse_sample(
        const std::string& line, std::array<double, 6>& result) const
    {
        std::istringstream input(line);
        std::string token;
        for (size_t i = 0; i < result.size(); ++i)
        {
            if (!std::getline(input, token, ',')) return false;
            char* end = nullptr;
            errno = 0;
            const double reading = std::strtod(token.c_str(), &end);
            while (end && (*end == ' ' || *end == '\t')) ++end;
            if (end == token.c_str() || !end || *end != '\0' || errno == ERANGE ||
                !std::isfinite(reading) || reading < 0.0 || reading > 65.535)
                return false;
            result[i] = reading;
        }
        // Reject extra fields (and any trailing comma).
        if (std::getline(input, token, ',')) return false;
        return true;
    }

    void Generic_cssHilDataProvider::listen_for_samples()
    {
        const int server = ::socket(AF_INET, SOCK_STREAM, 0);
        if (server < 0)
        {
            sim_logger->error("HIL TCP: socket failed: %s", std::strerror(errno));
            return;
        }
        int reuse = 1;
        ::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(_port);
        if (::bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
            ::listen(server, 1) < 0)
        {
            sim_logger->error("HIL TCP: bind/listen failed on port %u: %s",
                              _port, std::strerror(errno));
            ::close(server);
            return;
        }
        sim_logger->info("HIL TCP: listening on port %u", _port);

        int client = -1;
        std::string pending;
        while (!_stop)
        {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(server, &read_fds);
            int highest = server;
            if (client >= 0)
            {
                FD_SET(client, &read_fds);
                if (client > highest) highest = client;
            }
            timeval timeout{};
            timeout.tv_usec = 200000; // Wake periodically so the destructor can join.
            const int ready = ::select(highest + 1, &read_fds, nullptr, nullptr, &timeout);
            if (ready < 0)
            {
                if (errno == EINTR) continue;
                sim_logger->error("HIL TCP: select failed: %s", std::strerror(errno));
                break;
            }
            if (ready == 0) continue;

            if (FD_ISSET(server, &read_fds))
            {
                int incoming = ::accept(server, nullptr, nullptr);
                if (incoming >= 0)
                {
                    if (client >= 0) ::close(client);
                    client = incoming;
                    pending.clear();
                    sim_logger->info("HIL TCP: client connected");
                }
            }
            if (client >= 0 && FD_ISSET(client, &read_fds))
            {
                char buffer[256];
                const ssize_t count = ::recv(client, buffer, sizeof(buffer), 0);
                if (count <= 0)
                {
                    ::close(client);
                    client = -1;
                    pending.clear();
                    sim_logger->info("HIL TCP: client disconnected");
                    continue;
                }
                pending.append(buffer, static_cast<size_t>(count));
                if (pending.size() > 4096)
                {
                    // Bound memory used by a malformed or non-newline-terminated stream.
                    pending.clear();
                    sim_logger->error("HIL TCP: oversize input discarded");
                    continue;
                }
                std::string::size_type newline;
                while ((newline = pending.find('\n')) != std::string::npos)
                {
                    const std::string line = pending.substr(0, newline);
                    pending.erase(0, newline + 1);
                    std::array<double, 6> sample{};
                    if (!parse_sample(line, sample))
                    {
                        sim_logger->error("HIL TCP: rejected invalid sample");
                        continue;
                    }
                    {
                        std::lock_guard<std::mutex> lock(_sample_mutex);
                        _latest = sample;
                        _last_received = std::chrono::steady_clock::now();
                        _has_sample = true;
                    }
                    sim_logger->info("HIL TCP: received sample, CSS0=%.3f V", sample[0]);
                }
            }
        }
        if (client >= 0) ::close(client);
        ::close(server);
    }

    boost::shared_ptr<SimIDataPoint> Generic_cssHilDataProvider::get_data_point(void) const
    {
        std::array<double, 6> sample{{0, 0, 0, 0, 0, 0}};
        bool valid = false;
        {
            std::lock_guard<std::mutex> lock(_sample_mutex);
            valid = _has_sample &&
                (std::chrono::steady_clock::now() - _last_received <= _stale_timeout);
            if (valid) sample = _latest;
        }
        const int flag = valid ? 1 : 0;
        // Invalid/stale samples become zeros through Generic_cssDataPoint's validity handling.
        return boost::shared_ptr<SimIDataPoint>(new Generic_cssDataPoint(
            1.0, flag, flag, flag, flag, flag, flag,
            sample[0], sample[1], sample[2],
            sample[3], sample[4], sample[5]));
    }
}
