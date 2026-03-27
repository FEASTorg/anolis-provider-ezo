#pragma once

#include <chrono>
#include <string>

#include "config/provider_config.hpp"

namespace anolis_provider_ezo::runtime {

struct RuntimeState {
    ProviderConfig config;
    bool ready = false;
    std::string startup_message;
    std::chrono::system_clock::time_point started_at;
};

void reset();
void initialize(const ProviderConfig &config);
RuntimeState snapshot();

} // namespace anolis_provider_ezo::runtime
