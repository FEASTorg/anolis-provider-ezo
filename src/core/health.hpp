#pragma once

#include <vector>

#include "core/runtime_state.hpp"
#include "protocol.pb.h"

namespace anolis_provider_ezo::health {

using DeviceHealth = anolis::deviceprovider::v1::DeviceHealth;
using ProviderHealth = anolis::deviceprovider::v1::ProviderHealth;
using WaitReadyResponse = anolis::deviceprovider::v1::WaitReadyResponse;

ProviderHealth make_provider_health(const runtime::RuntimeState &state);
std::vector<DeviceHealth> make_device_health(const runtime::RuntimeState &state,
                                             bool include_excluded = true);
void populate_wait_ready(const runtime::RuntimeState &state, WaitReadyResponse &out);

} // namespace anolis_provider_ezo::health
