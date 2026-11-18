#ifndef NOS3_GENERIC_CSS_SWITCHING_DATA_PROVIDER_HPP
#define NOS3_GENERIC_CSS_SWITCHING_DATA_PROVIDER_HPP

#include <sim_i_data_provider.hpp>
#include <boost/property_tree/ptree.hpp>
#include <atomic>
#include <memory>
#include <thread>

namespace Nos3
{
    class Generic_cssSwitchingDataProvider : public SimIDataProvider
    {
    public:
        explicit Generic_cssSwitchingDataProvider(const boost::property_tree::ptree& config);
        ~Generic_cssSwitchingDataProvider() override;
        boost::shared_ptr<SimIDataPoint> get_data_point(void) const override;

    private:
        Generic_cssSwitchingDataProvider(const Generic_cssSwitchingDataProvider&) = delete;
        Generic_cssSwitchingDataProvider& operator=(const Generic_cssSwitchingDataProvider&) = delete;

        enum Mode { SIM = 0, HIL = 1 };
        void control_loop();
        std::unique_ptr<SimIDataProvider> _sim_provider;
        std::unique_ptr<SimIDataProvider> _hil_provider;
        std::atomic<int> _mode;
        std::atomic<bool> _stop{false};
        const unsigned short _control_port;
        std::thread _control_thread;
    };
}
#endif