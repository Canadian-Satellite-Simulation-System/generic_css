
#ifndef NOS3_GENERIC_CSS_HIL_DATA_PROVIDER_HPP
#define NOS3_GENERIC_CSS_HIL_DATA_PROVIDER_HPP

#include <boost/property_tree/ptree.hpp>
#include <boost/shared_ptr.hpp>

#include <sim_i_data_provider.hpp>
#include <generic_css_data_point.hpp>

namespace Nos3
{
    class Generic_cssHilDataProvider : public SimIDataProvider
    {
    public:
        // Constructor
        Generic_cssHilDataProvider(
            const boost::property_tree::ptree& config
        );

        // Return the latest six sensor readings
        boost::shared_ptr<SimIDataPoint>
        get_data_point(void) const;

    private:
        // Prevent copying
        Generic_cssHilDataProvider(
            const Generic_cssHilDataProvider&
        ) = delete;

        Generic_cssHilDataProvider& operator=(
            const Generic_cssHilDataProvider&
        ) = delete;
    };
}

#endif

