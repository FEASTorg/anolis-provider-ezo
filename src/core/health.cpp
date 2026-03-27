#include "core/health.hpp"

#include <chrono>
#include <string>

namespace anolis_provider_ezo::health {

ProviderHealth make_provider_health(const runtime::RuntimeState &state) {
    ProviderHealth health;
    health.set_state(state.ready ? ProviderHealth::STATE_OK : ProviderHealth::STATE_DEGRADED);
    health.set_message(state.ready ? "ok" : "not ready");
    (*health.mutable_metrics())["impl"] = "ezo";
    (*health.mutable_metrics())["phase"] = "1";
    (*health.mutable_metrics())["configured_devices"] = std::to_string(state.config.devices.size());
    (*health.mutable_metrics())["bus_path"] = state.config.bus_path;
    return health;
}

std::vector<DeviceHealth> make_device_health(const runtime::RuntimeState &) {
    return {};
}

void populate_wait_ready(const runtime::RuntimeState &state, WaitReadyResponse &out) {
    const auto now = std::chrono::system_clock::now();
    const auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(now - state.started_at).count();

    (*out.mutable_diagnostics())["ready"] = state.ready ? "true" : "false";
    (*out.mutable_diagnostics())["init_time_ms"] = "0";
    (*out.mutable_diagnostics())["uptime_ms"] = std::to_string(uptime);
    (*out.mutable_diagnostics())["configured_device_count"] = std::to_string(state.config.devices.size());
    (*out.mutable_diagnostics())["provider_version"] = ANOLIS_PROVIDER_EZO_VERSION;
    (*out.mutable_diagnostics())["provider_impl"] = "ezo";
}

} // namespace anolis_provider_ezo::health
