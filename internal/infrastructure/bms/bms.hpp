//
// Created by brucegoose on 5/26/23.
//

#ifndef INFRASTRUCTURE_BMS_HPP
#define INFRASTRUCTURE_BMS_HPP

#include <memory>
#include <functional>

#include "domain/message.hpp"

namespace infrastructure {

    enum class BmsType {
        SERIAL,
        NONE
    };

    struct BmsConfig {
        [[nodiscard]] virtual BmsType get_bms_type() const = 0;
        [[nodiscard]] virtual int get_bms_polling_timeout() const = 0;
        [[nodiscard]] virtual int get_bms_shutdown_threshold() const = 0;
        [[nodiscard]] virtual int get_bms_read_timeout() const = 0;
    };

    class Bms {
    public:
        [[nodiscard]] static std::shared_ptr<Bms> Create(
            const BmsConfig &config, domain::BmsMessageCallback &&post_callback
        );
        Bms(const BmsConfig &config, domain::BmsMessageCallback &&post_callback);
        virtual void Start() = 0;
        virtual void Stop() = 0;
    protected:
        domain::BmsMessageCallback _post_callback;
        const int _bms_polling_timeout;
        const int _bms_shutdown_threshold;
    };
}

#endif //INFRASTRUCTURE_BMS_HPP
