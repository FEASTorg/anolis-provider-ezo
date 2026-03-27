#include "core/runtime_state.hpp"

#include <mutex>
#include <sstream>
#include <utility>

namespace anolis_provider_ezo::runtime {
namespace {

std::mutex g_mutex;
RuntimeState g_state;

} // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = RuntimeState{};
}

void initialize(const ProviderConfig &config) {
    RuntimeState state;
    state.config = config;
    state.started_at = std::chrono::system_clock::now();
    state.ready = true;

    std::ostringstream out;
    out << "phase1 skeleton ready: devices=" << state.config.devices.size();
    state.startup_message = out.str();

    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = std::move(state);
}

RuntimeState snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

} // namespace anolis_provider_ezo::runtime
