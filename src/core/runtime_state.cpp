#include "core/runtime_state.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>

#include "i2c/ezo_i2c_bridge.hpp"
#include "logging/logger.hpp"

extern "C" {
#include "ezo_control.h"
#include "ezo_ph.h"
#include "ezo_product.h"
}

namespace anolis_provider_ezo::runtime {
namespace {

std::mutex g_mutex;
RuntimeState g_state;
std::shared_ptr<i2c::BusExecutor> g_executor;

constexpr int kMinSamplePeriodMs = 50;
constexpr int kMinStaleAfterMs = 500;
constexpr char kSignalPhValue[] = "ph.value";

bool has_prefix(const std::string &value, const std::string &prefix) {
    return value.rfind(prefix, 0) == 0;
}

bool is_mock_mode(const ProviderConfig &config) {
    return has_prefix(config.bus_path, "mock://");
}

int sample_period_ms(const ProviderConfig &config) {
    return std::max(config.query_delay_us / 1000, kMinSamplePeriodMs);
}

int stale_after_ms(const ProviderConfig &config) {
    return std::max(sample_period_ms(config) * 3, kMinStaleAfterMs);
}

ezo_product_id_t expected_product_for_type(EzoDeviceType type) {
    switch(type) {
    case EzoDeviceType::Ph:
        return EZO_PRODUCT_PH;
    case EzoDeviceType::Orp:
        return EZO_PRODUCT_ORP;
    case EzoDeviceType::Ec:
        return EZO_PRODUCT_EC;
    case EzoDeviceType::Do:
        return EZO_PRODUCT_DO;
    case EzoDeviceType::Rtd:
        return EZO_PRODUCT_RTD;
    case EzoDeviceType::Hum:
        return EZO_PRODUCT_HUM;
    }

    return EZO_PRODUCT_UNKNOWN;
}

const char *type_id_for_device(EzoDeviceType type) {
    switch(type) {
    case EzoDeviceType::Ph:
        return "sensor.ezo.ph";
    case EzoDeviceType::Orp:
        return "sensor.ezo.orp";
    case EzoDeviceType::Ec:
        return "sensor.ezo.ec";
    case EzoDeviceType::Do:
        return "sensor.ezo.do";
    case EzoDeviceType::Rtd:
        return "sensor.ezo.rtd";
    case EzoDeviceType::Hum:
        return "sensor.ezo.hum";
    }
    return "sensor.ezo.unknown";
}

i2c::Status make_status(i2c::StatusCode code, const std::string &message) {
    return i2c::Status{code, message};
}

i2c::Status status_from_ezo_result(ezo_result_t result, const std::string &context) {
    if(result == EZO_OK) {
        return i2c::Status::ok();
    }

    switch(result) {
    case EZO_ERR_INVALID_ARGUMENT:
        return make_status(i2c::StatusCode::InvalidArgument,
                           context + ": " + ezo_result_name(result));
    case EZO_ERR_TRANSPORT:
        return make_status(i2c::StatusCode::Unavailable,
                           context + ": " + ezo_result_name(result));
    case EZO_ERR_BUFFER_TOO_SMALL:
    case EZO_ERR_PROTOCOL:
    case EZO_ERR_PARSE:
        return make_status(i2c::StatusCode::Internal,
                           context + ": " + ezo_result_name(result));
    case EZO_OK:
        break;
    }

    return make_status(i2c::StatusCode::Internal,
                       context + ": " + ezo_result_name(result));
}

std::unique_ptr<i2c::ISession> make_session(const ProviderConfig &config) {
    if(is_mock_mode(config)) {
        return std::make_unique<i2c::NoopSession>(config.bus_path);
    }

#if defined(__linux__)
    return std::make_unique<i2c::LinuxSession>(
        config.bus_path,
        config.timeout_ms,
        config.retry_count);
#else
    return std::make_unique<i2c::NoopSession>(config.bus_path);
#endif
}

std::vector<ActiveDevice>::iterator find_active_device_unlocked(
    std::vector<ActiveDevice> &devices,
    const std::string &device_id) {
    return std::find_if(
        devices.begin(),
        devices.end(),
        [&device_id](const ActiveDevice &device) { return device.spec.id == device_id; });
}

anolis::deviceprovider::v1::CapabilitySet build_ph_capabilities(const ProviderConfig &config) {
    anolis::deviceprovider::v1::CapabilitySet capabilities;
    auto *signal = capabilities.add_signals();
    signal->set_signal_id(kSignalPhValue);
    signal->set_name("pH");
    signal->set_description("Latest pH measurement");
    signal->set_value_type(anolis::deviceprovider::v1::VALUE_TYPE_DOUBLE);
    signal->set_unit("pH");
    signal->set_poll_hint_hz(1000.0 / static_cast<double>(sample_period_ms(config)));
    signal->set_stale_after_ms(static_cast<uint32_t>(stale_after_ms(config)));
    return capabilities;
}

anolis::deviceprovider::v1::Device build_descriptor(const ProviderConfig &config,
                                                    const DeviceSpec &spec) {
    anolis::deviceprovider::v1::Device descriptor;
    const std::string formatted_address = format_i2c_address(spec.address);
    descriptor.set_device_id(spec.id);
    descriptor.set_provider_name("anolis-provider-ezo");
    descriptor.set_type_id(type_id_for_device(spec.type));
    descriptor.set_type_version("1");
    descriptor.set_label(spec.label.empty() ? spec.id : spec.label);
    descriptor.set_address(formatted_address);
    (*descriptor.mutable_tags())["hw.bus_path"] = config.bus_path;
    (*descriptor.mutable_tags())["hw.i2c_address"] = formatted_address;
    (*descriptor.mutable_tags())["bus_path"] = config.bus_path;
    (*descriptor.mutable_tags())["i2c_address"] = formatted_address;
    (*descriptor.mutable_tags())["configured_type"] = to_string(spec.type);
    return descriptor;
}

ezo_product_id_t mock_product_for_address(int address) {
    switch(address) {
    case 0x61:
        return EZO_PRODUCT_DO;
    case 0x62:
        return EZO_PRODUCT_ORP;
    case 0x63:
        return EZO_PRODUCT_PH;
    case 0x64:
        return EZO_PRODUCT_EC;
    case 0x66:
        return EZO_PRODUCT_RTD;
    case 0x6F:
        return EZO_PRODUCT_HUM;
    default:
        return EZO_PRODUCT_UNKNOWN;
    }
}

void fill_mock_identity(int address, ezo_device_info_t *info) {
    if(info == nullptr) {
        return;
    }

    std::memset(info, 0, sizeof(*info));
    info->product_id = mock_product_for_address(address);

    const ezo_product_metadata_t *metadata = ezo_product_get_metadata(info->product_id);
    if(metadata != nullptr && metadata->vendor_short_code != nullptr) {
        std::snprintf(
            info->product_code,
            sizeof(info->product_code),
            "%s",
            metadata->vendor_short_code);
    } else {
        std::snprintf(info->product_code, sizeof(info->product_code), "UNK");
    }
    std::snprintf(info->firmware_version, sizeof(info->firmware_version), "mock-1.0");
}

i2c::Status probe_identity_real(i2c::BusExecutor &executor,
                                const ProviderConfig &config,
                                const DeviceSpec &spec,
                                ezo_device_info_t *info_out) {
    if(info_out == nullptr) {
        return make_status(i2c::StatusCode::InvalidArgument, "probe requires output info");
    }

    ezo_device_info_t info{};
    const ezo_product_id_t expected_product = expected_product_for_type(spec.type);
    const int timeout_ms = std::max(config.timeout_ms, 2000);

    const i2c::Status status = executor.submit(
        "startup_probe:" + spec.id,
        std::chrono::milliseconds(timeout_ms),
        [&](i2c::ISession &session) {
            i2c::EzoDeviceBinding binding;
            i2c::Status bind_status =
                i2c::bind_ezo_i2c_device(session, static_cast<uint8_t>(spec.address), binding);
            if(!bind_status.is_ok()) {
                return bind_status;
            }

            ezo_timing_hint_t hint{};
            const ezo_result_t send_result =
                ezo_control_send_info_query_i2c(&binding.device, expected_product, &hint);
            if(send_result != EZO_OK) {
                return status_from_ezo_result(send_result, "send info query");
            }

            if(hint.wait_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(hint.wait_ms));
            }

            const ezo_result_t read_result = ezo_control_read_info_i2c(&binding.device, &info);
            if(read_result != EZO_OK) {
                return status_from_ezo_result(read_result, "read info response");
            }

            return i2c::Status::ok();
        });

    if(status.is_ok()) {
        *info_out = info;
    }
    return status;
}

std::string build_startup_message(const RuntimeState &state) {
    std::ostringstream out;
    out << "phase3 startup complete: active=" << state.active_devices.size()
        << ", excluded=" << state.excluded_devices.size()
        << ", configured=" << state.config.devices.size();
    if(!state.i2c_status_message.empty()) {
        out << ", i2c=" << state.i2c_status_message;
    }
    return out.str();
}

} // namespace

void shutdown() {
    std::shared_ptr<i2c::BusExecutor> executor;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        executor = std::move(g_executor);
        g_state.i2c_executor_running = false;
    }
    if(executor) {
        executor->stop();
    }
}

void reset() {
    shutdown();
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = RuntimeState{};
}

void initialize(const ProviderConfig &config) {
    reset();

    RuntimeState state;
    state.config = config;
    state.started_at = std::chrono::system_clock::now();
    state.ready = false;

    auto executor = std::make_shared<i2c::BusExecutor>(make_session(config));
    const i2c::Status start_status = executor->start();

    state.i2c_executor_running = start_status.is_ok();
    state.i2c_status_message = start_status.message;
    state.i2c_metrics = executor->snapshot_metrics();
    if(!start_status.is_ok()) {
        state.startup_message = "phase3 startup failed to initialize I2C executor: " +
                                start_status.message;
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state = std::move(state);
        return;
    }

    const bool mock_mode = is_mock_mode(config);
    for(const DeviceSpec &spec : config.devices) {
        if(spec.type != EzoDeviceType::Ph) {
            state.excluded_devices.push_back(
                ExcludedDevice{spec, "phase3 supports only devices[].type=ph"});
            continue;
        }

        ezo_device_info_t info{};
        i2c::Status probe_status = i2c::Status::ok();
        if(mock_mode) {
            fill_mock_identity(spec.address, &info);
        } else {
            probe_status = probe_identity_real(*executor, config, spec, &info);
        }

        if(!probe_status.is_ok()) {
            state.excluded_devices.push_back(ExcludedDevice{spec, probe_status.message});
            continue;
        }

        if(info.product_id != EZO_PRODUCT_PH) {
            std::string actual = "unknown";
            if(const ezo_product_metadata_t *meta = ezo_product_get_metadata(info.product_id)) {
                actual = meta->family_name;
            }

            state.excluded_devices.push_back(
                ExcludedDevice{spec, "type mismatch: configured ph, detected " + actual});
            continue;
        }

        ActiveDevice device;
        device.spec = spec;
        device.descriptor = build_descriptor(config, spec);
        device.capabilities = build_ph_capabilities(config);
        device.startup_product_code = info.product_code;
        device.startup_firmware_version = info.firmware_version;
        (*device.descriptor.mutable_tags())["ezo_product_code"] = device.startup_product_code;
        (*device.descriptor.mutable_tags())["ezo_firmware"] = device.startup_firmware_version;
        state.active_devices.push_back(std::move(device));
    }

    state.ready = !state.active_devices.empty();
    if(state.ready && state.i2c_status_message.empty()) {
        state.i2c_status_message = "ok";
    }
    state.startup_message = build_startup_message(state);

    std::vector<std::string> active_ids;
    active_ids.reserve(state.active_devices.size());
    for(const ActiveDevice &device : state.active_devices) {
        active_ids.push_back(device.spec.id);
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state = std::move(state);
        g_executor = std::move(executor);
    }

    for(const std::string &device_id : active_ids) {
        const i2c::Status refresh_status = refresh_ph_sample(device_id);
        if(!refresh_status.is_ok()) {
            logging::warning("startup sample failed for device '" + device_id + "': " +
                             refresh_status.message);
        }
    }
}

RuntimeState snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    RuntimeState copy = g_state;
    if(g_executor) {
        copy.i2c_executor_running = g_executor->is_running();
        copy.i2c_metrics = g_executor->snapshot_metrics();
        if(copy.i2c_metrics.last_error.empty()) {
            if(copy.i2c_status_message.empty()) {
                copy.i2c_status_message = "ok";
            }
        } else {
            copy.i2c_status_message = copy.i2c_metrics.last_error;
        }
    }
    return copy;
}

i2c::Status submit_i2c_job(const std::string &job_name,
                           std::chrono::milliseconds timeout,
                           i2c::BusExecutor::Job job) {
    std::shared_ptr<i2c::BusExecutor> executor;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        executor = g_executor;
    }

    if(!executor) {
        return i2c::Status{
            i2c::StatusCode::Unavailable,
            "i2c executor is not running",
        };
    }

    return executor->submit(job_name, timeout, std::move(job));
}

i2c::Status refresh_ph_sample(const std::string &device_id) {
    if(device_id.empty()) {
        return make_status(i2c::StatusCode::InvalidArgument, "device_id is required");
    }

    ProviderConfig config;
    DeviceSpec spec;
    bool mock_mode = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = find_active_device_unlocked(g_state.active_devices, device_id);
        if(it == g_state.active_devices.end()) {
            return make_status(i2c::StatusCode::NotFound, "unknown device_id");
        }
        config = g_state.config;
        spec = it->spec;
        mock_mode = is_mock_mode(config);
    }

    if(mock_mode) {
        const auto now = std::chrono::system_clock::now();
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = find_active_device_unlocked(g_state.active_devices, device_id);
        if(it == g_state.active_devices.end()) {
            return make_status(i2c::StatusCode::NotFound, "unknown device_id");
        }

        const uint64_t sequence = ++it->sample.sequence;
        const double base = 6.8 + (static_cast<double>(spec.address % 7) * 0.1);
        const double delta = static_cast<double>(sequence % 20) * 0.01;
        it->sample.value = base + delta;
        it->sample.sampled_at = now;
        it->sample.has_sample = true;
        it->sample.last_read_ok = true;
        it->sample.last_error.clear();
        ++it->sample.success_count;
        return i2c::Status::ok();
    }

    double ph_value = 0.0;
    const int timeout_ms = std::max(config.timeout_ms, sample_period_ms(config) + 1500);
    const i2c::Status status = submit_i2c_job(
        "sample:" + device_id,
        std::chrono::milliseconds(timeout_ms),
        [&](i2c::ISession &session) {
            i2c::EzoDeviceBinding binding;
            i2c::Status bind_status =
                i2c::bind_ezo_i2c_device(session, static_cast<uint8_t>(spec.address), binding);
            if(!bind_status.is_ok()) {
                return bind_status;
            }

            ezo_timing_hint_t hint{};
            const ezo_result_t send_result = ezo_ph_send_read_i2c(&binding.device, &hint);
            if(send_result != EZO_OK) {
                return status_from_ezo_result(send_result, "send pH read");
            }

            if(hint.wait_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(hint.wait_ms));
            }

            ezo_ph_reading_t reading{};
            const ezo_result_t read_result = ezo_ph_read_response_i2c(&binding.device, &reading);
            if(read_result != EZO_OK) {
                return status_from_ezo_result(read_result, "read pH response");
            }

            ph_value = reading.ph;
            return i2c::Status::ok();
        });

    const auto now = std::chrono::system_clock::now();
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = find_active_device_unlocked(g_state.active_devices, device_id);
        if(it == g_state.active_devices.end()) {
            return make_status(i2c::StatusCode::NotFound, "unknown device_id");
        }

        if(status.is_ok()) {
            it->sample.value = ph_value;
            it->sample.sampled_at = now;
            it->sample.has_sample = true;
            it->sample.last_read_ok = true;
            it->sample.last_error.clear();
            ++it->sample.success_count;
        } else {
            it->sample.last_read_ok = false;
            it->sample.last_error = status.message;
            ++it->sample.failure_count;
        }
    }

    return status;
}

} // namespace anolis_provider_ezo::runtime
