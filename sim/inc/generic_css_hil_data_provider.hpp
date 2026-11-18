#ifndef NOS3_GENERIC_CSS_HIL_DATA_PROVIDER_HPP
#define NOS3_GENERIC_CSS_HIL_DATA_PROVIDER_HPP

#include <sim_i_data_provider.hpp>
#include <generic_css_data_point.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/shared_ptr.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace Nos3
{
    class Generic_cssHilDataProvider : public SimIDataProvider
    {
    public:
        explicit Generic_cssHilDataProvider(const boost::property_tree::ptree& config);
        ~Generic_cssHilDataProvider() override;

        boost::shared_ptr<SimIDataPoint> get_data_point(void) const override;

    private:
        Generic_cssHilDataProvider(const Generic_cssHilDataProvider&) = delete;
        Generic_cssHilDataProvider& operator=(const Generic_cssHilDataProvider&) = delete;

        void listen_for_samples();
        bool parse_sample(const std::string& line, std::array<double, 6>& result) const;

        const unsigned short _port;
        const std::chrono::milliseconds _stale_timeout;
        std::atomic<bool> _stop{false};
        std::thread _listener;

        mutable std::mutex _sample_mutex;
        std::array<double, 6> _latest{{0, 0, 0, 0, 0, 0}};
        std::chrono::steady_clock::time_point _last_received{};
        bool _has_sample{false};
    };
}

#endif

