//
// Created by brucegoose on 5/26/23.
//

#include "bms.hpp"

#ifdef _BMS_SERIAL_
#include "serial_bms.hpp"
#endif

#include "null_bms.hpp"

namespace infrastructure {

    std::shared_ptr<Bms> Bms::Create(
        const infrastructure::BmsConfig &config, domain::BmsMessageCallback &&post_callback
    ) {
        switch(config.get_bms_type()) {
#ifdef _BMS_SERIAL_
            case BmsType::SERIAL:
                return std::make_shared<SerialBms>(config, std::move(post_callback));
#endif
            case BmsType::NONE:
                return std::make_shared<NullBms>(config, std::move(post_callback));
            default:
                throw std::runtime_error("Selected BMS type unavailable...");
        }
    }

    Bms::Bms(
        const infrastructure::BmsConfig &config, domain::BmsMessageCallback &&post_callback
    ):
        _bms_polling_timeout(config.get_bms_polling_timeout()),
        _bms_shutdown_threshold(config.get_bms_shutdown_threshold()),
        _post_callback(std::move(post_callback))
    {}
}