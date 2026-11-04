
#include <generic_css_hil_data_provider.hpp>
#include <ItcLogger/Logger.hpp>

namespace Nos3
{
    // Register the provider with NOS3
    REGISTER_DATA_PROVIDER(
        Generic_cssHilDataProvider,
        "GENERIC_CSS_HIL_PROVIDER"
    );

    extern ItcLogger::Logger *sim_logger;

    // Constructor
    Generic_cssHilDataProvider::Generic_cssHilDataProvider(
        const boost::property_tree::ptree& config
    ) : SimIDataProvider(config)
    {
        sim_logger->info(
            "Generic_cssHilDataProvider: Initialized"
        );
    }

    // Return six sensor readings
    boost::shared_ptr<SimIDataPoint>
    Generic_cssHilDataProvider::get_data_point(void) const
    {
        // Temporary hardcoded voltages
        const double css0 = 0.5;
        const double css1 = 1.0;
        const double css2 = 1.5;
        const double css3 = 2.0;
        const double css4 = 2.5;
        const double css5 = 3.0;

        // All six channels are valid
        const int valid = 1;

        // No additional scaling
        const double scale_factor = 1.0;

        // Create a data point using the existing
        // Generic_cssDataPoint constructor
        SimIDataPoint* dp = new Generic_cssDataPoint(
            scale_factor,
            valid, valid, valid,
            valid, valid, valid,
            css0, css1, css2,
            css3, css4, css5
        );

        return boost::shared_ptr<SimIDataPoint>(dp);
    }
}
